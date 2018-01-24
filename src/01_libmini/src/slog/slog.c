//
//  slog.c
//  minilib
//
//  Created by 周凯 on 15/8/21.
//  Copyright (c) 2015年 zk. All rights reserved.
//
#include <time.h>
#include <sys/time.h>
#include <syslog.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/syscall.h>
#include <termios.h>
#include <math.h>

#include "slog/slog.h"
#include "filesystem/filesystem.h"

#include <assert.h>

/*日志以月份分界，否则以日分界*/
// #define SLOG_SPLIT_BY_MON
/*日志头字段中添加后续的日志长度字段*/
// #define SLOG_ADD_LENGTH_FIELD

#ifdef SLOG_ADD_LENGTH_FIELD
  #define LENGTH_FIELD_FMTSIZE  (6)
#else
  #define LENGTH_FIELD_FMTSIZE  (0)
#endif

/**
 * 日志级别定义表
 */
const SLogLevelT g_slogLevels[] = {
	{ LOG_DEBUG,   "D", LOG_D_LEVEL },			/* 调试信息     */
	{ LOG_INFO,    "I", LOG_I_LEVEL },			/* 应用信息     */
	{ LOG_WARNING, "W", LOG_W_LEVEL },			/* 警告信息     */
	{ LOG_ERR,     "E", LOG_E_LEVEL },			/* 错误信息     */
	{ LOG_ALERT,   "S", LOG_S_LEVEL },			/* 系统信息     */
	{ LOG_EMERG,   "F", LOG_F_LEVEL },			/* 致命信息     */
};

/*
 * 当前日志设置
 */
SLogCfgT g_slogCfg = {
	.fd             = STDOUT_FILENO,
	.crtv           = 0,
	.lock           = NULLOBJ_AO_SPINLOCK,
	.clevel         = &g_slogLevels[0],
	.alevel         = &g_slogLevels[0],
	.alevels        = DIM(g_slogLevels),
};

/*写是否加锁*/
#ifdef SLOG_LOCK_WRITE
  #define     SLOG_LOCK_WRITE()	\
	AO_ThreadSpinLock(&g_curLogConf.lock)

  #define     SLOG_UNLOCK_WRITE() \
	AO_ThreadSpinUnlock(&g_curLogConf.lock)
#else
  #define     SLOG_LOCK_WRITE()	\
	((void)0)

  #define     SLOG_UNLOCK_WRITE() \
	((void)0)
#endif

static inline
bool _GetTimeStamp(SLogCfgT *cfg, time_t *time, int *stamp)
{
	struct timeval  tv = {};
	struct tm       tm = {};
	struct tm       oldct = {};
	bool            ret = false;

	gettimeofday(&tv, NULL);

	localtime_r(&tv.tv_sec, &tm);

	SET_POINTER(time, tv.tv_sec);
	SET_POINTER(stamp, tv.tv_usec);

	/*判断是否达到下一日或下一月*/
	if (likely(cfg->crtv > 0)) {
		localtime_r(&cfg->crtv, &oldct);
#ifdef SLOG_SPLIT_BY_MON
		if (unlikely((tm.tm_year * 12 + tm.tm_mon) !=
			(oldct.tm_year * 12 + oldct.tm_mon))) {
			ret = true;
		}

#else
		if (unlikely((tm.tm_year != oldct.tm_year) ||
			(tm.tm_yday != oldct.tm_yday))) {
			ret = true;
		}
#endif
	}

	return ret;
}

static inline
char *_FormatTime(char *tmString, size_t size, time_t tmInt, const char *formate)
{
	struct tm local = {};

	localtime_r((time_t *)&tmInt, &local);
	strftime(tmString, size, formate, &local);

	return tmString;
}

void (SLogSetLevel)(SLogCfgT *cfg, const SLogLevelT *level)
{
	assert(likely(cfg));
	return_if_fail(cfg->alevel && cfg->alevels > 0);
	return_if_fail(level >= cfg->alevel && level < &cfg->alevel[cfg->alevels]);
	ATOMIC_SET(&cfg->clevel, level);
}

