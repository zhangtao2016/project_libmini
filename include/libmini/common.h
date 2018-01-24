//
//  common.h
//  libmini
//
//  Created by 周凯 on 15/7/16.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __libmini_common_h__
#define __libmini_common_h__

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>

/* -------------------------------------------                */
// #undef DEBUG

/*
 * C导出到C++
 */
#if defined(__cplusplus)
  #if !defined(__BEGIN_DECLS)
    #define __BEGIN_DECLS \
	extern "C" {
    #define __END_DECLS	\
	}
  #endif
#else
  #define __BEGIN_DECLS
  #define __END_DECLS
#endif

/*
 * 平台监测
 */
#if !defined(__LINUX__) && (defined(__linux__) || defined(__KERNEL__) \
	|| defined(_LINUX) || defined(LINUX) || defined(__linux))
  #define  __LINUX__    (1)
#elif !defined(__APPLE__) && defined(__MacOSX__)
  #define  __APPLE__    (1)
#elif !defined(__CYGWIN__) && (defined(__CYGWIN32__) || defined(CYGWIN))
  #define  __CYGWIN__   (1)
#endif

#ifdef __LINUX__
  #include <malloc.h>
#endif

/*
 * intel x86 平台
 */
#if (__i386__ || __i386 || __amd64__ || __amd64)
  #ifndef __X86__
    #define __X86__
  #endif
#endif

#ifndef _cpu_pause
  #if defined(__X86__) || defined(__GNUC__)
    #define _cpu_pause()        __asm__("pause")
  #else
    #define _cpu_pause()        ((void)0)
  #endif
#endif

#ifndef _cpu_nop
  #if defined(__X86__) || defined(__GNUC__)
    #define _cpu_nop()  __asm__("nop")
  #else
    #define _cpu_nop()  ((void)0)
  #endif
#endif

/*
 * 编译器版本
 */
/* gcc version. for example : v4.1.2 is 40102, v3.4.6 is 30406 */
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

/* -------------------------------------------                */
/*隐藏与可见*/
#if GCC_VERSION >= 40000
  #define EXPORT __attribute__((visibility("default")))
#else
  #define EXPORT
#endif
/* -------------------------------------------                */

#if !(defined(STMT_BEGIN) && defined(STMT_END))
  #define STMT_BEGIN do {
  #define STMT_END \
	}	   \
	while (0)
#endif

/*
 * 字符串格式化参数检查
 */
#if !defined(__printflike)
  #define       __printflike(fmtarg, firstvararg) \
	__attribute__((__format__(__printf__, fmtarg, firstvararg)))
#endif

/*
 *逻辑跳转优化
 */
#if GCC_VERSION
/*条件大多数为真，与if配合使用，直接执行if中语句
 *如果x == 0,那么结果就是 0; 如果x == 1,那么结果就是1.
 *使用了!!是为了让x转化成bool型的。
*/
  #define likely(x)     __builtin_expect(!!(x), 1)
/*条件大多数为假，与if配合使用，直接执行else中语句*/
  #define unlikely(x)   __builtin_expect(!!(x), 0)
#else
  #define likely(x)     (!!(x))
  #define unlikely(x)   (!!(x))
#endif

/*
 * 魔数
 * 结构体中设置一个magic的成员变量，已检查结构体是否被正确初始化
 */
#if !defined(OBJMAGIC)
  #define OBJMAGIC (0xfedcba98)
/*设置魔术*/
  #define REFOBJ(obj)				 \
	({					 \
		bool _ret = false;		 \
		if (likely((obj))) {		 \
			(obj)->magic = OBJMAGIC; \
			_ret = true;		 \
		}				 \
		_ret;				 \
	})
/*重置魔数*/
  #define UNREFOBJ(obj)			     \
	({				     \
		bool _ret = false;	     \
		if (likely((obj) &&	     \
		(obj)->magic == OBJMAGIC)) { \
			(obj)->magic = 0;    \
			_ret = true;	     \
		}			     \
		_ret;			     \
	})
/*验证魔数*/
  #define ISOBJ(obj) \
	((obj) && (obj)->magic == OBJMAGIC)
/*断言魔数*/
  #define ASSERTOBJ(obj) \
	(assert(ISOBJ((obj))))
