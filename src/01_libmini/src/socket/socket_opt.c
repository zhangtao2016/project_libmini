//
//  socket_opt.c
//
//
//  Created by 周凯 on 15/9/6.
//
//

#include "socket/socket_opt.h"
#include "slog/slog.h"
#include "except/exception.h"

void SO_GetIntOption(int fd, int level, int name, int *val)
{
	int             rc = -1;
	socklen_t       length = -1;

	length = sizeof(int);
	rc = getsockopt(fd, level, name, val, &length);

	if (unlikely(rc < 0)) {
		x_printf(E, "Get option [L : %d, O : %d] failed: %s.",
			level, name, x_strerror(errno));
		RAISE(EXCEPT_SYS);
	}
}

void SO_SetIntOption(int fd, int level, int name, int val)
{
	int rc = -1;

	rc = setsockopt(fd, level, name, &val, (socklen_t)sizeof(int));

	if (unlikely(rc < 0)) {
		x_printf(E, "Set option [L : %d, O : %d] failed: %s.",
			level, name, x_strerror(errno));
		RAISE(EXCEPT_SYS);
	}
}

/**
 * 获取时间型套接字选项
 * 微秒单位
 */
void SO_GetTimeOption(int fd, int level, int name, long *usec)
{
	int             rc = -1;
	struct timeval  tv = {};
	socklen_t       length = -1;

	length = sizeof(struct timeval);
	rc = getsockopt(fd, level, name, &tv, &length);

	if (unlikely(rc < 0)) {
		x_printf(E, "Get option [L : %d, O : %d] failed: %s.",
			level, name, x_strerror(errno));
		RAISE(EXCEPT_SYS);
	}

	SET_POINTER(usec, tv.tv_sec * 1000000 + tv.tv_usec);
}

/**
 * 设置时间型套接字选项
 * 微秒单位
 */
void SO_SetTimeOption(int fd, int level, int name, long usec)
{
	struct timeval  tv = {};
	int             rc = -1;

	return_if_fail(usec > -1);

	tv.tv_sec = usec / 1000000;
	tv.tv_usec = usec % 1000000;

	rc = setsockopt(fd, level, name, &tv, (socklen_t)sizeof(struct timeval));

	if (unlikely(rc < 0)) {
		x_printf(E, "Set option [L : %d, O : %d] failed: %s.",
			level, name, x_strerror(errno));
		RAISE(EXCEPT_SYS);
	}
}

void SO_SetCloseLinger(int fd, bool onoff, int linger)
{
	struct linger   buff = { onoff ? 1 : 0, linger > 0 ? linger : 0 };
	int             rc = -1;

	rc = setsockopt(fd, SOL_SOCKET, SO_LINGER, &buff, sizeof(buff));

	if (unlikely(rc < 0)) {
		x_printf(E, "Set the option of linger failed: %s.",
			x_strerror(errno));
		RAISE(EXCEPT_SYS);
	}
}

void SO_EnableKeepAlive(int fd, int idle, int intervel, int trys)
{
	SO_SetIntOption(fd, SOL_SOCKET, SO_KEEPALIVE, 1);

#ifdef TCP_KEEPIDLE
	if (idle > 0) {
		SO_SetIntOption(fd, IPPROTO_TCP, TCP_KEEPIDLE, idle);
	}
#endif

#ifdef TCP_KEEPINTVL
	if (intervel > 0) {
		SO_SetIntOption(fd, IPPROTO_TCP, TCP_KEEPINTVL, intervel * 2);
	}
#endif

#ifdef TCP_KEEPCNT
	if (trys) {
		SO_SetIntOption(fd, IPPROTO_TCP, TCP_KEEPCNT, trys);
	}
#endif
}

