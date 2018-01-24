//
//  utils.h
//  minilib
//
//  Created by 周凯 on 15/8/21.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __minilib__utils__
#define __minilib__utils__

#include "common.h"

__BEGIN_DECLS
/* -------------------------------------------                */

/*一些全局，可以通过接口修改，不可以裸修改，仅在调用过一次相应的接口后有效*/
extern long             g_PageSize;	/**<页面大小*/
extern __thread long    g_ThreadID;	/**<线程号*/
extern long             g_ProcessID;	/**<进程号*/
extern long             g_CPUCores;	/**<cpu数量*/
extern const char       *g_ProgName;	/**<进程名*/
/* -------------------------------------------                */

/*
 * 日志信息
 */
#define LOG_D_LEVEL             "DEBUG"
#define LOG_I_LEVEL             "INFO "
#define LOG_W_LEVEL             "WARN "
#define LOG_E_LEVEL             "ERROR"
#define LOG_S_LEVEL             "SYS  "
#define LOG_F_LEVEL             "FATAL"

#define LOG_D_COLOR             PALETTE_FULL(_COLOR_CYAN_, _DEPTH_0_, _STYLE_NULL_, _SCENE_NULL_)
#define LOG_I_COLOR             PALETTE_FULL(_COLOR_GREEN_, _DEPTH_1_, _STYLE_ITALIC_, _SCENE_NULL_)
#define LOG_W_COLOR             PALETTE_FULL(_COLOR_YELLOW_, _DEPTH_2_, _STYLE_ITALIC_, _SCENE_CYAN_)
#define LOG_E_COLOR             PALETTE_FULL(_COLOR_PURPLE_, _DEPTH_2_, _STYLE_UNDERLINE_, _SCENE_BLUE_)
#define LOG_S_COLOR             PALETTE_FULL(_COLOR_RED_, _DEPTH_2_, _STYLE_UNDERLINE_, _SCENE_GREEN_)
#define LOG_F_COLOR             PALETTE_FULL(_COLOR_RED_, _DEPTH_2_, _STYLE_TWINKLE_, _SCENE_BLACK_)

#define LOG_COLOR_GRAY          PALETTE_FULL(_COLOR_GRAY_, _DEPTH_1_, _STYLE_NULL_, _SCENE_NULL_)
#define LOG_COLOR_RED           PALETTE_FULL(_COLOR_RED_, _DEPTH_1_, _STYLE_NULL_, _SCENE_NULL_)
#define LOG_COLOR_GREEN         PALETTE_FULL(_COLOR_GREEN_, _DEPTH_1_, _STYLE_NULL_, _SCENE_NULL_)
#define LOG_COLOR_YELLOW        PALETTE_FULL(_COLOR_YELLOW_, _DEPTH_1_, _STYLE_NULL_, _SCENE_NULL_)
#define LOG_COLOR_BLUE          PALETTE_FULL(_COLOR_BLUE_, _DEPTH_1_, _STYLE_NULL_, _SCENE_NULL_)
#define LOG_COLOR_PURPLE        PALETTE_FULL(_COLOR_PURPLE_, _DEPTH_1_, _STYLE_NULL_, _SCENE_NULL_)
#define LOG_COLOR_CYAN          PALETTE_FULL(_COLOR_CYAN_, _DEPTH_1_, _STYLE_NULL_, _SCENE_NULL_)
#define LOG_COLOR_WHITE         PALETTE_FULL(_COLOR_WHITE_, _DEPTH_1_, _STYLE_NULL_, _SCENE_NULL_)
#define LOG_COLOR_NULL          PALETTE_NULL

/*
 * 终端日志格式
 */
