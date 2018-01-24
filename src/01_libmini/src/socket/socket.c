//
//  socket.c
//
//
//  Created by 周凯 on 15/9/7.
//
//

#include "socket/socket.h"
#include "socket/socket_opt.h"
#include "io/io_base.h"
#include "slog/slog.h"
#include "except/exception.h"

/*
 * 侦听队列大小
 */
#ifndef LISTENQ
  #define LISTENQ (10240)
#endif

/*非阻塞连接超时（ms）*/
#ifndef CONNTIMEOUT
  #define CONNTIMEOUT (500)
#endif

int TcpListen(const char *host, const char *serv, SetSocketCB cb, void *usr)
{
	struct addrinfo                 *aiptr = NULL;
	struct addrinfo *volatile       ai = NULL;
	volatile int                    fd = -1;

	assert(host);
	assert(serv);

	TRY
	{
		int code = 0;

		ai = SA_GetAddrInfo(host, serv, AI_PASSIVE, AF_UNSPEC, SOCK_STREAM);

		for (aiptr = ai; aiptr != NULL; aiptr = aiptr->ai_next) {
			int flag = -1;

			fd = socket(aiptr->ai_family, aiptr->ai_socktype, aiptr->ai_protocol);

			if (unlikely(fd < 0)) {
				code = errno;
				continue;
			}

			SO_ReuseAddress(fd);

			if (cb) {
				cb(fd, usr);
			}

			flag = bind(fd, aiptr->ai_addr, aiptr->ai_addrlen);

			if (likely(flag == 0)) {
				flag = listen(fd, LISTENQ);
				RAISE_SYS_ERROR(flag);

				break;
			}

			code = errno;
			close(fd);
			fd = -1;
		}

		/*抛出最后的错误*/
		AssertError(aiptr, code);
	}
	CATCH
	{
		x_printf(E, "Attempt to listen at "
			"`%s : %s` failed : %s.",
			host, serv,
			x_strerror(errno));

		if (likely(fd > -1)) {
			close(fd);
			fd = -1;
		}
	}
	FINALLY
	{
		SA_FreeAddrInfo((struct addrinfo **)&ai);
	}
	END;

	return fd;
}

int TcpConnect(const char *host, const char *serv, SetSocketCB cb, void *usr)
{
	struct addrinfo                 *aiptr = NULL;
	struct addrinfo *volatile       ai = NULL;
	volatile int                    fd = -1;

	assert(host);
	assert(serv);

	TRY
	{
		int code = 0;
		ai = SA_GetAddrInfo(host, serv, 0, AF_UNSPEC, SOCK_STREAM);

		for (aiptr = ai; aiptr != NULL; aiptr = aiptr->ai_next) {
			int flag = -1;

			fd = socket(aiptr->ai_family, aiptr->ai_socktype, aiptr->ai_protocol);

			if (unlikely(fd < 0)) {
				code = errno;
				continue;
			}

			if (cb) {
				cb(fd, usr);
			}

			struct timeval  start = {};
			long            remain = 0;

			remain = SO_GetSndTimeout(fd);
			remain = remain == 0 ? CONNTIMEOUT : remain;
			/*自己记录流失时间，并可能连接多次，直到超时或成功连接*/
			gettimeofday(&start, NULL);
again:
			flag = connect(fd, aiptr->ai_addr, aiptr->ai_addrlen);

			if (likely(flag == 0)) {
				break;
			} else {
				code = errno;

				if (likely((code == EINPROGRESS) || (code == EALREADY))) {
					struct timeval  end = {};
					long            diffms = 0;

					gettimeofday(&end, NULL);
					diffms = (end.tv_sec - start.tv_sec) * 1000;
					diffms += (end.tv_usec - start.tv_usec) / 1000;
					remain -= diffms;

					if (likely(remain >= 0)) {
						flag = FD_CheckWrite(fd, remain);
						code = errno;

						if (likely((flag == 0) || (code == EINTR))) {
							goto again;
						}

						if (unlikely(code == EAGAIN)) {
							code = ETIMEDOUT;
						}
					} else {
						code = ETIMEDOUT;
					}
				} else if (likely(code == EISCONN)) {
					/*非阻塞连接，已完成连接*/
					break;
				}
			}

			close(fd);
			fd = -1;
		}

		AssertError(aiptr, code);
	}
	CATCH
	{
		x_printf(E, "Attempt to connect to `%s : %s` failed : %s.",
			host, serv, x_strerror(errno));

		if (likely(fd > -1)) {
			close(fd);
			fd = -1;
		}
	}
	FINALLY
	{
		SA_FreeAddrInfo((struct addrinfo **)&ai);
	}
	END;

	return fd;
}

