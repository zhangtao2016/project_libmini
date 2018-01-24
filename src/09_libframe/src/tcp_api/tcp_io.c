//
//  tcp_io.c
//  supex
//
//  Created by 周凯 on 15/12/24.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include "tcp_api/tcp_io.h"

ssize_t tcp_read(struct tcp_socket *tcp)
{
	ASSERTOBJ(tcp);
	AssertError(tcp->type == TCP_TYPE_CONN, ENOTCONN);
	ssize_t nbytes = -1;
	ssize_t miou = -1;
	miou = unlikely(tcp->miou <= 0) ? TCP_MIOU : tcp->miou;
	/*increment `miou` size of space for cache*/
	miou = cache_incrmem(&tcp->cache, (int)miou);
	return_val_if_fail(miou > 0, -1);
	
	nbytes = read(tcp->fd, &tcp->cache.buff[tcp->cache.end], miou);

	if (likely(nbytes > 0)) {
		tcp->cache.end += nbytes;
	}

	return nbytes;
}

ssize_t tcp_write(struct tcp_socket *tcp)
{
	ASSERTOBJ(tcp);
	AssertError(tcp->type == TCP_TYPE_CONN, ENOTCONN);

	ssize_t nbytes = -1;

	nbytes = cache_data_length(&tcp->cache);
	return_val_if_fail(nbytes > 0, 0);

	ssize_t miou = -1;
	miou = unlikely(tcp->miou <= 0) ? TCP_MIOU : tcp->miou;
	miou = MIN(miou, nbytes);

	nbytes = write(tcp->fd, cache_data_address(&tcp->cache), miou);

	if (likely(nbytes > 0)) {
		tcp->cache.start += nbytes;
	}

	return nbytes;
}

ssize_t tcp_send(struct tcp_socket *tcp, tcp_idle_cb idle, void *usr)
{
	ASSERTOBJ(tcp);

	size_t sndbytes = cache_data_length(&tcp->cache);
	return_val_if_fail(sndbytes, 0);

	struct timeval  start = {};
	long            remain = 0;
	ssize_t         n = -1;
	ssize_t         nbytes = 0;

	remain = tcp->timeout;
	/*增大发送单元，开始发送*/
	tcp->miou = (ssize_t)sndbytes;

	if (tcp->timeout > 0) {
		gettimeofday(&start, NULL);
	}

	while (1) {
		/*判断是否超时*/
		if (tcp->timeout > 0) {
			struct timeval  end = {};
			long            diffms = 0;
			gettimeofday(&end, NULL);
			diffms = (end.tv_sec - start.tv_sec) * 1000;
			diffms += (end.tv_usec - start.tv_usec) / 1000;

			remain = tcp->timeout - diffms;

			if (unlikely(remain < 1)) {
				errno = ETIMEDOUT;
				return -1;
			}
		}

		/*写出数据*/
		n = tcp_write(tcp);

		if (likely(n > 0)) {
			nbytes += n;
		} else if (likely(n == 0)) {
			break;
		} else if (likely(errno == EAGAIN)) {
			if (idle) {
				/*如果idle不为空，将控制权临时的交给调用者*/
				if (unlikely(!idle(tcp, usr))) {
					if (unlikely(errno == 0)) {
						errno = ETIMEDOUT;
					}
					return -1;
				}
			} else if (likely(remain != 0)) {
				/*休眠等待io可用*/
				bool flag = false;
				/*如果remain为0，在不可写的情况下仅操作一次就退出*/
				flag = FD_CheckWrite(tcp->fd, remain);
				return_val_if_fail(flag == 0, -1);
			} else {
				return -1;
			}
		} else {
			/*出错*/
//			tcp->timeout -= remain;
			return -1;
		}
	}

	return nbytes;
}

ssize_t tcp_recv(struct tcp_socket *tcp, tcp_parse_cb parse, tcp_idle_cb idle, void *usr)
{
	ASSERTOBJ(tcp);
	struct timeval  start = {};
	long            remain = 0;
	ssize_t         n = -1;
	ssize_t         nbytes = 0;

	remain = tcp->timeout;
	/*减小发送单元，开始接收*/
	if (unlikely(tcp->miou < 1)) {
		tcp->miou = TCP_MIOU;
	}

	if (tcp->timeout > 0) {
		gettimeofday(&start, NULL);
	}

	while (1) {
		/*判断是否超时*/
		if (tcp->timeout > 0) {
			struct timeval  end = {};
			long            diffms = 0;
			gettimeofday(&end, NULL);
			diffms = (end.tv_sec - start.tv_sec) * 1000;
			diffms += (end.tv_usec - start.tv_usec) / 1000;

			remain = tcp->timeout - diffms;

			if (unlikely(remain < 1)) {
				errno = ETIMEDOUT;
				return -1;
			}
		}

		n = tcp_read(tcp);

		if (likely(n > 0)) {
			nbytes += n;

			if (likely(parse)) {
				int flag = 0;
				flag = parse(tcp, usr);

				if (likely(flag > 0)) {
					break;
				} else if (likely(flag == 0)) {
					continue;
				} else {
					/*没有设置错误则视为协议错误*/
					if (unlikely(errno == 0)) {
						errno = EPROTO;
					}
					return -1;
				}
			}
		} else if (likely(n == 0)) {
			nbytes = 0;
			/*对端断开*/
			break;
		} else if (likely(errno == EAGAIN)) {
			if (idle) {
				/*如果idle不为空，将控制权临时的交给调用者*/
				if (unlikely(!idle(tcp, usr))) {
					if (unlikely(errno == 0)) {
						errno = ETIMEDOUT;
					}
					return -1;
				}
			} else if (likely(remain != 0)) {
				/*休眠等待io可用*/
				int flag = false;
				/*如果remain为0，在不可读的情况下仅操作一次就退出*/
				flag = FD_CheckRead(tcp->fd, remain);
				return_val_if_fail(flag == 0, -1);
			} else {
				return -1;
			}
		} else {
			/*出错*/
//			tcp->timeout -= remain;
			return -1;
		}
	}

	return nbytes;
}

