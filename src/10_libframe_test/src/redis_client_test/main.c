#include <stdio.h>
#include <unistd.h>
#include "evcoro_scheduler.h"
#include "tcp_api/tcp_io.h"
#include "tcp_api/redis_api/redis_string.h"
#include "tcp_api/redis_api/redis_reqresp.h"

static const char * REDIS_REQ_INFO[] = {
	"set field %d",
	"get field",
	"lpush list 1 2 3 4 5 %d",
	"lrange list 0 %d",
//	"mischief",
};

struct socket_info_t {
	struct redis_socket	redis;
	bool 		needclose;
	int 		magic;
};

struct conn_info_t {
	const char *ipaddr;
	const char *port;
};

static inline struct socket_info_t *socket_info_create(struct conn_info_t *info);

static inline void socket_info_destroy(struct socket_info_t *sockinfo);

static bool make_send_data(struct socket_info_t *sockinfo);

static bool read_idle(struct tcp_socket *tcp, void *usr);
static bool write_idle(struct tcp_socket *tcp, void *usr);

static void deal_sigintr(struct ev_loop *loop, ev_signal *watcher, int event);
static void deal_sigpipe(struct ev_loop *loop, ev_signal *watcher, int event);
static void sigquit(int signo);

static void loop_idle(struct evcoro_scheduler *loop, void *usr);
static void client_send(struct evcoro_scheduler *scheduler, void *usr);
static void client_recv(struct evcoro_scheduler *scheduler, void *usr);

int main(int argc, char* argv[])
{
	if( unlikely(argc != 3) ){
		printf("usage: %s [ip] [port]\n",argv[0]);
		return -1;
	}
	
	Signal(SIGQUIT, sigquit);
	
	SLogSetLevel(SLOG_W_LEVEL);
	struct evcoro_scheduler *volatile scheduler = NULL;
	struct conn_info_t conn_info = { argv[1], argv[2] };
	TRY
	{
		scheduler = evcoro_create(5000);
		AssertError(scheduler, ENOMEM);
		
		ev_signal sigintr = { .data = scheduler };
		ev_signal sigpipe = { .data = scheduler };
		
		ev_signal_init(&sigintr, deal_sigintr, SIGINT);
		ev_signal_init(&sigpipe, deal_sigpipe, SIGPIPE);
		ev_signal_start(evcoro_get_evlistener(scheduler), &sigintr);
		ev_signal_start(evcoro_get_evlistener(scheduler), &sigpipe);
		
		evcoro_loop(scheduler, loop_idle, &conn_info);
	}
	CATCH
	{}
	FINALLY
	{
		evcoro_destroy(scheduler, (evcoro_destroycb)socket_info_destroy);
	}
	END;
	

	return 0;
}

static void loop_idle(struct evcoro_scheduler *loop, void *usr)
{
	struct conn_info_t *info = usr;
	assert(info);
	
	if (evcoro_workings(loop) + evcoro_suspendings(loop) < 100) {
		struct socket_info_t *sockinfo = socket_info_create(info);
		if (likely(sockinfo)) {
			bool flag = false;
			flag = evcoro_push(loop, client_send, sockinfo, 0);
			if (unlikely(!flag)) {
				socket_info_destroy(sockinfo);
				x_printf(W, "add new task failed ...");
			} else {
				x_printf(D, "add new task success ...");
			}
		} else {
			x_perror("create a new connection error : %s.", x_strerror(errno));
			if (unlikely(errno != ENOMEM)) {
				evcoro_stop(loop);
			}
		}
	}
	
	static struct timeval prec = { };
	struct timeval cur = { };
	if (unlikely(prec.tv_sec == 0)) {
		gettimeofday(&prec, NULL);
		x_printf(S, "start calculate time ...");
	} else {
		gettimeofday(&cur, NULL);
		struct timeval diff = { };
		diff.tv_sec = cur.tv_sec - prec.tv_sec;
		diff.tv_usec = cur.tv_usec - prec.tv_usec;
		if (diff.tv_usec < 0) {
			diff.tv_sec -= 1;
			diff.tv_usec += 1000000;
		}
		x_printf(S, "escape time : %03ld.%06d, %u/%u",
				 diff.tv_sec, (int)diff.tv_usec,
				 evcoro_workings(loop), evcoro_suspendings(loop));
		prec.tv_sec = cur.tv_sec;
		prec.tv_usec = cur.tv_usec;
	}
#if 0
	usleep(500 * 1000);
#endif
}

