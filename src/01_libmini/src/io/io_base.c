//
//  io_base.c
//  minilib
//
//  Created by 周凯 on 15/8/24.
//  Copyright (c) 2015年 zk. All rights reserved.
//
#include <sys/resource.h>
#include "io/io_base.h"
#include "utils.h"
#include "slog/slog.h"
#include "string/string.h"
#include "except/exception.h"

static inline int _FD_SelectWrap(int fd, long ms, int which)
{
	int             flag = 0;
	fd_set          set;
	struct timeval  tm = {};
	struct timeval  *tmptr = NULL;

	FD_ZERO(&set);
	FD_SET(fd, &set);

	if (likely(ms > -1)) {
		tm.tv_sec = ms / 1000;
		tm.tv_usec = (ms % 1000) * 1000;
		tmptr = &tm;
	} else {
		tmptr = NULL;
	}

	switch (which)
	{
		case 'R':
			flag = select(fd + 1, &set, NULL, NULL, tmptr);
			break;

		case 'W':
			flag = select(fd + 1, NULL, &set, NULL, tmptr);
			break;

		default:
			flag = select(fd + 1, NULL, NULL, &set, tmptr);
			break;
	}

	if (unlikely(flag == 0)) {
		errno = unlikely(ms == 0) ? EAGAIN : ETIMEDOUT;
	}

	return flag;
}

int FD_CheckWrite(int fd, long ms)
{
	int ret = _FD_SelectWrap(fd, ms, 'W');

	return likely(ret > 0) ? 0 : -1;
}

int FD_CheckRead(int fd, long ms)
{
	int ret = _FD_SelectWrap(fd, ms, 'R');

	return likely(ret > 0) ? 0 : -1;
}

int FD_CloseAll()
{
	struct rlimit   rl;
	int             i = 0;
	int             rc = 0;

	rc = getrlimit(RLIMIT_NOFILE, &rl);

	if (unlikely(rc < 0)) {
		x_printf(E, "getrlimit() for RLIMIT_NOFILE failed : %s", x_strerror(errno));
		return -1;
	}

	if (rl.rlim_max == RLIM_INFINITY) {
		rl.rlim_max = 1024;
	}

	for (i = 0; i < (int)rl.rlim_max; i++) {
		close(i);
	}

	return 0;
}

/* ----------------------                       */

void LineBuffInit(struct LineBuff *buff)
{
	assert(buff);

	buff->magic = OBJMAGIC;
	memset(buff->buff, 0, sizeof(buff->buff));
	buff->nlend = -1;
	buff->nlstart = -1;
}

ssize_t ReadLine(int fd, struct LineBuff *buff)
{
	ssize_t         n = 0;
	const char      *ptr = NULL;

	ASSERTOBJ(buff);

find:
	ptr = x_strchr(&buff->buff[buff->nlend + 1], buff->size - (buff->nlend + 1), '\n');

	if (!ptr) {
		/*位移空间*/
		if (DIM(buff->buff) - buff->size <= 0) {
			if (unlikely(buff->nlend == -1)) {
				x_printf(E, "The length of line is large, that more than %d.\n", DIM(buff->buff));
				errno = ERANGE;
				return -1;
			}

			/*在未找到数据行的情况下nlend是上次有效行的结束下标*/
			buff->size -= (buff->nlend + 1);
			memmove(buff->buff, &buff->buff[buff->nlend + 1], buff->size);
			buff->nlstart = -1;
			buff->nlend = -1;
		}

again:
		n = read(fd, &buff->buff[buff->size], DIM(buff->buff) - buff->size);

		if (likely(n > 0)) {
			buff->size += n;
			goto find;
		} else if (likely(n == 0)) {
			buff->nlstart = buff->nlend + 1;
			buff->nlend = buff->size - 1;
			return 0;
		} else {
			int error = errno;

			if (likely(error == EINTR)) {
				goto again;
			} else {
				return -1;
			}
		}
	} else {
		buff->nlstart = buff->nlend + 1;
		buff->nlend = (int)(ptr - buff->buff);
		n = buff->nlend - buff->nlstart + 1;
	}

	return n;
}

char *GetLine(struct LineBuff *buff)
{
	ASSERTOBJ(buff);

	if (unlikely(buff->nlstart >= buff->nlend)) {
		return NULL;
	}

	buff->buff[buff->nlend] = '\0';
	return &buff->buff[buff->nlstart];
}

