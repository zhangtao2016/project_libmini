//
//  tcp_base.c
//  supex
//
//  Created by 周凯 on 15/12/26.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include "tcp_api/tcp_base.h"
static void _tcp_set_timeout(int fd, void *usr);

/**
 * 连接远端
 */
struct tcp_socket *tcp_connect(const char *ipaddr, const char *port, int timeout)
{
	struct tcp_socket *volatile client = NULL;

	TRY
	{
		New(client);
		AssertError(client, ENOMEM);

		/*initial*/
		client->fd = -1;
		client->miou = TCP_MIOU;
		client->timeout = -1;
		client->type = TCP_TYPE_CONN;

		bool flag = false;
		flag = cache_init(&client->cache);
		AssertRaise(flag, EXCEPT_SYS);
		/*connect remote host*/
		client->fd = TcpConnect(ipaddr, port, _tcp_set_timeout, &timeout);
		RAISE_SYS_ERROR(client->fd);
		/*disable time out on fd*/
		SO_SetSndTimeout(client->fd, 0);
		/*enable nonblock on fd*/
		FD_Enable(client->fd, O_NONBLOCK);

		/*initial information of socket*/
		/*store foreign*/
		struct sockaddr_storage sockaddr = {};
		/*get address of foreign socket*/
		SF_GetRemoteAddr(client->fd, &sockaddr, sizeof(sockaddr));
		/*translate address of local socket*/
		SA_ntop((SA)&sockaddr, client->faddr, sizeof(client->faddr));
		client->fport = SA_GetPort((SA)&sockaddr);
		/*get address of local socket*/
		SF_GetLocalAddr(client->fd, &sockaddr, sizeof(sockaddr));
		/*translate address of local socket*/
		SA_ntop((SA)&sockaddr, client->laddr, sizeof(client->laddr));
		client->lport = SA_GetPort((SA)&sockaddr);

		REFOBJ(client);
	}
	CATCH
	{
		if (likely(client)) {
			cache_finally(&client->cache);

			if (client->fd > -1) {
				close(client->fd);
			}

			Free(client);
		}
	}
	END;

	return client;
}

/**
 * 侦听本地
 */
struct tcp_socket *tcp_listen(const char *ipaddr, const char *port)
{
	struct tcp_socket *volatile server = NULL;

	TRY
	{
		New(server);
		AssertError(server, ENOMEM);

		/*initial*/
		server->fd = -1;
		server->miou = -1;
		server->timeout = -1;
		server->type = TCP_TYPE_LISTEN;

		server->fd = TcpListen(ipaddr, port, NULL, NULL);
		RAISE_SYS_ERROR(server->fd);
		/*enable nonblock on fd*/
		FD_Enable(server->fd, O_NONBLOCK);

		struct sockaddr_storage sockaddr = {};
		/*get address of local socket*/
		SF_GetLocalAddr(server->fd, &sockaddr, sizeof(sockaddr));
		/*translate address of local socket*/
		SA_ntop((SA)&sockaddr, server->laddr, sizeof(server->laddr));
		server->lport = SA_GetPort((SA)&sockaddr);

		REFOBJ(server);
	}
	CATCH
	{
		if (likely(server)) {
			if (server->fd > -1) {
				close(server->fd);
			}

			Free(server);
		}
	}
	END;

	return server;
}

struct tcp_socket *tcp_accept(struct tcp_socket *server)
{
	ASSERTOBJ(server);
	assert(server->type == TCP_TYPE_LISTEN);
	struct tcp_socket *volatile client = NULL;