static void client_send(struct evcoro_scheduler *scheduler, void *usr)
{
	struct socket_info_t *sockinfo = usr;
	ASSERTOBJ(sockinfo);
	
	evcoro_cleanup_push(scheduler, (evcoro_destroycb)socket_info_destroy, sockinfo);
	
	bool flag = false;
	flag = make_send_data(sockinfo);
	return_if_fail(flag);
	
	ssize_t nbytes = 0;
#if 0
	char buff[16] = { };
	while (1) {
		nbytes = cache_get(&sockinfo->redis.tcp->cache, buff, sizeof(buff));
		if (likely(nbytes > 0)) {
			nbytes = write(sockinfo->redis.tcp->fd, buff, nbytes);
			if (unlikely(nbytes < 0)) {
				int error = errno;
				if (likely(error == EAGAIN)) {
					union evcoro_event event;
					bool flag = false;
					evcoro_io_init(&event, sockinfo->redis.tcp->fd, .5);
					flag = evcoro_idleswitch(scheduler, &event, EVCORO_WRITE);
					if (unlikely(!flag)) {
						x_perror("send data failed : %d : timed out.", sockinfo->redis.tcp->fd);
						return;
					}
				} else {
					x_perror("send data failed : %d : %s", sockinfo->redis.tcp->fd,
							 x_strerror(errno));
					return;
				}
			}
			
			union evcoro_event event;
			evcoro_timer_init(&event, RandReal(0.01, 0.05));
			evcoro_idleswitch(scheduler, &event, EVCORO_TIMER);
		}
		if (unlikely(nbytes < sizeof(buff) ||
					 cache_data_length(&sockinfo->redis.tcp->cache) == 0)) {
			x_printf(I, "send data success : %d.", sockinfo->redis.tcp->fd);
			break;
		}
	}
#else
	nbytes = redis_reqsend(&sockinfo->redis, write_idle, scheduler);
#endif
	
	TRY
	{
		RAISE_SYS_ERROR(nbytes);
		
		bool flag = false;
		flag = evcoro_push(scheduler, client_recv, sockinfo, 0);
		AssertError(flag, ENOMEM);
		
		evcoro_cleanup_pop(scheduler, false);
	}
	CATCH
	{
		evcoro_cleanup_pop(scheduler, true);
	}
	END;
}

static void client_recv(struct evcoro_scheduler *scheduler, void *usr)
{
	struct socket_info_t *sockinfo = usr;
	ASSERTOBJ(sockinfo);
	
	evcoro_cleanup_push(scheduler, (evcoro_destroycb)socket_info_destroy, sockinfo);
	
	redis_init(&sockinfo->redis);
	
	ssize_t nbytes = 0;
	sockinfo->redis.tcp->timeout = -1;
	nbytes = redis_resprecv(&sockinfo->redis, read_idle, scheduler);
	
	TRY
	{
		RAISE_SYS_ERROR(nbytes);
		
		/* --------------------- */
#if 0
		printf(LOG_I_COLOR"===============\n"LOG_COLOR_NULL);
		const char *buff = cache_data_address(&sockinfo->redis.tcp->cache);
		struct redis_presult *rs = &sockinfo->redis.parse.rs;
		if (rs->fields < 0) {
			printf(LOG_I_COLOR"(null)\n"LOG_COLOR_NULL);
		} else {
			int i = 0;
			for (i = 0; i < rs->fields; i++) {
				printf(LOG_I_COLOR"%.*s\n"LOG_COLOR_NULL,
					   rs->field[i].len < 0 ? ((int)sizeof("(null)") - 1) : rs->field[i].len,
					   rs->field[i].len < 0 ? "(null)" : &buff[rs->field[i].offset]);
			}
		}
		printf(LOG_I_COLOR"===============\n"LOG_COLOR_NULL);
#endif
		/* --------------------- */
		bool flag = false;
		flag = evcoro_push(scheduler, client_send, sockinfo, 0);
		AssertError(flag, ENOMEM);
		
		evcoro_cleanup_pop(scheduler, false);
	}
	CATCH
	{
		evcoro_cleanup_pop(scheduler, true);
	}
	END;
}

