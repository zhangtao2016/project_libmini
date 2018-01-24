#include <stdio.h>
#include <unistd.h>
#include <ev.h>
#include "tcp_api/redis_api/redis_string.h"
#include "tcp_api/redis_api/redis_reqresp.h"
#include "tcp_api/tcp_base.h"


struct socket_info_t {
	struct redis_socket	redis;
	struct ev_io		watcher;
	bool 		needclose;
	int 		magic;
};

static inline void socket_info_init(struct socket_info_t *sockinfo, struct tcp_socket *tcp);

static inline void socket_info_finally(struct socket_info_t *sockinfo);

static void client_send(struct ev_loop *loop, ev_io *watcher, int event);

static void client_recv(struct ev_loop *loop, ev_io *watcher, int event);

static void server_accept(struct ev_loop *loop, ev_io *watcher, int event);

static bool make_send_data(struct socket_info_t *sockinfo);

static void deal_sigintr(struct ev_loop *loop, ev_signal *sig, int event);

static void deal_sigpipe(struct ev_loop *loop, ev_signal *sig, int event);

int main(int argc, char *argv[])
{
	if (unlikely(argc != 3)) {
		printf("usage: %s [ip] [port]\n", argv[0]);
		return -1;
	}
	SLogSetLevel(SLOG_W_LEVEL);
	struct socket_info_t volatile   *server = NULL;
	struct ev_loop volatile         *loop = NULL;

	TRY
	{
		Rand();
		struct tcp_socket *tcp = NULL;

		New(server);
		AssertError(server, ENOMEM);

#ifdef __LINUX__
		loop = ev_default_loop(EVBACKEND_EPOLL);
#else
		loop = ev_default_loop(EVBACKEND_SELECT);
#endif
		AssertError(loop, ENOMEM);

		tcp = tcp_listen(argv[1], argv[2]);
		AssertRaise(tcp, EXCEPT_SYS);

		socket_info_init((struct socket_info_t *)server, tcp);

		ev_signal       sigint = {};
		ev_signal       sigpipe = {};

		ev_signal_init(&sigint, deal_sigintr, SIGINT);
		ev_signal_init(&sigpipe, deal_sigpipe, SIGPIPE);
		ev_io_init((struct ev_io *)&server->watcher, server_accept,
			server->redis.tcp->fd, EV_READ);

		ev_signal_start((struct ev_loop *)loop, &sigint);
		ev_signal_start((struct ev_loop *)loop, &sigpipe);
		ev_io_start((struct ev_loop *)loop, (struct ev_io *)&server->watcher);

		ev_set_userdata((struct ev_loop *)loop, (void *)server);
		ev_loop((struct ev_loop *)loop, 0);
	}
	CATCH
	{}
	FINALLY
	{
		socket_info_finally((struct socket_info_t *)server);
		ev_loop_destroy((struct ev_loop *)loop);
	}
	END;

	return 0;
}

static void server_accept(struct ev_loop *loop, ev_io *watcher, int event)
{
	struct socket_info_t    *server = watcher->data;
	struct socket_info_t    *client = NULL;
	struct tcp_socket       *tcp = NULL;

	ASSERTOBJ(server);

	tcp = tcp_accept(server->redis.tcp);	// 等待用户连接

	if (unlikely(!tcp)) {
		if (errno != EAGAIN) {
			RAISE(EXCEPT_SYS);
		} else {
			return;
		}
	}

	x_printf(S, "server accept successed : %d", tcp->fd);

	New(client);

	if (unlikely(!client)) {
		tcp_destroy(tcp);
		return;
	}

	socket_info_init(client, tcp);
	redis_init(&client->redis);
	ev_io_init(&client->watcher, client_recv, client->redis.tcp->fd, EV_READ);
	ev_io_start(loop, &client->watcher);
}

static void client_recv(struct ev_loop *loop, ev_io *watcher, int event)
{
	ssize_t                 ret = 0;
	struct socket_info_t    *client = watcher->data;

	ASSERTOBJ(client);
	client->redis.tcp->miou = -1;
	client->redis.tcp->timeout = 0;
	ret = redis_reqrecv(&client->redis, NULL, client);

	if (likely(ret > 0)) {
		x_printf(I, "receive data success : %d", client->redis.tcp->fd);
		bool flag = false;
		flag = make_send_data(client);
		if (unlikely(!flag)) {
			goto error;
		}
	success:
		ev_io_stop(loop, &client->watcher);
		ev_io_init(&client->watcher, client_send, client->redis.tcp->fd, EV_WRITE);
		ev_io_start(loop, &client->watcher);
	} else if (likely(ret == 0)) {
		x_printf(D, "client already closed : %d", client->redis.tcp->fd);
		goto error;
	} else if (likely(errno == EAGAIN)) {
		x_printf(D, "receive data warning : %d, try again ...", client->redis.tcp->fd);
	} else {
		if (likely(errno == EPROTO)) {
			ssize_t bytes = 0;
			redis_init(&client->redis);
			bytes = redis_string_error(&client->redis.tcp->cache,"undefined this command");
			if (likely(bytes > 0)) {
    			goto success;
			}
		}
		x_perror("receive data failure : %d, %s", client->redis.tcp->fd, x_strerror(errno));
		goto error;
	}

	return;
	
error:
	ev_io_stop(loop, &client->watcher);
	socket_info_finally(client);
	return;
}

