//
//  string.h
//  minilib
//
//  Created by 周凯 on 15/8/21.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __minilib__string__
#define __minilib__string__

#include "../utils.h"

__BEGIN_DECLS

/* -------------------------------------------                */

/* ===================================================================
 * 宏函数定义 (字符处理)
 * =================================================================== */

/*
 * #ifndef bzero
 * #define  bzero(s, len) memset((s), 0, (len))
 * #endif
 */

#ifndef isascii
  #define  isascii(c)   (((c) & ~0x7f) == 0)
  #define  ISASCII(c)   isascii(c)
#else
  #define  ISASCII(c)   isascii(c)
#endif

#ifdef  isupper
  #define  ISUPPER(c)   isupper(c)
#else
  #define  ISUPPER(c)   ('A' <= (c) && (c) <= 'Z')
#endif

#ifdef  islower
  #define  ISLOWER(c)   islower(c)
#else
  #define  ISLOWER(c)   ('a' <= (c) && (c) <= 'z')
#endif

#ifdef  isdigit
  #define  ISDIGIT(c)   isdigit(c)
#else
  #define  ISDIGIT(c)   ('0' <= (c) && (c) <= '9')
#endif

#ifdef  isalpha
  #define  ISALPHA(c)   isalpha(c)
#else
  #define  ISALPHA(c)   (ISUPPER(c) || ISLOWER(c))
#endif

#ifdef  isalnum
  #define  ISALNUM(c)   isalnum(c)
#else
  #define  ISALNUM(c)   (ISDIGIT(c) || ISALPHA(c))
#endif

#ifdef  isxdigit
  #define  ISXDIGIT(c) isxdigit(c)
#else
  #define  ISXDIGIT(c)				     \
	(ISDIGIT(c) || ('A' <= (c) && (c) <= 'F') || \
	('a' <= (c) && (c) <= 'f'))
#endif

#ifdef  isblank
  #define  ISBLANK(c)   isblank(c)
#else
  #define  ISBLANK(c)   ((c) == ' ' || (c) == '\t')
#endif

#ifdef  isspace
  #define  ISSPACE(c) isspace((int)c)
#else
  #define  ISSPACE(c) \
	((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r' || (c) == '\v' || (c) == '\f')
#endif

#ifdef  toupper
  #define  TOUPPER(c)   toupper(c)
#else
  #define  TOUPPER(c)   (ISLOWER(c) ? 'A' + ((c) - 'a') : (c))
#endif

#ifdef  tolower
  #define  TOLOWER(c)   tolower(c)
#else
  #define  TOLOWER(c)   (ISUPPER(c) ? 'a' + ((c) - 'A') : (c))
#endif

/* -------------------------------------------                */

char *x_strdup(const char *src);

const char *x_strchr(const char *src, size_t size, int chr);

const char *x_strrchr(const char *src, size_t size, int chr);

const char *x_strcasechr(const char *src, size_t size, int chr);

const char *x_strstr(const char *s1, const char *s2, size_t size);

const char *x_strcasestr(const char *s1, const char *s2, size_t size);

int x_atoi(const char *nptr, int size);

size_t x_strlncat(char *dst, size_t len, const char *src, size_t n);

/**
 * @return 返回buff的16进制字符串，使用后需要手动free
 */
char *binary2hexstr(const char *buff, size_t size);

/**
 * 先打印低位
 * @return 返回buff的2进制位串，使用后需要手动free
 */
char *binary2bitstr(const char *buff, size_t size);

/**
 * 先打印高位
 * @return 返回buff的2进制位串，使用后需要手动free
 */
char *binary2bitrstr(const char *buff, size_t size);

__END_DECLS
#endif	/* defined(__minilib__string__) */