	TRY
	{
		New(client);
		AssertError(client, ENOMEM);

		/*initial*/
		client->fd = -1;
		client->miou = TCP_MIOU;
		client->timeout = server->timeout;
		client->type = TCP_TYPE_CONN;

		bool flag = false;
		flag = cache_init(&client->cache);
		AssertRaise(flag, EXCEPT_SYS);

		/*store address of foreign socket*/
		struct sockaddr_storage sockaddr = {};
		socklen_t               socklen = sizeof(sockaddr);
		struct timeval          start = {};
		long                    remain = -1;

		remain = server->timeout;

		/*it has been seted in nonblock, record time which be escaped by yourself, it could call accept() many times.*/
		if (remain > 0) {
			gettimeofday(&start, NULL);
		}

		/*else only call accept() one time or until a client has came*/

again:
		client->fd = accept(server->fd, (SA)&sockaddr, &socklen);

		if (unlikely(client->fd < 0)) {
			if (likely((errno == EAGAIN) && (server->timeout != 0))) {
				int flag = 0;
				/*check fd for read*/
				flag = FD_CheckRead(server->fd, remain);

				if (likely(flag == 0)) {
					if (server->timeout > 0) {
						struct timeval  end = {};
						long            diffms = 0;
						/*calculate amount of time which had been escaped*/
						gettimeofday(&end, NULL);
						diffms = (end.tv_sec - start.tv_sec) * 1000;
						diffms += (end.tv_usec - start.tv_usec) / 1000;
						remain -= diffms;
					}

					if (likely((server->timeout < 0) || (remain > 0))) {
						goto again;
					} else {
						errno = ETIMEDOUT;
					}
				}
			}

			RAISE(EXCEPT_SYS);
		}

		/*enable nonblock on fd*/
		FD_Enable(client->fd, O_NONBLOCK);

		/*translate address of foreign socket*/
		SA_ntop((SA)&sockaddr, client->faddr, sizeof(client->faddr));
		client->fport = SA_GetPort((SA)&sockaddr);
		/*get address of local socket*/
		SF_GetLocalAddr(client->fd, &sockaddr, sizeof(sockaddr));
		/*translate address of local socket*/
		SA_ntop((SA)&sockaddr, client->laddr, sizeof(client->laddr));
		client->lport = SA_GetPort((SA)&sockaddr);

		REFOBJ(client);
	}
	CATCH
	{
		if (likely(client)) {
			cache_finally(&client->cache);

			if (client->fd > -1) {
				close(client->fd);
			}

			Free(client);
		}
	}
	END;

	return client;
}

/**以fd初始化tcp对象*/
bool tcp_init(struct tcp_socket *tcp, int fd, bool isclient)
{
	return_val_if_fail(REFOBJ(tcp) || fd < 0, false);
	
	bool volatile ret = false;
	
	TRY
	{
		tcp->fd = fd;
		tcp->timeout = 0;
		tcp->miou = TCP_MIOU;
		tcp->type = TCP_TYPE_LISTEN;
		
		/*store foreign*/
		struct sockaddr_storage sockaddr = {};
		if (isclient) {
			/*get address of foreign socket*/
			SF_GetRemoteAddr(tcp->fd, &sockaddr, sizeof(sockaddr));
			/*translate address of local socket*/
			SA_ntop((SA)&sockaddr, tcp->faddr, sizeof(tcp->faddr));
			tcp->fport = SA_GetPort((SA)&sockaddr);
			tcp->type = TCP_TYPE_CONN;
		}
		
		/*get address of local socket*/
		SF_GetLocalAddr(tcp->fd, &sockaddr, sizeof(sockaddr));
		/*translate address of local socket*/
		SA_ntop((SA)&sockaddr, tcp->laddr, sizeof(tcp->laddr));
		tcp->lport = SA_GetPort((SA)&sockaddr);
		
		ret = true;
	}
	CATCH
	{
		ret = false;
	}
	END;
	
	return true;
}
/**释放tcp对象内部占用的数据内存，但不关闭描述符*/
void tcp_finally(struct tcp_socket *tcp)
{
	return_if_fail(UNREFOBJ(tcp));
	cache_finally(&tcp->cache);
}


/* ------------------     */
static void _tcp_set_timeout(int fd, void *usr)
{
	SO_SetSndTimeout(fd, *(int *)usr);
	/*set keep alive*/
}

