#include <stdio.h>
#include <unistd.h>
#include <ev.h>
#include "tcp_api/http_api/http_string.h"
#include "tcp_api/http_api/http_reqresp.h"
#include "tcp_api/tcp_base.h"


struct socket_info_t {
	struct http_socket	http;
	struct ev_io		watcher;
	bool 		needclose;
	int 		magic;
};

static inline void socket_info_init(struct socket_info_t *sockinfo, struct tcp_socket *tcp);

static inline void socket_info_finally(struct socket_info_t *sockinfo);

static void client_send(struct ev_loop *loop, ev_io *watcher, int event);

static void client_recv(struct ev_loop *loop, ev_io *watcher, int event);

static void server_accept(struct ev_loop *loop, ev_io *watcher, int event);

static void make_send_data(struct socket_info_t *sockinfo);

static void deal_sigintr(struct ev_loop *loop, ev_signal *sig, int event);

static void deal_sigpipe(struct ev_loop *loop, ev_signal *sig, int event);

int main(int argc, char *argv[])
{
	if (unlikely(argc != 3)) {
		printf("usage: %s [ip] [port]\n", argv[0]);
		return -1;
	}
	SLogSetLevel(SLOG_I_LEVEL);
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
			server->http.tcp->fd, EV_READ);

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

	tcp = tcp_accept(server->http.tcp);	// 等待用户连接

	if (unlikely(!tcp)) {
		if (errno != EAGAIN) {
			RAISE(EXCEPT_SYS);
		} else {
			return;
		}
	}

	x_printf(D, "server accept successed");

	New(client);

	if (unlikely(!client)) {
		tcp_destroy(tcp);
		return;
	}

	socket_info_init(client, tcp);
	http_init(&client->http);
	ev_io_init(&client->watcher, client_recv, client->http.tcp->fd, EV_READ);
	ev_io_start(loop, &client->watcher);
}

static void client_recv(struct ev_loop *loop, ev_io *watcher, int event)
{
	ssize_t                 ret = 0;
	struct socket_info_t    *client = watcher->data;

	ASSERTOBJ(client);
	client->http.tcp->miou = -1;
	client->http.tcp->timeout = 0;
	ret = http_reqrecv(&client->http, NULL, client);

	if (likely(ret > 0)) {
		x_printf(I, "receive data success : %d", client->http.tcp->fd);
		ev_io_stop(loop, &client->watcher);
		make_send_data(client);
		ev_io_init(&client->watcher, client_send, client->http.tcp->fd, EV_WRITE);
		ev_io_start(loop, &client->watcher);
	} else if (likely(ret == 0)) {
		x_printf(D, "client already closed : %d", client->http.tcp->fd);
		ev_io_stop(loop, &client->watcher);
		socket_info_finally(client);
	} else if (likely(errno == EAGAIN)) {
		x_printf(D, "receive data warning : %d, try again ...",
				 client->http.tcp->fd);
	} else {
		x_perror("receive data failure : %d, %s", client->http.tcp->fd, x_strerror(errno));
		ev_io_stop(loop, &client->watcher);
		socket_info_finally(client);
	}
}

static void client_send(struct ev_loop *loop, ev_io *watcher, int event)
{
	ssize_t                 ret = 0;
	struct socket_info_t    *client = watcher->data;

	ASSERTOBJ(client);

	client->http.tcp->miou = -1;
	client->http.tcp->timeout = 0;
	ret = http_respsend(&client->http, NULL, client);
	if (likely(ret > 0)) {
		ev_io_stop(loop, &client->watcher);
		if (unlikely(client->needclose)) {
			x_printf(W, "send data warning : need close connection.");
			socket_info_finally(client);
		} else {
			x_printf(I, "send data success : %d", client->http.tcp->fd);
			http_init(&client->http);
			ev_io_init(&client->watcher, client_recv, client->http.tcp->fd, EV_READ);
			ev_io_start(loop, &client->watcher);
		}
	} else if (likely(ret == 0)) {
		errno = EINVAL;
		RAISE(EXCEPT_SYS);
	} else if (likely(errno == EAGAIN)) {
		x_printf(D, "send data warning : %d, try again ...", client->http.tcp->fd);
	} else {
		ev_io_stop(loop, &client->watcher);
		socket_info_finally(client);
	}
}

static void make_send_data(struct socket_info_t *client)
{
	const char 		*http_buff = cache_data_address(&client->http.tcp->cache);
	unsigned        url_len = client->http.parse.hs.url_len;
	const char		*url = &http_buff[client->http.parse.hs.url_offset];
	unsigned		body_len = client->http.parse.hs.body_size;
	const char		*body = &http_buff[client->http.parse.hs.body_offset];
	const char      *method = http_method_str(client->http.parse.hs.method);
	bool			keep_alive = client->http.parse.hs.keep_alive;
	static const int return_codes[] = { 200, 400, 200, 413, 200, 500, 200, 503, 200 };
	
	x_printf(I, "METHOD : %s\n"
			 "KEEP-ALIVE : %s\n"
			 "URL : %.*s\n"
			 "BODY : %.*s",
			 method,
			 keep_alive ? "YES" : "NO",
			 (int)url_len, url,
			 (int)(likely(body_len > 0) ? body_len : sizeof("null") - 1),
			 likely(body_len > 0) ? body : "null");
	
	int code = (int)RandInt(0, 1000);
	if (likely(return_codes[code % DIM(return_codes)] == 200)) {
		if (keep_alive) {
			client->needclose = RandChance(.8f);
			client->needclose = false; 
		} else {
			client->needclose = true;
		}
	} else {
		client->needclose = true;
	}
	http_init(&client->http);
#if 0
	http_string_resp(&client->http.tcp->cache, return_codes[code % DIM(return_codes)], !client->needclose);
#else
	client->needclose = RandChance(.01f);
	http_string_resp(&client->http.tcp->cache, 200, !client->needclose);
#endif
	x_printf(I, "start send data : %d\n%.*s", client->http.tcp->fd,
				cache_data_length(&client->http.tcp->cache),
				cache_data_address(&client->http.tcp->cache));
}

static inline void socket_info_init(struct socket_info_t *sockinfo, struct tcp_socket *tcp)
{
	memset(sockinfo, 0, sizeof(*sockinfo));
	sockinfo->http.tcp = tcp;
	sockinfo->http.tcp->timeout = 0;
	sockinfo->watcher.data = sockinfo;
	REFOBJ(sockinfo);
}

static inline void socket_info_finally(struct socket_info_t *server)
{
	return_if_fail(UNREFOBJ(server));
	if (ISOBJ(server->http.tcp) && ISOBJ(&server->http.tcp->cache)) {
		cache_clean(&server->http.tcp->cache);
	}
	tcp_destroy(server->http.tcp);
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