#endif	/* if !defined(OBJMAGIC) */
/* -------------------------------------------                */

/*
 * 终端色彩调色板定义
 */
#undef PALETTE_NULL
#undef PALETTE_FULL

#define _PALETTE_FORMAT_(element) "\033["#element "m"

#define _DEPTH_0_       2
#define _DEPTH_1_       0
#define _DEPTH_2_       1

#define _DEPTH_0_VALUE_(depth)  _PALETTE_FORMAT_(depth)
#define _DEPTH_1_VALUE_(depth)  ""
#define _DEPTH_2_VALUE_(depth)  _PALETTE_FORMAT_(depth)

#define _STYLE_ITALIC_          3
#define _STYLE_UNDERLINE_       4
#define _STYLE_TWINKLE_         5
#define _STYLE_NULL_

#define _STYLE_ITALIC_VALUE_(style)     _PALETTE_FORMAT_(style)
#define _STYLE_UNDERLINE_VALUE_(style)  _PALETTE_FORMAT_(style)
#define _STYLE_TWINKLE_VALUE_(style)    _PALETTE_FORMAT_(style)
#define _STYLE_NULL_VALUE_(style)       ""

#define _COLOR_GRAY_    30
#define _COLOR_RED_     31
#define _COLOR_GREEN_   32
#define _COLOR_YELLOW_  33
#define _COLOR_BLUE_    34
#define _COLOR_PURPLE_  35
#define _COLOR_CYAN_    36
#define _COLOR_WHITE_   37
#define _COLOR_NULL_

#define _COLOR_GRAY_VALUE_(color)       _PALETTE_FORMAT_(color)
#define _COLOR_RED_VALUE_(color)        _PALETTE_FORMAT_(color)
#define _COLOR_GREEN_VALUE_(color)      _PALETTE_FORMAT_(color)
#define _COLOR_YELLOW_VALUE_(color)     _PALETTE_FORMAT_(color)
#define _COLOR_BLUE_VALUE_(color)       _PALETTE_FORMAT_(color)
#define _COLOR_PURPLE_VALUE_(color)     _PALETTE_FORMAT_(color)
#define _COLOR_CYAN_VALUE_(color)       _PALETTE_FORMAT_(color)
#define _COLOR_WHITE_VALUE_(color)      _PALETTE_FORMAT_(color)
#define _COLOR_NULL_VALUE_(color)       ""

#define _SCENE_BLACK_   40
#define _SCENE_RED_     41
#define _SCENE_GREEN_   42
#define _SCENE_YELLOW_  43
#define _SCENE_BLUE_    44
#define _SCENE_PURPLE_  45
#define _SCENE_CYAN_    46
#define _SCENE_WHITE_   47
#define _SCENE_NULL_

#define _SCENE_BLACK_VALUE_(scene)      _PALETTE_FORMAT_(scene)
#define _SCENE_RED_VALUE_(scene)        _PALETTE_FORMAT_(scene)
#define _SCENE_GREEN_VALUE_(scene)      _PALETTE_FORMAT_(scene)
#define _SCENE_YELLOW_VALUE_(scene)     _PALETTE_FORMAT_(scene)
#define _SCENE_BLUE_VALUE_(scene)       _PALETTE_FORMAT_(scene)
#define _SCENE_PURPLE_VALUE_(scene)     _PALETTE_FORMAT_(scene)
#define _SCENE_CYAN_VALUE_(scene)       _PALETTE_FORMAT_(scene)
#define _SCENE_WHITE_VALUE_(scene)      _PALETTE_FORMAT_(scene)
#define _SCENE_NULL_VALUE_(scene)       ""

#define PALETTE_NULL "\033[m"
#define PALETTE_FULL(color, depth, style, scene) \
	PALETTE_NULL color##VALUE_(color) depth##VALUE_(depth) style##VALUE_(style) scene##VALUE_(scene)

/* -------------------------------------------                */
#define MAX_BLOCK_SIZE  (8192)					/* 最大文本块长度 */
#define MAX_LINE_SIZE   (4096)					/* 最大单行文本长度 */
#define MAX_PATH_SIZE   (512)					/* 最大路径长度 */
#define MAX_NAME_SIZE   (64)					/* 最大文件名长度*/
/* -------------------------------------------                */
#endif	/* ifndef __minilib_common_h__ */