bool (SLogOpen)(SLogCfgT *cfg, const char *path, const SLogLevelT *level)
{
	bool            ret = false;
	struct timeval  tv = {};
	struct tm       tm = {};
	time_t          oldct = -1;
	char            oldname[MAX_NAME_SIZE];
	char            oldpath[MAX_PATH_SIZE];
	char            fullname[MAX_PATH_SIZE] = {};
	char            strtm[32] = {};
	int             fd = -1;
	int             oldfd = -1;

	gettimeofday(&tv, NULL);

	oldct = ATOMIC_SWAP(&cfg->crtv, tv.tv_sec);

	/*不能刷新太频繁*/
	return_val_if_fail(oldct != tv.tv_sec, false);

	AO_ThreadSpinLock(&cfg->lock);

	snprintf(oldname, sizeof(oldname), "%s", cfg->name);
	snprintf(oldpath, sizeof(oldpath), "%s", cfg->path);

	if (likely(path && (path[0] != '\0'))) {
		/*以指定路径打开日志*/
		const char      *basename = NULL;
		bool            flag = false;

		basename = strrchr(path, '/');

		if (basename) {
			basename += 1;

			snprintf(cfg->path, sizeof(cfg->path), "%.*s", (int)(basename - path), path);
			snprintf(cfg->name, sizeof(cfg->name), "%s", basename);

			flag = FS_IsDirectory(cfg->path, true);

			if (unlikely(!flag)) {
				goto error;
			}
		} else {
			snprintf(cfg->path, sizeof(cfg->path), "%s", "./");
			snprintf(cfg->name, sizeof(cfg->name), "%s", path);
		}
	} else if (unlikely(cfg->name[0] == '\0')) {
		goto error;
	}

#ifdef SLOG_SPLIT_BY_MON
	_FormatTime(strtm, sizeof(tm), tv.tv_sec, "%Y%m");
#else
	_FormatTime(strtm, sizeof(tm), tv.tv_sec, "%Y%m%d");
#endif

	snprintf(fullname, sizeof(fullname), "%s%s_%s", cfg->path, strtm, cfg->name);

	fd = open(fullname, O_CREAT | O_APPEND | O_WRONLY, 0644);

	if (unlikely(fd < 0)) {
		x_perror("open `%s` error : %s", fullname, x_strerror(errno));
error:
		snprintf(cfg->path, sizeof(cfg->path), "%s", oldpath);
		snprintf(cfg->name, sizeof(cfg->name), "%s", oldname);
	} else {
		oldfd = ATOMIC_SWAP(&cfg->fd, fd);

		if (likely(oldfd != fd)) {
			/*如果不是标准输出，则返还给调用者*/
			if (unlikely((oldfd == STDOUT_FILENO) ||
				/*不是一个终端关联*/
				(isatty(oldfd) == 0))) {
				oldfd = -1;
			} else {
				close(oldfd);
			}

			ret = true;
		} else {
			/*原描述是不可能和新打开的描述符相同的*/
			x_printf(F, "OS's file descriptor manager occured ERROR.");
			abort();
		}
	}

	AO_ThreadSpinUnlock(&cfg->lock);

	/*成功，这设置新等级*/
	if (likely(ret && level)) {
		SLogSetLevel(level);
	}

	return ret;
}

/**日志格式缓存*/
static __thread char _g_LogFmt[SLOG_MSGFMT_MAX] = {};

