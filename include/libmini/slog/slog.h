//
//  slog.h
//  minilib
//
//  Created by 周凯 on 15/8/21.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __minilib__slog__
#define __minilib__slog__

#include "../utils.h"
#include "../atomic/atomic.h"

__BEGIN_DECLS

/* -------------------------------------------                */

/*
 * 打印调试信息
 */
#undef x_printf

#ifdef DEBUG
  #define SLogOpen(path, level)         (true)
  #define SLogSetLevel(level)           ((void)0)
  #define SLogIntegerToLevel(level)     ((void*)0)

  #define SLogWriteByInfo(level, function, file, line, fmt, ...)		\
	fprintf(stdout, 							\
		LOG_##level##_COLOR 						\
		LOG_##level##_LEVEL					  	\
		LOG_COLOR_WHITE "|%16s:%4d"     				\
		LOG_COLOR_PURPLE "|%06d" 					\
		LOG_COLOR_BLUE "|%20s()" 					\
		LOG_##level##_COLOR "|" 					\
		fmt PALETTE_NULL "\n",					  	\
		(file), (line), (int)GetThreadID(), (function), ##__VA_ARGS__)

  #define x_printf(level, fmt, ...)		\
        SLogWriteByInfo(level, 			\
        		__FUNCTION__, 		\
        		__FILE__, 		\
                	__LINE__, 		\
                	fmt, ##__VA_ARGS__)

#else

  #define SLOG_D_LEVEL  (&g_slogLevels[0])
  #define SLOG_I_LEVEL  (&g_slogLevels[1])
  #define SLOG_W_LEVEL  (&g_slogLevels[2])
  #define SLOG_E_LEVEL  (&g_slogLevels[3])
  #define SLOG_S_LEVEL  (&g_slogLevels[4])
  #define SLOG_F_LEVEL  (&g_slogLevels[5])

/*判断日志级别是否正确*/
  #define isRightSlogLevel(level)	 \
      (((level != &g_slogLevels[0]) && 	 \
	(level != &g_slogLevels[1]) &&	 \
	(level != &g_slogLevels[2]) &&	 \
	(level != &g_slogLevels[3]) &&	 \
	(level != &g_slogLevels[4]) &&	 \
	(level != &g_slogLevels[5])) ? false : true)

  #define SLogOpen(path, level) ((SLogOpen)(&g_slogCfg, (path), (level)))

  #define SLogSetLevel(level)				   \
	(void)({					   \
		if (likely(isRightSlogLevel(level))) {	   \
			(SLogSetLevel)(&g_slogCfg, level); \
		}					   \
		0;					   \
	})

  #define SLogIntegerToLevel(level) ((SLogIntegerToLevel)(&g_slogCfg, (level)))

  #define SLogWriteByInfo(level, function, file, line, fmt, ...)	  \
	({								  \
		int n = 0;						  \
		if (unlikely(!isRightSlogLevel(SLOG_##level##_LEVEL))) {  \
			abort();					  \
		}							  \
		if (unlikely(SLOG_##level##_LEVEL >= g_slogCfg.clevel)) { \
			n = (SLogWrite)(&g_slogCfg, SLOG_##level##_LEVEL, \
			function, file, line,				  \
			fmt, ##__VA_ARGS__);				  \
		}							  \
		n;							  \
	})

  #define x_printf(level, fmt, ...)		       \
	SLogWriteByInfo(level, __FUNCTION__, __FILE__, \
		__LINE__, fmt, ##__VA_ARGS__)
#endif	/* ifdef DEBUG */

/*
 * 消息格式化字符串长度
 */
#define     SLOG_MSGFMT_MAX (512)

/**
 * 日志级别
 */
typedef struct _SLogLevel
{
	/** 日志级别代码 */
	const int       level;
	/** 日志级别简称称 */
	const char      *shortname;
	/** 日志级别名称 */
	const char      *name;
} SLogLevelT;

/**
 * 日志配置
 * 需要以静态方式初始化
 */
typedef struct _SLogCfg
{
	/*日志输出描述符*/
	int                     fd;
	/*日志文件创建／刷新时间*/
	time_t                  crtv;
	/*日志文件名*/
	char                    name[MAX_NAME_SIZE];
	/*日志路径*/
	char                    path[MAX_PATH_SIZE];
	/*日志配置信息加载锁*/
	AO_SpinLockT            lock;
	/*当前日志级别*/
	const SLogLevelT        *clevel;
	/*日志级别配置数组*/
	const SLogLevelT *const alevel;
	/*日志级别配置数组大小*/
	const int               alevels;
} SLogCfgT;

/**默认日志级别定义表*/
extern const SLogLevelT g_slogLevels[];
/**默认日志配置*/
extern SLogCfgT g_slogCfg;
/* -------------------------------------------                */

/**
 * 开启或刷新日志配置
 */
bool (SLogOpen)(SLogCfgT *cfg, const char *path, const SLogLevelT *level);

/**
 * 设置日志等级
 */
void (SLogSetLevel)(SLogCfgT *cfg, const SLogLevelT *level);

/**
 * 写日志
 */
int (SLogWrite)(SLogCfgT *cfg, 
		const SLogLevelT *level,
		const char *func, 
		const char *file, 
		int line,
		const char *formate, ...) __printflike(6, 7);

/**
 * 转换数字等级到对应的日志等级指针
 */
const SLogLevelT *(SLogIntegerToLevel)(const SLogCfgT * cfg, short level);

/* -------------------------------------------                */
__END_DECLS
#endif	/* defined(__minilib__slog__) */

