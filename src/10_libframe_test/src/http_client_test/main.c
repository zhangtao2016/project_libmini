#include <stdio.h>
#include <unistd.h>
#include "evcoro_scheduler.h"
#include "tcp_api/tcp_io.h"
#include "tcp_api/http_api/http_string.h"
#include "tcp_api/http_api/http_reqresp.h"

static const char * HTTP_REQ_INFO[] = {
	//获取资源
	"GET /index/text HTTP/1.1\r\n"
	"Host:%s:%d\r\n"
	"User-Agent:Firefox/2.14 \r\n"
	"Connection:%s\r\n"
	"Accept: */*\r\n"
	"Accept-Language:zh-cn,zh;q=0.7,en-us,en;q=0.3\r\n"
	"\r\n",
	
	//获取服务器支持的方法
	"OPTIONS * HTTP/1.1\r\n"
	"Host:%s:%d\r\n"
	"User-Agent:Firefox/2.14 \r\n"
	"Connection:%s\r\n"
	"\r\n",
	
	//获取资源的相关信息
	"HEAD /index/html HTTP/1.1\r\n"
	"Host:%s:%d\r\n"
	"User-Agent:Firefox/2.14 \r\n"
	"Connection:keep-alive\r\n"
	"Accept: */*\r\n"
	"Accept-Language:zh-cn,zh;q=0.7,en-us,en;q=0.3\r\n"
	"\r\n",
	
	//删除资源
	"DELETE /index/html HTTP/1.1\r\n"
	"Host:%s:%d\r\n"
	"User-Agent:Firefox/2.14 \r\n"
	"Connection:%s\r\n"
	"Accept: */*\r\n"
	"Accept-Language:zh-cn,zh;q=0.7,en-us,en;q=0.3\r\n"
	"\r\n",
	
	//提交资源
	"POST /index/text HTTP/1.1\r\n"
	"Host:%s:%d\r\n"
	"User-Agent:Firefox/2.14 \r\n"
	"Connection:%s\r\n"
	"Content-Type:text/html\r\n"
	"Content-Length:%d\r\n"
	"\r\n"
};

struct socket_info_t {
	struct http_socket	http;
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
static void repair_stack(void *usr);

static void loop_idle(struct evcoro_scheduler *loop, void *usr);
static void client_send(struct evcoro_scheduler *scheduler, void *usr);
static void client_recv(struct evcoro_scheduler *scheduler, void *usr);

int main(int argc, char* argv[])
{
	if( unlikely(argc != 3) ){
		printf("usage: %s [ip] [port]\n",argv[0]);
		return -1;
	}
	SLogSetLevel(SLOG_E_LEVEL);	
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
	usleep(500 * 1000);
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
		nbytes = cache_get(&sockinfo->http.tcp->cache, buff, sizeof(buff));
		if (likely(nbytes > 0)) {
			nbytes = write(sockinfo->http.tcp->fd, buff, nbytes);
			if (unlikely(nbytes < 0)) {
				int error = errno;
				if (likely(error == EAGAIN)) {
					union evcoro_event event;
					bool flag = false;
					evcoro_io_init(&event, sockinfo->http.tcp->fd, .5);
					flag = evcoro_idleswitch(scheduler, &event, EVCORO_WRITE);
					if (unlikely(!flag)) {
						x_perror("send data failed : %d : timed out.", sockinfo->http.tcp->fd);
						return;
					}
				} else {
					x_perror("send data failed : %d : %s", sockinfo->http.tcp->fd,
							 x_strerror(errno));
					return;
				}
			}
			
			union evcoro_event event;
			evcoro_timer_init(&event, RandReal(0.01, 0.05));
			evcoro_idleswitch(scheduler, &event, EVCORO_TIMER);
		}
		if (unlikely(nbytes < sizeof(buff) ||
					 cache_data_length(&sockinfo->http.tcp->cache) == 0)) {
			x_printf(I, "send data success : %d.", sockinfo->http.tcp->fd);
			break;
		}
	}
#else
	nbytes = http_reqsend(&sockinfo->http, write_idle, scheduler);
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
	
	http_init(&sockinfo->http);
	
	ssize_t nbytes = 0;
	sockinfo->http.tcp->timeout = -1;
	nbytes = http_resprecv(&sockinfo->http, read_idle, scheduler);
	
	TRY
	{
		RAISE_SYS_ERROR(nbytes);
		
		const char *buff = cache_data_address(&sockinfo->http.tcp->cache);
		const char *body = &buff[sockinfo->http.parse.hs.body_offset];
		unsigned body_len = sockinfo->http.parse.hs.body_size;
		int code = sockinfo->http.parse.hs.status_code;
		bool keep_alive = sockinfo->http.parse.hs.keep_alive;
		
		x_printf(I, "receive data success : %d\n"
				 "CODE : %d\n"
				 "KEEP-ALIVE : %s\n"
				 "BODY : %.*s",
				 sockinfo->http.tcp->fd,
				 code,
				 likely(keep_alive) ? "YES" : "NO",
				 (int)(likely(body_len > 0) ? body_len : sizeof("null") - 1),
				 likely(body_len > 0) ? body : "null");
		AssertError(keep_alive, ECONNRESET);
		
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
	
	sockinfo->http.tcp = tcp_connect(info->ipaddr, info->port, 10);
	if (unlikely(!sockinfo->http.tcp)) {
		Free(sockinfo);
		return NULL;
	}
	REFOBJ(sockinfo);
	return sockinfo;
}

static inline void socket_info_destroy(struct socket_info_t *sockinfo)
{
	return_if_fail(UNREFOBJ(sockinfo));
	cache_clean(&sockinfo->http.tcp->cache);
	tcp_destroy(sockinfo->http.tcp);
	Free(sockinfo);
}

static void repair_stack(void *usr)
{
	PopExceptFrame();
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
	int pos = (int)RandInt(0, DIM(HTTP_REQ_INFO) - 1);
	
	http_init(&sockinfo->http);
	ssize_t nbytes = 0;
	
	TRY
	{
		if (unlikely(pos == DIM(HTTP_REQ_INFO) - 1)) {
			nbytes = cache_appendf(&sockinfo->http.tcp->cache,
										 "this is a test information");
			RAISE_SYS_ERROR(nbytes);
			
			nbytes = cache_insertf(&sockinfo->http.tcp->cache,
										 HTTP_REQ_INFO[pos],
										 sockinfo->http.tcp->laddr, sockinfo->http.tcp->lport,
										 likely(RandChance(.85f)) ? "keep-alive" : "close",
										 (int)nbytes);
			RAISE_SYS_ERROR(nbytes);
		} else {
			nbytes = cache_appendf(&sockinfo->http.tcp->cache,
										 HTTP_REQ_INFO[pos],
										 sockinfo->http.tcp->laddr, sockinfo->http.tcp->lport,
										 likely(RandChance(.85f)) ? "keep-alive" : "close");
			RAISE_SYS_ERROR(nbytes);
		}
		
		x_printf(I, "start send data :%d\n%.*s",
				 sockinfo->http.tcp->fd,
				 cache_data_length(&sockinfo->http.tcp->cache),
				 cache_data_address(&sockinfo->http.tcp->cache));
		
		ReturnValue(true);
	}
	CATCH
	{
		
	}
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
