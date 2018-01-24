//
//  main.c
//  supex
//
//  Created by 周凯 on 15/12/29.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include <stdio.h>
#include "tcp_api/tcp_io.h"

void snd(struct tcp_socket *tcp);
void rcv(struct tcp_socket *tcp);
int rcv_parse(struct tcp_socket *tcp, void *usr);

int main(int argc, char **argv)
{
	if (unlikely(argc != 3)) {
		x_perror("usage %s <host> <port>", argv[0]);
		return EXIT_FAILURE;
	}

	Rand();
	SetProgName(argv[0]);

	struct tcp_socket *volatile client = NULL;

	TRY
	{
		client = tcp_connect(argv[1], argv[2], 1000);
		AssertRaise(client, EXCEPT_SYS);

		/*设置操时*/
		client->timeout = 100;
		

		bool flag = true;

		while (likely(flag)) {
#if 0
			/*获取用户按键*/
			printf("Enter 'C/c' to continue ...");
			int enter = Getch(NULL);
			switch (enter)
			{
				case 'c':
				case 'C':
					snd(client);
					break;

				default:
					flag = false;
					break;
			}
			printf("\n");
#else
			snd(client);
			rcv(client);
			long slp = RandInt(100, 1200);
//			usleep((int)slp * 1000);
#endif
		}
	}
	CATCH
	{}
	FINALLY
	{
		tcp_destroy(client);
	}
	END;

	return EXIT_SUCCESS;
}

void snd(struct tcp_socket *tcp)
{
	int     i = 0;
	int     j = 0;
	char    buff[128];

	/*设置发送单元*/
	tcp->miou = 5;
	
	/*产生一些数据*/
	j = (int)RandInt(1, 'z' - 'a' + 1);

	for (i = 0; j-- > 0 && i < DIM(buff); i++) {
		buff[i] = 'a' + j;
	}

	j = (int)RandInt(1, 'Z' - 'A' + 1);

	for (; j-- > 0 && i < DIM(buff); i++) {
		buff[i] = 'A' + j;
	}

	j = (int)RandInt(1, '9' - '0' + 1);

	for (; j-- > 0 && i < DIM(buff); i++) {
		buff[i] = '0' + j;
	}

	i = MIN(DIM(buff) - 1, i);
	buff[i] = '\n';
	i += 1;

	/*推入数据*/
	ssize_t nbytes = 0;
	nbytes = cache_append(&tcp->cache, buff, i);
	RAISE_SYS_ERROR(nbytes);
	printf("\nexpect `%d` bytes : %.*s", nbytes,
		cache_data_length(&tcp->cache),
		cache_data_address(&tcp->cache)
		);
	/*发送数据*/
	nbytes = tcp_send(tcp, NULL, NULL);
	RAISE_SYS_ERROR(nbytes);
	printf("send `%d` bytes.\n", nbytes);

	cache_clean(&tcp->cache);
//	cache_cutmem(&tcp->cache);
}

void rcv(struct tcp_socket *tcp)
{
	tcp->miou = 1;
	ssize_t nbytes = 0;
	nbytes = tcp_recv(tcp, rcv_parse, NULL, NULL);
	RAISE_SYS_ERROR(nbytes);
	cache_clean(&tcp->cache);
}

int rcv_parse(struct tcp_socket *tcp, void *usr)
{
	if (likely(cache_data_length(&tcp->cache) == 1)) {
		return 1;
	}
	return -1;
}
