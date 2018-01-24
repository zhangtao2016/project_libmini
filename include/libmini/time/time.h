//
//  time.h
//  minilib
//
//  Created by 周凯 on 15/8/21.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __minilib__time__
#define __minilib__time__

#include <time.h>
#include <sys/time.h>
#include "../utils.h"

__BEGIN_DECLS

/*
 * 时间字符串最大长度
 */
#define     TM_STRING_SIZE              (25)

/*
 * 时间格式
 */
#define     TM_FORMAT_DATE              "%Y-%m-%d"
#define     TM_FORMAT_SHORT_DATE        "%y-%m-%d"
#define     TM_FORMAT_TIME              "%H:%M:%S"
#define     TM_FORMAT_DATETIME          "%Y-%m-%d %H:%M:%S"

/**
 * 格式时间
 */
char *TM_FormatTime(char *tmString, size_t size, time_t tmInt, const char *formate);

/**
 * 解析时间
 */
time_t TM_ParseTime(const char *tmString, const char *formate);

/**
 * 填充绝对时间，精度为毫秒
 */
void TM_FillAbsolute(struct timespec *tmspec, long value);

/**
 * 获取当前时间的秒数和微秒数，并总毫秒数（相对于1970年的数字）
 */
#define TM_GetTimeStamp(secptr, nsecptr)		\
	({						\
		struct timeval _tm = {};		\
		time_t *_tptr1 = (time_t *)(secptr);	\
		int *_tptr2 = (int *)(nsecptr);		\
		gettimeofday(&_tm, NULL);		\
		SET_POINTER(_tptr1, _tm.tv_sec);	\
		SET_POINTER(_tptr2, _tm.tv_usec);	\
		_tm.tv_sec * 1000 + _tm.tv_usec / 1000;	\
	})

__END_DECLS
#endif	/* defined(__minilib__time__) */

