//
//  main.c
//  supex
//
//  Created by 周凯 on 15/12/29.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include <stdio.h>
#include "tcp_api/tcp_io.h"

void fork_client(struct tcp_socket *client);

void recv_client(struct tcp_socket *client);

int recv_parse(struct tcp_socket *client, void *usr);

void signal_quitcb(int signo);
void signal_contcb(int signo);
void exit_callback();

int main(int argc, char **argv)
{
	if (unlikely(argc != 2)) {
		x_perror("usage %s <port>.", argv[0]);
		return EXIT_FAILURE;
	}

	struct tcp_socket *volatile server = NULL;

	Signal(SIGCHLD, signal_contcb);
	Signal(SIGINT, signal_contcb);
	Signal(SIGQUIT, signal_quitcb);
	atexit(exit_callback);
	
	TRY
	{
		server = tcp_listen("0.0.0.0", argv[1]);
		AssertRaise(server, EXCEPT_SYS);

		server->timeout = -1;

		while (1) {
			struct tcp_socket *volatile client = NULL;
			client = tcp_accept(server);

			if (likely(client)) {
				fork_client(client);
			} else {
				int code = errno;

				if (likely(code == EAGAIN)) {
					int flag = 0;
					flag = FD_CheckRead(server->fd, -1);
					RAISE_SYS_ERROR(flag);
				} else if (code == ETIMEDOUT || code == EINTR) {
					x_printf(W, "TCP_ACCEPT() : %s.", x_strerror(code));
				} else {
					RAISE(EXCEPT_SYS);
				}
			}
		}
	}
	CATCH
	{}
	FINALLY
	{
		tcp_destroy(server);
	}
	END;

	return EXIT_SUCCESS;
}

void fork_client(struct tcp_socket *client)
{
	pid_t pid = 0;

	TRY
	{
		ForkFrontEndStart(&pid);
//		ForkBackEndStart();
		
//		pid = fork();
//		RAISE_SYS_ERROR(pid);
		
//		if (unlikely(pid == 0)) {
//			InitProcess();
		
			Signal(SIGCHLD, SIG_DFL);
			Signal(SIGINT, SIG_DFL);
			Signal(SIGQUIT, SIG_DFL);
		
			recv_client(client);
			
//			exit(EXIT_SUCCESS);
//		} else {
//			x_printf(S, "new child for deal request of client...");
//		}
		
//		ForkBackEndTerm();
		ForkFrontEndTerm();
		
		x_printf(S, "new child for deal request of client...");
	}
	CATCH
	{}
	FINALLY
	{
		tcp_destroy(client);
	}
	END;
}

void recv_client(struct tcp_socket *client)
{
	ASSERTOBJ(client);
	client->timeout = 1000;
	client->miou = 10;
	
	TRY
	{
		while (1) {
			ssize_t bytes = 0;
			size_t  pos = client->cache.start;
			bytes = tcp_recv(client, recv_parse, NULL, &pos);

			if (likely(bytes > 0)) {
				char buff[16] = {};
				bytes = DIM(buff);

				printf("receive `%d` bytes : ", cache_data_length(&client->cache));
				
				do {
					bytes = cache_get(&client->cache, buff, DIM(buff));

					if (likely(bytes > 0)) {
						printf("%.*s", (int)bytes, buff);
					} else {
						break;
					}
				} while (1);
				
				printf("\n");
			} else if (likely(bytes == 0)) {
				printf(LOG_W_COLOR "close `%d` by remote.\n"LOG_COLOR_NULL, client->fd);
				break;
			} else {
				RAISE(EXCEPT_SYS);
			}
			
			usleep(10 * 1000);
			cache_clean(&client->cache);
			client->cache.end = 1;
			bytes = tcp_send(client, NULL, NULL);
			RAISE_SYS_ERROR(bytes);
			printf("send `%zd` bytes\n", bytes);
		}
	}
	CATCH
	{}
	FINALLY
	{
		tcp_destroy(client);
	}
	END;
}

int recv_parse(struct tcp_socket *client, void *usr)
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

void signal_contcb(int signo)
{
//	exit_callback();
	if (signo == SIGCHLD) {
		ForkWaitProcess(0, false);
	}
}

void signal_quitcb(int signo)
{
	kill(0, SIGINT);
	ForkWaitAllProcess();
	exit(EXIT_FAILURE);
}

void exit_callback()
{
	kill(0, SIGINT);
}