static void client_send(struct ev_loop *loop, ev_io *watcher, int event)
{
	ssize_t                 ret = 0;
	struct socket_info_t    *client = watcher->data;

	ASSERTOBJ(client);

	client->redis.tcp->miou = -1;
	client->redis.tcp->timeout = 0;
	ret = redis_respsend(&client->redis, NULL, client);
	if (likely(ret > 0)) {
		ev_io_stop(loop, &client->watcher);
		x_printf(I, "send data success : %d", client->redis.tcp->fd);
		redis_init(&client->redis);
		ev_io_init(&client->watcher, client_recv, client->redis.tcp->fd, EV_READ);
		ev_io_start(loop, &client->watcher);
	} else if (likely(ret == 0)) {
		errno = EINVAL;
		RAISE(EXCEPT_SYS);
	} else if (likely(errno == EAGAIN)) {
		x_printf(D, "send data warning : %d, try again ...", client->redis.tcp->fd);
	} else {
		ev_io_stop(loop, &client->watcher);
		socket_info_finally(client);
	}
}

static bool make_send_data(struct socket_info_t *client)
{
	const char 		*buff = cache_data_address(&client->redis.tcp->cache);
	struct redis_presult *rs = &client->redis.parse.rs;
	
	uint64_t command = rs->command_type;
	int i = 0;
	const char *cmdname = redis_command_name(command);
#if 0
	printf(LOG_I_COLOR"command: %s "LOG_COLOR_NULL, likely(cmdname) ? cmdname : "<UNKNOW>");
	
	for (i = 0; i < rs->fields; i++) {
		printf(LOG_I_COLOR"%.*s "LOG_COLOR_NULL, rs->field[i].len, &buff[rs->field[i].offset]);
	}
	printf("\n");
#endif
	redis_init(&client->redis);
	struct redis_string rstring = { .cache = &client->redis.tcp->cache };
	redis_string_init(&rstring);
	ssize_t bytes = 0;
	
	switch (command) {
  		case redis_command_get:
			bytes = redis_string_bulk(rstring.cache, "%ld", RandInt(1, 32));
			return_val_if_fail(bytes > 0, false);
			break;
		case redis_command_set:
			bytes = redis_string_number(rstring.cache, RandInt(0, 1));
			return_val_if_fail(bytes > 0, false);
			break;
		case redis_command_lpush:
			bytes = redis_string_number(rstring.cache, i);
			return_val_if_fail(bytes > 0, false);
			break;
		case redis_command_lrange:
		{
			/*分段添加数据*/
			int j = (int)RandInt(1, 10);
			for (i = 0; i < j; i++) {
				bytes = redis_string_appendf(&rstring, "%ld", RandInt(1, j));
				return_val_if_fail(bytes > 0, false);
			}
			bytes = redis_string_complete(&rstring);
			return_val_if_fail(bytes > 0, false);
		}
			break;
  		default:
			bytes = redis_string_error(rstring.cache,
						"undefined this command %llu", command);
			return_val_if_fail(bytes > 0, false);
			break;
	}
	
	x_printf(I, "start send data : %d\n%.*s", client->redis.tcp->fd,
				cache_data_length(&client->redis.tcp->cache),
				cache_data_address(&client->redis.tcp->cache));
	
	return true;
}

static inline void socket_info_init(struct socket_info_t *sockinfo, struct tcp_socket *tcp)
{
	memset(sockinfo, 0, sizeof(*sockinfo));
	sockinfo->redis.tcp = tcp;
	sockinfo->redis.tcp->timeout = 0;
	sockinfo->watcher.data = sockinfo;
	REFOBJ(sockinfo);
}

static inline void socket_info_finally(struct socket_info_t *server)
{
	return_if_fail(UNREFOBJ(server));
	if (ISOBJ(server->redis.tcp) && ISOBJ(&server->redis.tcp->cache)) {
		cache_clean(&server->redis.tcp->cache);
	}
	tcp_destroy(server->redis.tcp);
	Free(server);
}

static void deal_sigintr(struct ev_loop *loop, ev_signal *sig, int event)
{
	struct socket_info_t *server = ev_userdata(loop);

	ev_break(loop, EVBREAK_ALL);
	ev_io_stop(loop, &server->watcher);
	ev_signal_stop(loop, sig);
	x_perror("receive SIGINT ...");
}

static void deal_sigpipe(struct ev_loop *loop, ev_signal *sig, int event)
{
	x_perror("receive SIGPIPE ...");
}