#undef  x_printf
#define x_printf(level, fmt, ...)							      \
	fprintf(stdout, LOG_##level##_COLOR LOG_##level##_LEVEL				      \
		LOG_COLOR_WHITE "|%16s:%4d" LOG_COLOR_PURPLE "|%06d" LOG_COLOR_BLUE "|%20s()" \
		LOG_##level##_COLOR "|" fmt PALETTE_NULL "\n",				      \
		__FILE__, __LINE__, (int)GetThreadID(),					      \
		__FUNCTION__, ##__VA_ARGS__)

/*
 * 打印错误级别的信息
 */
#undef  x_perror
#define x_perror(...) x_printf(E, ##__VA_ARGS__)

/*
 * 重定义断言
 */
#ifndef NDEBUG
  #undef assert
  #define assert(expr)							 \
	((likely((expr)) ? ((void)0) :					 \
	(x_printf(F, "assertion `%s` failed, raise a exception", #expr), \
	RAISE(EXCEPT_ASSERT))))
#endif

/* -------------------------------------------                */

/*
 * 最小值
 */
#undef  MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/*
 * 最大值
 */
#undef  MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/*
 * 计算数组长度
 */
#undef  DIM
#define DIM(x) ((int)(sizeof((x)) / sizeof((x)[0])))

/*
 * 返回x对应的负数
 */
#undef  NEG
#define NEG(x) ((x) > 0 ? -(x) : (x))

/*
 * 确定范围值
 */
#undef  INRANGE
#define INRANGE(x, low, high) \
	(((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

/* -------------------------------------------                */

/*
 * 若字符串s为空则返回"NULL", 否则返回s
 */
#undef  SWITCH_NULL_STR
#define SWITCH_NULL_STR(s) ((void *)(s) == (void *)0 ? "NULL" : (s))

/*
 * 若x指针不为空则赋值
 */
#undef  SET_POINTER
#define SET_POINTER(x, v)		      \
	STMT_BEGIN			      \
	if (likely((x))) {		      \
		*(x) = (__typeof__(*(x)))(v); \
	}				      \
	STMT_END

/* -------------------------------------------                */

/*
 * 返回以SIZE / BASESIZE 的最大倍数
 */
#undef  CEIL_TIMES
#define CEIL_TIMES(SIZE, BASESIZE) \
	(((SIZE) + (BASESIZE)-1) / (BASESIZE))

/*
 * 返回以BASESIZE为单位对齐的SIZE
 */
#undef  ADJUST_SIZE
#define ADJUST_SIZE(SIZE, BASESIZE) \
	(CEIL_TIMES((SIZE), (BASESIZE)) * (BASESIZE))

/* -------------------------------------------                */

/* ===================================================================
 * 随机数
 * =================================================================== */

/*
 * 将时间设置为随机种子
 */
#define Rand() (srandom((unsigned int)time(NULL)))

/*
 * 随机获取整数 x ： low <= x <= high
 */
#define RandInt(low, high)		      \
	(assert((long)(high) >= (long)(low)), \
	(long)((((double)random()) / ((double)RAND_MAX + 1)) * (high - low + 1)) + low)

/*
 * 随机获取小数 x ： low <= x < high
 */
#define RandReal(low, high)			  \
	(assert((double)(high) >= (double)(low)), \
	(((double)random()) / ((double)RAND_MAX + 1)) * (high - low) + low)

/*
 * 测试一次概率为 p(分数) 的事件
 */
#define RandChance(p) (RandReal(0, 1) < (double)(p) ? true : false)

/* -------------------------------------------                */

/*
 * 如果条件判断失败则返回（一个值）
 */
#define return_if_fail(expr)						 \
	STMT_BEGIN							 \
	if (likely((expr))) {						 \
		(void)0;						 \
	} else {							 \
		/*x_printf(D, "assertion `%s` failed, return", #expr);*/ \
		return;							 \
	}								 \
	STMT_END

#define return_val_if_fail(expr, val)					 \
	STMT_BEGIN							 \
	if (likely((expr))) {						 \
		(void)0;						 \
	} else {							 \
		/*x_printf(D, "assertion `%s` failed, return", #expr);*/ \
		return (val);						 \
	}								 \
	STMT_END

/* -------------------------------------------                */

/* ===================================================================
 * 结构体操作
 * =================================================================== */

/*
 * 获取结构中指定成员的偏移长度
 */
#ifndef offsetof
  #define  offsetof(TYPE, MEMBER) ((int)&((TYPE *)0)->MEMBER)
#endif

/*
 * 通过结构成员的指定获取结构的首地址
 */
#ifndef container_of
  #define  container_of(PTR, TYPE, MEMBER) \
	((TYPE *)((char *)(PTR)-offsetof(TYPE, MEMBER)))
#endif

/* -------------------------------------------                */

/* ===================================================================
 * 内存分配
 * =================================================================== */

/*
 * 分配一个ptr对应的对象
 */
#undef  New
#define New(ptr)					    \
	((ptr) = (__typeof__(ptr))calloc(1, sizeof *(ptr)), \
	(likely((ptr)) ? (ptr) :			    \
	(x_printf(W, "no more space, calloc() failed."), errno = ENOMEM, NULL)))

/*
 * 分配n个ptr对应的对象数组
 */
#undef  NewArray
#define NewArray(n, ptr)					 \
	((ptr) = (__typeof__(ptr))malloc((n) * (sizeof *(ptr))), \
	(likely((ptr)) ? (ptr) :				 \
	(x_printf(W, "no more space, malloc() failed."), errno = ENOMEM, NULL)))

/*
 * 分配n个ptr对应的对象数组，并清零
 */
#undef  NewArray0
#define NewArray0(n, ptr)					\
	((ptr) = (__typeof__(ptr))calloc((n), (sizeof *(ptr))),	\
	(likely((ptr)) ? (ptr) :				\
	(x_printf(W, "no more space, calloc() failed."), errno = ENOMEM, NULL)))

/*
 * 改变ptr内存块的长度
 */
#undef  Resize
#define Resize(ptr, nbytes)						   \
	({								   \
		void *_old = (void *)(ptr);				   \
		(ptr) = (__typeof__(ptr))realloc((void *)(ptr), (nbytes)); \
		if (unlikely(!(ptr))) {					   \
			x_printf(W, "no more space, realloc() failed.");   \
			errno = ENOMEM;					   \
			if (likely(_old)) {				   \
				free(_old);				   \
			}						   \
		}							   \
		(ptr);							   \
	})

/*
 * 检查释放
 */
#undef  Free
#define Free(ptr)			     \
	STMT_BEGIN			     \
	if (likely((ptr))) {		     \
		free((void *)(ptr));	     \
		(ptr) = (__typeof__(ptr)) 0; \
	}				     \
	STMT_END

/* -------------------------------------------                */

/* ===================================================================
 * 交换字节序
 * =================================================================== */

/* 交换双字节 */
#if !defined(SWAP_2BYTES)
  #define SWAP_2BYTES(x)	 \
	((((x) & 0xff00) >> 8) | \
	(((x) & 0x00ff) << 8))
#endif

/* 交换四字节 */
#if !defined(SWAP_4BYTES)
  #define SWAP_4BYTES(x)	      \
	((((x) & 0xff000000) >> 24) | \
	(((x) & 0x00ff0000) >> 8) |   \
	(((x) & 0x0000ff00) << 8) |   \
	(((x) & 0x000000ff) << 24))
#endif

/* 交换八字节 */
#if !defined(SWAP_8BYTES)
  #define SWAP_8BYTES(x)		      \
	((((x) & 0xff00000000000000) >> 56) | \
	(((x) & 0x00ff000000000000) >> 40) |  \
	(((x) & 0x0000ff0000000000) >> 24) |  \
	(((x) & 0x000000ff00000000) >> 8) |   \
	(((x) & 0x00000000ff000000) << 8) |   \
	(((x) & 0x0000000000ff0000) << 24) |  \
	(((x) & 0x000000000000ff00) << 40) |  \
	(((x) & 0x00000000000000ff) << 56))
#endif

/*
 * 是否是小端字节序
 */
union _byteorder
{
	int             i;
	unsigned char   c[4];
};

extern const union _byteorder g_ByteOrder;

#define ISLITTLEEND() (likely(g_ByteOrder.c[0] == 0x04) ? true : false)

/* ---------------------------------            */

/*
 * 位图定义宏，如下使用：
 * static unsigned short stopwatch[] = {
 *    S16 _ _ _ _ _ X X X X X _ _ _ X X _ ,
 *    S16 _ _ _ X X X X X X X X X _ X X X ,
 *    S16 _ _ X X X _ _ _ _ _ X X X _ X X ,
 *    S16 _ X X X _ _ _ _ _ _ _ X X X _ X ,
 *    S16 _ X X _ _ _ _ _ _ _ _ _ X X _ _ ,
 *    S16 X X _ _ _ _ _ _ _ _ _ _ _ X X _ ,
 *    S16 X X _ _ _ _ _ _ _ _ _ _ _ X X _ ,
 *    S16 X X _ X X X X X _ _ _ _ _ X X _ ,
 *    S16 X X _ _ _ _ _ X _ _ _ _ _ X X _ ,
 *    S16 X X _ _ _ _ _ X _ _ _ _ _ X X _ ,
 *    S16 _ X X _ _ _ _ X _ _ _ _ X X _ _ ,
 *    S16 _ X X X _ _ _ X _ _ _ X X X _ X ,
 *    S16 _ _ X X X _ _ _ _ _ X X X _ X X ,
 *    S16 _ _ _ X X X X X X X X X _ X X X ,
 *    S16 _ _ _ _ _ X X X X X _ _ _ X X _ ,
 * };
 *
 */
#undef X
#define X       ) *2 + 1
#undef _
#define _       ) *2
#define S16     ((((((((((((((((0			/*16位位图*/
#define S32     ((((((((((((((((((((((((((((((((0	/*32位位图*/

/* -------------------------------------------                */

/**
 * 获取系统调用错误消息,线程安全
 */
char *x_strerror(int error);

/**
 * 是否是getaddrinfo()类的错误
 */
bool IsGaiError(int error);

/**
 * 返回getaddrinfo()类的错误字符串
 */
const char *GaiStrerror(int error);

/* -------------------------------------------                */

/**
 * 设置程序名称,仅第一次调用有效,线程安全
 */
void SetProgName(const char *name);

/*
 * 获取程序名称
 */
char *GetProgName(char *buff, size_t size);

/*
 * 获取指定栈帧的调用函数名称,调用该函数的层为0,当传入负数时视为0
 */
char *GetCallerName(char *buff, size_t size, int layer);

/* -------------------------------------------                */

/*
 * 将调用线程绑定到指定的 CPU 核上执行.当传入负数时, CPU 绑定核由函数决定
 */
bool BindCPUCore(long which);

/*
 * 获取 CPU 核心数
 */
long GetCPUCores();

/*
 * 获取调用线程的线程 ID,主线程的 ID 与进程 ID 一致
 */
long GetThreadID();

/*
 * 获取调用进程的进程 ID.
 */
long GetProcessID();

/* -------------------------------------------                */

/**
 * 从终端获取一个字符，但不回显该字符
 */
int Getch(int *c);

/**
 * 获取页大小
 */
long GetPageSize();

/* -------------------------------------------                */

__END_DECLS
#endif	/* defined(__minilib__utils__) */