int (SLogWrite)(SLogCfgT *cfg, const SLogLevelT *level,
	const char *func, const char *file, int line,
	const char *formate, ...)
{
	assert(likely(cfg && level));

	char            strtm[32] = { 0 };
	char            progName[32] = {};
	char            *timeptr = NULL;
	int             hdrbytes = 0;
	time_t          sec = 0;
	int             nsec = 0;
	bool            flushlog = false;
	long            ptid = -1;
	int             nbytes = -1;
	const char      *prgptr = NULL;

	ptid = likely(g_ThreadID > -1) ? g_ThreadID : GetThreadID();
	prgptr = likely(g_ProgName) ? g_ProgName : GetProgName(progName, sizeof(progName));

	func = SWITCH_NULL_STR(func);
	file = SWITCH_NULL_STR(file);

	/*转换时间*/
	flushlog = _GetTimeStamp(cfg, &sec, &nsec);

	/*是否需要刷新日志*/
	if (unlikely(flushlog)) {
		(SLogOpen)(cfg, NULL, NULL);
	}

	timeptr = _FormatTime(strtm, sizeof(strtm), sec, "%Y-%m-%d %H:%M:%S");
	timeptr = SWITCH_NULL_STR(timeptr);

#if !defined(SLOG_SEMPLE_MSGHDR)
	char *fileName = NULL;

	fileName = strrchr(file, '/');
	file = fileName ? (fileName + 1) : file;
  #if defined(SLOG_CALLER)
	char    caller[32] = {};
	char    *callerptr = NULL;
	callerptr = GetCallerName(caller, sizeof(caller), 2);
	callerptr = SWITCH_NULL_STR(callerptr);
  #endif

	/*构造整个日志的格式字符串*/
	hdrbytes = snprintf(_g_LogFmt + LENGTH_FIELD_FMTSIZE,
			sizeof(_g_LogFmt) - LENGTH_FIELD_FMTSIZE,
  #if defined(SLOG_CALLER)
			"[%s|%s.%06d|%10s.%06ld.000|%24s()|%32s:%4|(%s())] : %s\n",
  #else
			LOG_COLOR_YELLOW "[%s|%s.%06d|%10s.%06ld.000|%24s()|%32s:%4d]\n%s\n" LOG_COLOR_NULL,
  #endif
			level->name, timeptr, nsec, prgptr, ptid, func, file, line,
  #if defined(SLOG_CALLER)
			callerptr,
  #endif
			formate);

#else
	hdrbytes = snprintf(logMsg, sizeof(logMsg),
			LOG_COLOR_YELLOW "|%s|%s.%06d|%16s.%06ld.000|\n%s\n" LOG_COLOR_NULL,
			level->name, timeptr, nsec, prgptr, ptid, formate);
#endif	/* if !defined(_SLOG_SEMPLE_MSGHDR) */

	/*not enough buffer to format log, or format unkown*/
	if (unlikely((hdrbytes < 0) || !(sizeof(_g_LogFmt) - LENGTH_FIELD_FMTSIZE > hdrbytes))) {
		dprintf(STDERR_FILENO,
			"%s(), %s:%d|not enough buffer to format log, or format unkown.\n",
			func, file, line);
		errno = ERANGE;
		nbytes = -1;
	} else {
		va_list ap;

		/*添加长度字段*/
#ifdef SLOG_ADD_LENGTH_FIELD
		va_start(ap, formate);
		int logbytes = vsnprintf(NULL, 0, _g_LogFmt + LENGTH_FIELD_FMTSIZE, ap);
		va_end(ap);
  #if 0
		/*是否包含自身长度*/
		logbytes += LENGTH_FIELD_FMTSIZE;
  #endif

		/*not enough buffer to format log, or format unkown*/
		if (unlikely((logbytes < 0) || (logbytes > (pow(10, LENGTH_FIELD_FMTSIZE)) - 1))) {
			dprintf(STDERR_FILENO,
				"%s(), %s:%d|not enough buffer to format log, or format unkown.\n",
				func, file, line);
			errno = ERANGE;
			nbytes = -1;
		} else {
			char bc = _g_LogFmt[LENGTH_FIELD_FMTSIZE];
			snprintf(_g_LogFmt, LENGTH_FIELD_FMTSIZE + 1, "%-*d", LENGTH_FIELD_FMTSIZE, logbytes);
			_g_LogFmt[LENGTH_FIELD_FMTSIZE] = bc;
#else
		{
#endif					/* ifdef SLOG_ADD_LENGTH_FIELD */
again:
			va_start(ap, formate);
			SLOG_LOCK_WRITE();
			nbytes = vdprintf(cfg->fd, _g_LogFmt, ap);
			SLOG_UNLOCK_WRITE();
			va_end(ap);

			if (unlikely(nbytes < 0)) {
				int code = errno;

				if (likely(code == EINTR)) {
					goto again;
				}

				dprintf(STDERR_FILENO, "%s(), %s:%d|write log failed : %s.\n",
					func, file, line, x_strerror(code));
				errno = code;
			} else {
#if 0
				fsync(fd);
#endif
			}
		}
	}

	return nbytes;
}

const SLogLevelT *(SLogIntegerToLevel)(const SLogCfgT * cfg, short level)
{
	assert(likely(cfg && cfg->alevel && cfg->alevels > 0));
	return &cfg->alevel[INRANGE(level, 0, cfg->alevels - 1)];
}

