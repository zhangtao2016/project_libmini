//
//  test.c
//  libmini
//
//  Created by 周凯 on 15/12/2.
//  Copyright © 2015年 zk. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <sys/syscall.h>

#include "evcoro_scheduler.h"

volatile bool g_test_running = true;

struct client
{
	char    ip[INET_ADDRSTRLEN];
	int     port;
	int     fd;
};

__thread long g_tid = -1;

static inline void inittid()
{
#if defined(SYS_thread_selfid)
	g_tid = syscall(SYS_thread_selfid);
#elif defined(SYS_gettid)
	g_tid = syscall(SYS_gettid);
#endif
}

void sighdl(int signo)
{
	g_test_running = false;
}

void freeclient(struct client *client)
{
	assert(client);

	if (client->fd > -1) {
		printf("\033[1;33m" "`%ld` - connect disconnect: [%d] - [%.*s : %d]" "\033[m" "\n",
			g_tid, client->fd, (int)sizeof(client->ip), client->ip, client->port);
		close(client->fd);
	}

	free(client);
}

void workexit(void *usr)
{
	printf("\033[1;31m" "`%ld` - one task destroy ..." "\033[m" "\n", g_tid);

	free(usr);
}

void work(struct evcoro_scheduler *scheduler, void *arg)
{
	struct sockaddr *addr = arg;

	assert(addr);
	union evcoro_event      event = {};
	struct client           *client = calloc(1, sizeof(*client));
	assert(client);
	int     rc = 0;
	int     flag = 0;
	//	evcoro_cleanup_push(scheduler, workexit, addr);
	evcoro_cleanup_push(scheduler, (evcoro_destroycb)freeclient, client);

	/*增加一个新的连接*/
	client->fd = socket(AF_INET, SOCK_STREAM, 0);

	if (unlikely(client->fd < 0)) {
		evcoro_stop(scheduler);
		goto clean;
	}

	rc = fcntl(client->fd, F_GETFL, &flag);

	if (unlikely(client->fd < 0)) {
		evcoro_stop(scheduler);
		goto clean;
	}

	rc = fcntl(client->fd, F_SETFL, flag | O_NONBLOCK);

	if (unlikely(client->fd < 0)) {
		evcoro_stop(scheduler);
		goto clean;
	}

	/*判断是否连接成功*/
	printf("\033[1;33m" "`%ld` - start connect : [%d]" "\033[m" "\n", g_tid, client->fd);
test_conn:
	rc = connect(client->fd, addr, sizeof(struct sockaddr_in));

	if (likely(rc < 0)) {
		int code = errno;

		if (likely(code == EISCONN)) {
			printf("\033[1;33m" "`%ld` - connection standy : [%d]" "\033[m" "\n", g_tid, client->fd);
		} else if (likely((code == EALREADY) || (code == EINPROGRESS))) {
			bool flag = false;
			printf("\033[1;33m" "`%ld` - connecting ... : [%d]" "\033[m" "\n", g_tid, client->fd);
			/*没有执行协程则休眠一会*/
			evcoro_io_init(&event, client->fd, 5.0);
			flag = evcoro_idleswitch(scheduler, &event, EVCORO_WRITE);

			if (unlikely(!flag)) {
				/*超时，未就绪*/
				printf("\033[1;33m" "`%ld` - connect timed out: [%d]" "\033[m" "\n", g_tid, client->fd);
			} else {
				/*可写，测试时候已连接*/
				goto test_conn;
			}
		} else {
			/*出错*/
			fprintf(stderr, "\033[1;31m" "`%ld` - connect () error : [%d] %s" "\033[m" "\n", g_tid, client->fd, strerror(code));
			evcoro_stop(scheduler);
			goto clean;
		}
	} else {
		printf("\033[1;32m" "`%ld` - new connection : [%d]" "\033[m" "\n", g_tid, client->fd);
	}

	{
		struct sockaddr_in      addr = {};
		socklen_t               addrl = sizeof(addr);
		getsockname(client->fd, (struct sockaddr *)&addr, &addrl);
		inet_ntop(addr.sin_family, &addr.sin_addr, client->ip, sizeof(client->ip));
		client->port = ntohs(addr.sin_port);
	}

	while (1) {
		/*逐个字节读取*/
		ssize_t bytes;
		char    buff;

		bytes = read(client->fd, &buff, sizeof(buff));

		if (unlikely(bytes < 0)) {
			int code = errno;

			if (likely(code == EAGAIN)) {
				bool            flag = false;
				struct timeval  tv = {};
				struct timeval  tv2 = {};
				gettimeofday(&tv, NULL);
				/*没有执行协程则休眠一会*/
				evcoro_io_init(&event, client->fd, 5.50);
				flag = evcoro_idleswitch(scheduler, &event, EVCORO_READ);

				if (unlikely(!flag)) {
					gettimeofday(&tv2, NULL);

					int64_t ss = tv2.tv_sec - tv.tv_sec;
					int64_t us = tv2.tv_usec - tv.tv_usec;

					if (us < 0) {
						ss -= 1;
						us += 1000000;
					}

					/*超时，未就绪*/
					printf("\033[1;33m" "`%ld` - read timed out: [%d] - %03lld.%06lld" "\033[m" "\n", g_tid, client->fd, ss, us);
					goto clean;
				} else {
					gettimeofday(&tv2, NULL);

					int64_t ss = tv2.tv_sec - tv.tv_sec;
					int64_t us = tv2.tv_usec - tv.tv_usec;

					if (us < 0) {
						ss -= 1;
						us += 1000000;
					}

					printf("\033[1;32m" "`%ld` - read standby: [%d] - %03lld.%06lld" "\033[m" "\n", g_tid, client->fd, ss, us);
					continue;
				}
			} else {
				/*出错*/
				fprintf(stderr, "\033[1;31m" "`%ld` - read () error : [%d] %s" "\033[m" "\n", g_tid, client->fd, strerror(code));
				goto clean;
			}
		} else if (unlikely(bytes == 0)) {
			printf("\033[1;33m" "`%ld` - terminal close : [%d]" "\033[m" "\n", g_tid, client->fd);
			goto clean;
		} else {
			printf("\033[1;32m" "`%ld` - read 1 byte : [%d] - %c" "\033[m" "\n", g_tid, client->fd, buff);
		}

		/*快速切出*/
		evcoro_fastswitch(scheduler);
	}

clean:

	evcoro_cleanup_pop(scheduler, true);
	//	evcoro_cleanup_pop(scheduler, true);
	workexit(addr);
}

