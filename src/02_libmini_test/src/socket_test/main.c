#include "libmini.h"

void SetConnectTimeout(int fd, void *usr)
{
	int to = (int)(intptr_t)usr;

	if (to > 0) {
		SO_SetSndTimeout(fd, to);
	}
}

int main(int argc, char **argv)
{
	int sfd = -1;

	if (argc == 4) {
		/*conect*/
		int to = atoi(argv[3]);

		sfd = TcpConnect(argv[1], argv[2], SetConnectTimeout, (void *)(intptr_t)to);

		if (sfd > 0) {
			x_printf(D, "connect `%s:%s` successful.", argv[1], argv[2]);
		} else {
			x_printf(D, "connect `%s:%s` failed : %s.", argv[1], argv[2], x_strerror(errno));
		}

		usleep(1);
	} else if (argc == 3) {
		/*listen*/

		sfd = TcpListen(argv[1], argv[2], NULL, NULL);

		if (sfd > 0) {
			bool isrd = false;

			x_printf(D, "listen `%s:%s` successful.", argv[1], argv[2]);

			FD_Enable(sfd, O_NONBLOCK);

again:
			isrd = FD_CheckRead(sfd, 1000);

			if (isrd == 0) {
				int clnt = accept(sfd, NULL, NULL);

				if (likely(clnt > -1)) {
					TRY
					{
						char                    ip[IPADDR_MAXSIZE] = {};
						struct sockaddr_storage ss = {};
						SF_GetRemoteAddr(clnt, (SA)&ss, sizeof(ss));

						SA_ntop((SA)&ss, ip, sizeof(ip));

						x_printf(D, "accept a client from `%s:%d`.",
							ip, SA_GetPort((SA)&ss));
					}
					CATCH
					{}
					FINALLY
					{
						close(clnt);
					}
					END;
				} else {
					x_printf(E, "accept failed : %s.", x_strerror(errno));
				}
			} else {
				x_printf(D, "time out ...");
			}

			goto again;
		} else {
			x_printf(D, "listen %s:%s failed : %s.", argv[1], argv[2], x_strerror(errno));
		}
	} else {
		/*help*/
		char prg[32] = {};

		GetProgName(prg, sizeof(prg));

		x_printf(E, "Usage %s <host> <port> to listen, "
			"or Usage %s <host> <port> <#time> to connect.", prg, prg);

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

