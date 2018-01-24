#include <netinet/tcp.h>
#include "libmini.h"

static void setopt(int fd, void *usr)
{
	//        SO_SetCloseLinger(fd, true, 0);

	SO_SetSndTimeout(fd, 2000);
	FD_Enable(fd, O_NONBLOCK);
	SO_EnableKeepAlive(fd, 5, 5, 3);
}

static inline struct timeval    difftimeval(struct timeval *tv1, struct timeval *tv2);

int main(int argc, char **argv)
{
	int     loop = 0;
	int     i = 0;
	char    buff[1024] = {};

	Signal(SIGPIPE, SignalNormalCB);
	Signal(SIGINT, SignalNormalCB);

	if (unlikely(argc != 4)) {
		x_printf(E, "Usage %s <host> <service> <loop>.", argv[0]);
		return EXIT_FAILURE;
	}

	Rand();

	for (i = 0; i < DIM(buff); i++) {
		buff[i] = RandInt(1, 1024) % 127;
	}

	loop = atoi(argv[3]);

	do {
		int volatile fd = 0;
		TRY
		{
			struct timeval  start = {};
			struct timeval  end = {};
			struct timeval  escape = {};

			gettimeofday(&start, NULL);
			fd = TcpConnect(argv[1], argv[2], setopt, NULL);
			gettimeofday(&end, NULL);

			escape = difftimeval(&end, &start);
			printf(LOG_I_COLOR
				"connect() escape : %02ld.%06d"
				PALETTE_NULL
				"\n", escape.tv_sec, escape.tv_usec);

			RAISE_SYS_ERROR(fd);

			ssize_t nbytes = 0;

			gettimeofday(&start, NULL);
			nbytes = write(fd, buff, sizeof(buff));
			gettimeofday(&end, NULL);

			escape = difftimeval(&end, &start);
			printf(LOG_I_COLOR
				"  write() escape : %02ld.%06d"
				PALETTE_NULL
				"\n", escape.tv_sec, escape.tv_usec);

			RAISE_SYS_ERROR(nbytes);
#if 0
			Getch(NULL);
#endif

			char bytes = 0;
			nbytes = read(fd, &bytes, 1);
			RAISE_SYS_ERROR(nbytes);
		}
		CATCH
		{}
		FINALLY
		{
			if (likely(fd > -1)) {
				close(fd);
			}
		}
		END;
	} while (--loop > 0);

	return EXIT_SUCCESS;
}

static inline struct timeval difftimeval(struct timeval *tv1, struct timeval *tv2)
{
	struct timeval tv = {};

	tv.tv_usec = tv1->tv_usec - tv2->tv_usec + 1000000;

	tv.tv_sec = tv1->tv_sec - tv2->tv_sec - 1;

	tv.tv_sec += tv.tv_usec / 1000000;
	tv.tv_usec = tv.tv_usec % 1000000;

	return tv;
}