void idle(struct evcoro_scheduler *scheduler, void *arg)
{
	assert(arg);

	struct timeval tv = {};

	gettimeofday(&tv, NULL);

	if ((evcoro_workings(scheduler) + evcoro_suspendings(scheduler)) < 50) {
		struct sockaddr_in *addr = NULL;
		addr = calloc(1, sizeof(*addr));
		assert(addr);

		memcpy(addr, arg, sizeof(*addr));
		evcoro_cleanup_push(scheduler, workexit, addr);

		if (!evcoro_push(scheduler, work, addr, 0)) {
			evcoro_cleanup_pop(scheduler, true);
		} else {
			evcoro_cleanup_pop(scheduler, false);
		}
	}

	if (evcoro_workings(scheduler) == 0) {
		union evcoro_event event = {};
		/*没有执行协程则休眠一会*/
		evcoro_timer_init(&event, 1.0);
		//		evcoro_idleswitch(scheduler, &event, EVCORO_TIMER);
	}

	fprintf(stderr, "\033[1;31m" "<<< `%ld` - %ld.%06ld : IDLE, tasks [%d]/[%d] >>>" "\033[m" "\n",
		g_tid, tv.tv_sec, tv.tv_usec,
		evcoro_workings(scheduler), 
		evcoro_suspendings(scheduler));

	if (unlikely(!g_test_running)) {
		evcoro_stop(scheduler);
	}
}

void *test(void *args)
{
	struct sockaddr_in      *socket = args;
	struct evcoro_scheduler *scheduler = NULL;

	inittid();

	scheduler = evcoro_create(-1);
	assert(scheduler);

	evcoro_loop(scheduler, idle, socket);

	evcoro_destroy(scheduler, workexit);

	return NULL;
}

int main(int argc, char **argv)
{
	struct sockaddr_in socket = {};

#if defined(__linux__) || defined(__linux)
	pthread_t tid[12];
#else
	pthread_t tid[1];
#endif
	int i = 0;

	if (unlikely(argc != 3)) {
error:
		printf("\033[1;31m" "usage %s <ip> <port>" "\033[m" "\n", argv[0]);
		return EXIT_FAILURE;
	}

	signal(SIGINT, sighdl);

	socket.sin_family = AF_INET;
	socket.sin_port = htons(atoi(argv[2]));
	int rc = inet_pton(AF_INET, argv[1], &socket.sin_addr);

	if (unlikely(rc != 1)) {
		goto error;
	}

	for (i = 0; i < (int)sizeof(tid) / sizeof(tid[0]); i++) {
		pthread_create(&tid[i], NULL, test, &socket);
	}

	for (i = 0; i < (int)sizeof(tid) / sizeof(tid[0]); i++) {
		pthread_join(tid[i], NULL);
	}

	return EXIT_SUCCESS;
}

