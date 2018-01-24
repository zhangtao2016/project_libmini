//
//  time.c
//  minilib
//
//  Created by 周凯 on 15/8/21.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#include "time/time.h"
#include "slog/slog.h"
#include "except/exception.h"

/**
 * 格式时间
 */
char *TM_FormatTime(char *tmString, size_t size, time_t tmInt, const char *formate)
{
	struct tm       local;
	struct tm       *tmptr = NULL;
	size_t          nbytes = 0;

	assert(tmString);
	assert(formate);

	tmptr = localtime_r((time_t *)&tmInt, &local);

	if (unlikely(!tmptr)) {
		x_printf(E, "Can't formate time [%ld] : %s.",
			tmInt, x_strerror(errno));
		return NULL;
	}

	nbytes = strftime(tmString, size, formate, &local);

	if (unlikely(nbytes == 0)) {
		errno = ENOSPC;
		x_printf(E, "Can't formate time [%ld] : %s.",
			tmInt, x_strerror(errno));
		return NULL;
	}

	return tmString;
}

/**
 * 解析时间
 */
time_t TM_ParseTime(const char *tmString, const char *formate)
{
	time_t result = -1;

#if defined(__LINUX__) && !defined(_GNU_SOURCE)
	x_printf(W, "Please define `_GNU_SOURCE` macro.");
	errno = ENXIO;
	return result;

#else
	struct tm       local = {};
	char            *ptr = NULL;

	assert(tmString && formate);

	ptr = strptime(tmString, formate, &local);

	if (unlikely(ptr != NULL)) {
		return -1;
	}

	result = mktime(&local);

	if (unlikely(result == -1)) {
		errno = EINVAL;
		x_printf(E, "Can't parse the string of time [%s] : %s.",
			tmString, x_strerror(errno));
	}
	return result;
#endif	/* if defined(__LINUX__) && !defined(_GNU_SOURCE) */
}

/**
 * 填充绝对时间，精度为毫秒
 */
void TM_FillAbsolute(struct timespec *tmspec, long value)
{
	assert(tmspec);

	struct timeval tv = {};

	gettimeofday(&tv, NULL);

	tv.tv_sec += value / 1000;
	tv.tv_usec += (value % 1000) * 1000;

	if (tv.tv_usec > 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}

	tmspec->tv_sec = tv.tv_sec;
	tmspec->tv_nsec = (long)(tv.tv_usec * 1000);
}

