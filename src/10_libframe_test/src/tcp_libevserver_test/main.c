#include <stdio.h>
#include "tcp_api/tcp_io.h"
#include "../libev/ev.h"

static void _recv(struct ev_loop* loop, struct ev_io* watcher, int event);

static void _accept(struct ev_loop* loop, struct ev_io* watcher, int event);

static int recv_parse(struct tcp_socket *client, void *usr);

int main(int argc, char **argv)
{
	if (unlikely(argc != 2)) {
		x_perror("usage %s <port>.", argv[0]);
		return EXIT_FAILURE;
	}

	struct tcp_socket *volatile server = NULL;
	struct ev_loop *volatile    loop = NULL;

	TRY
	{
		struct ev_io io_w = {};
		server = tcp_listen("0.0.0.0", argv[1]);
		ASSERTOBJ(server);

		loop = ev_default_loop(EVBACKEND_EPOLL);
		AssertError(loop, ENOMEM);
		
		ev_io_init(&io_w, _accept, server->fd, EV_READ);
		io_w.data = server;
		ev_io_start((struct ev_loop*)loop, &io_w);

		ev_loop((struct ev_loop*)loop, 0);
	}
	CATCH
	{
		ev_break((struct ev_loop*)loop, EVBREAK_ALL);
	}
	FINALLY
	{
		tcp_destroy((struct tcp_socket*)server);
		ev_loop_destroy((struct ev_loop*)loop);
	}
	END;

	return EXIT_SUCCESS;
}

static void  _accept(struct ev_loop *loop, ev_io* watcher, int event)
{
	struct tcp_socket *volatile client = NULL;
	struct tcp_socket *server = watcher->data;
	struct ev_io *io_w = NULL;

	New(io_w);
	AssertError(io_w, ENOMEM);

	ASSERTOBJ(server);
	client = tcp_accept(server);

	if (likely(client)) {
		x_printf(D, "accept successed");
		ev_io_init(io_w, _recv, client->fd, EV_READ);
		io_w->data = client;
		ev_io_start(loop, io_w);
	} else {
		x_printf(D, "accept failed");
		int code = errno;
		if (likely(code == EAGAIN)) {
			int flag = 0;
			flag = FD_CheckRead(server->fd, -1);
			RAISE_SYS_ERROR(flag);
		} else if (code == ETIMEDOUT) {
			x_printf(W, "TCP_ACCEPT() : %s.", x_strerror(code));
		} else {
			RAISE(EXCEPT_SYS);
		}
	}
}


static void _recv(struct ev_loop* loop, struct ev_io* watcher, int event)
{
	struct tcp_socket * client = watcher->data;
	ASSERTOBJ(client);
	client->timeout = 1000;

	ssize_t bytes = 0;
	size_t  pos = client->cache.start;
	bytes = tcp_recv(client, recv_parse, NULL, &pos);

	if (likely(bytes > 0)) {
		char buff[16] = {};
		bytes = DIM(buff);

		printf("receive `%d` bytes : ", (int)(client->cache.end - client->cache.start));
		do {
			bytes = cache_get(&client->cache, buff, DIM(buff));

			if (likely(bytes > 0)) {
				printf("%.*s", (int)bytes, buff);
			}
		} while (unlikely(bytes >= DIM(buff)));
		printf("\n");
		cache_movemem(&client->cache);
		client->cache.end = 0;

		ev_io_stop(loop, watcher);
		ev_io_init(watcher, _recv, client->fd, EV_READ);
		ev_io_start(loop, watcher);

	} else if (likely(bytes == 0)) {
		x_printf(D, LOG_W_COLOR "close `%d` by remote.\n"LOG_COLOR_NULL, client->fd);
		ev_io_stop(loop, watcher);
		tcp_destroy(client);
		Free(watcher);
	} else {
		x_printf(D, "receive data failed\n");
		ev_io_stop(loop, watcher);
		tcp_destroy(client);
		Free(watcher);
	}
}

static int recv_parse(struct tcp_socket *client, void *usr)
{
	size_t *pos = usr;

	const char *chr = NULL;

	chr = x_strchr(client->cache.buff + *pos, client->cache.end - *pos, '\n');
	*pos = client->cache.end;

	if (chr == NULL) {
		return 0;
	}
	return 1;
}