static inline struct socket_info_t *socket_info_create(struct conn_info_t *info)
{
	struct socket_info_t *sockinfo = NULL;
	New(sockinfo);
	return_val_if_fail(sockinfo, NULL);
	
	sockinfo->redis.tcp = tcp_connect(info->ipaddr, info->port, 10);
	if (unlikely(!sockinfo->redis.tcp)) {
		Free(sockinfo);
		return NULL;
	}
	REFOBJ(sockinfo);
	return sockinfo;
}

static inline void socket_info_destroy(struct socket_info_t *sockinfo)
{
	return_if_fail(UNREFOBJ(sockinfo));
	cache_clean(&sockinfo->redis.tcp->cache);
	tcp_destroy(sockinfo->redis.tcp);
	Free(sockinfo);
}

static bool read_idle(struct tcp_socket *tcp, void *usr)
{
	struct evcoro_scheduler *scheduler = usr;
	
	union evcoro_event event;
	evcoro_io_init(&event, tcp->fd, RandReal(.1f, 5.0f));
	evcoro_io_init(&event, tcp->fd, 5.0f);
	bool flag = evcoro_idleswitch(scheduler, &event, EVCORO_READ);
	if (unlikely(!flag)) {
		errno = ETIMEDOUT;
	}
	
	return flag;
}

static bool write_idle(struct tcp_socket *tcp, void *usr)
{
	struct evcoro_scheduler *scheduler = usr;
	
	union evcoro_event event;
	evcoro_io_init(&event, tcp->fd, RandReal(.1f, 5.0f));
	evcoro_io_init(&event, tcp->fd, 5.0f);
	bool flag = evcoro_idleswitch(scheduler, &event, EVCORO_WRITE);
	if (unlikely(!flag)) {
		errno = ETIMEDOUT;
	}
	
	return flag;
}

static bool make_send_data(struct socket_info_t *sockinfo)
{
	int pos = (int)RandInt(0, DIM(REDIS_REQ_INFO) - 1);
//	pos = 1;
	redis_init(&sockinfo->redis);
	ssize_t nbytes = 0;
	
	TRY
	{
		nbytes = redis_stringf(&sockinfo->redis.tcp->cache,
							   REDIS_REQ_INFO[pos], RandInt(1, 1024));
		RAISE_SYS_ERROR(nbytes);
		
		x_printf(I, "start send data :%d\n%.*s",
				 sockinfo->redis.tcp->fd,
				 cache_data_length(&sockinfo->redis.tcp->cache),
				 cache_data_address(&sockinfo->redis.tcp->cache));
		
		ReturnValue(true);
	}
	CATCH
	{}
	END;
	
	return false;
}

static void deal_sigintr(struct ev_loop *loop, ev_signal *sig, int event)
{
	struct evcoro_scheduler *scheduler = sig->data;
	
	ev_break(loop, EVBREAK_ALL);
	ev_signal_stop(loop, sig);
	evcoro_stop(scheduler);
	x_perror("receive SIGINT ...");
}

static void deal_sigpipe(struct ev_loop *loop, ev_signal *sig, int event)
{
	x_perror("receive SIGPIPE ...");
}

static void sigquit(int signo)
{
	x_perror("receive SIGQUIT ...");
	exit(EXIT_FAILURE);
}
