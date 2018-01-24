#include "libmini.h"

#define POOLS (10)

AO_T counter = 0;

static void setopt(int fd, void *usr);

void *work(void *usr);

pthread_t *create_pool(int cnt, void *(*work)(void *), void *usr);

void join_pool(pthread_t *pool, int cnt);

int main(int argc, char **argv)
{
	volatile int            server = -1;
	pthread_t *volatile     pool = NULL;
	volatile int            pfd[2] = { -1, -1 };

	if (unlikely(argc != 3)) {
		x_printf(E, "Usage %s <host> <service>", argv[0]);
		return EXIT_FAILURE;
	}

	Rand();
	Signal(SIGPIPE, SignalNormalCB);

	TRY
	{
		int rc = 0;

		server = TcpListen(argv[1], argv[2], setopt, NULL);
		RAISE_SYS_ERROR(server);

		rc = pipe((int *)&pfd[0]);
		RAISE_SYS_ERROR(rc);

		pool = create_pool(POOLS, work, (void *)(intptr_t)pfd[0]);

		while (1) {
			volatile int            client = 0;
			struct sockaddr_storage addr = {};
			socklen_t               len = sizeof(addr);
			char                    ipaddr[IPADDR_MAXSIZE] = {};

			TRY
			{
				ssize_t bytes = 0;

				client = accept(server, (SA)&addr, &len);

				if (unlikely((client < 0) || (errno == EMFILE))) {
					sleep(5);
				} else {
					RAISE_SYS_ERROR(client);
					x_printf(D, "NEW CONNECTION, ` %s : %d `",
						SA_ntop((SA)&addr, ipaddr, sizeof(ipaddr)),
						SA_GetPort((SA)&addr));

					bytes = write(pfd[1], (void *)&client, sizeof(client));
					RAISE_SYS_ERROR(bytes);
					x_printf(I, "Number of connection `%ld`", ATOMIC_ADD_F(&counter, 1));
				}
			}
			CATCH
			{
				if (likely(client > -1)) {
					close(client);
				}
			}
			END;
		}
	}
	CATCH
	{}
	FINALLY
	{
		close(server);
		close(pfd[1]);
		close(pfd[0]);
		join_pool(pool, POOLS);
	}
	END;

	return EXIT_SUCCESS;
}

static void setopt(int fd, void *usr)
{
	//        SO_SetCloseLinger(fd, true, 0);
}

void *work(void *usr)
{
	int     pfd = (int)(intptr_t)usr;
	char    buff[32] = {};

	x_printf(I, "work start ...");

	TRY
	{
		while (1) {
			int volatile    client = -1;
			ssize_t         bytes = 0;

			bytes = read(pfd, (int *)&client, sizeof(client));
			RAISE_SYS_ERROR(bytes);

			struct sockaddr_storage addr = {};
			char                    ipaddr[IPADDR_MAXSIZE] = {};

			TRY
			{
				SF_GetRemoteAddr(client, &addr, sizeof(addr));

#if 0
				FD_Enable(client, O_NONBLOCK);
				bytes = read(client, buff, sizeof(buff));
				RAISE_SYS_ERROR(bytes);

				bytes = write(client, &buff[0], 1);
				RAISE_SYS_ERROR(bytes);
#else
				int i = 0;

				for (i = 0; i < DIM(buff); ++i) {
					/* code */
					bytes = write(client, &buff[0], 1);
					RAISE_SYS_ERROR(bytes);

					//					sleep((int)RandInt(0, 2));
				}
#endif
			}
			CATCH
			{}
			FINALLY
			{
				x_printf(D, "DISCONNECTION, ` %s : %d `",
					SA_ntop((SA)&addr, ipaddr, sizeof(ipaddr)),
					SA_GetPort((SA)&addr));
				ATOMIC_DEC(&counter);
				close(client);
			}
			END;
		}
	}
	CATCH
	{}
	END;

	x_printf(I, "work end ...");

	return NULL;
}

pthread_t *create_pool(int cnt, void *(*work)(void *), void *usr)
{
	pthread_t *volatile     pools = NULL;
	int volatile            i = 0;

	assert(cnt);

	TRY
	{
		NewArray0(cnt, pools);
		AssertError(pools, ENOMEM);

		for (i = 0; i < cnt; i++) {
			int rc = 0;
			rc = pthread_create(&pools[i], NULL, work, usr);
			RAISE_SYS_ERROR_ERRNO(rc);
		}
	}
	CATCH
	{
		int j = 0;

		for (j = 0; j < i; j++) {
			pthread_cancel(pools[j]);
			pthread_join(pools[j], NULL);
		}

		Free(pools);

		RERAISE;
	}
	END;

	return pools;
}

void join_pool(pthread_t *pool, int cnt)
{
	return_if_fail(pool && cnt);

	int i = 0;

	for (i = 0; i < cnt; i++) {
		pthread_join(pool[i], NULL);
	}
}

