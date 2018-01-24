//
//  string.c
//  minilib
//
//  Created by 周凯 on 15/8/21.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#include "string/string.h"
#include "slog/slog.h"
#include "except/exception.h"

char *x_strdup(const char *src)
{
	return_val_if_fail(src != NULL && src[0] != '\0', NULL);

	size_t  len = strlen(src) + 1;
	char    *out = NULL;

	NewArray0(len, out);

	return_val_if_fail(out, NULL);

	snprintf(out, len, "%s", src);

	return out;
}

const char *x_strchr(const char *src, size_t size, int chr)
{
	const char      *ptr = NULL;
	size_t          i = 0;

	assert(src);

	return_val_if_fail(size > 0, NULL);

	for (i = 0; i < size; i++) {
		if (unlikely((int)src[i] == chr)) {
			ptr = &src[i];
			break;
		}
	}

	return ptr;
}

const char *x_strrchr(const char *src, size_t size, int chr)
{
	const char      *ptr = NULL;
	size_t          i = 0;

	assert(src);

	return_val_if_fail(size > 0, NULL);

	for (i = size; i > 0; i--) {
		if (unlikely((int)src[i - 1] == chr)) {
			ptr = &src[i - 1];
			break;
		}
	}

	return ptr;
}

const char *x_strcasechr(const char *src, size_t size, int chr)
{
	const char      *ptr = NULL;
	size_t          i = 0;

	assert(src);

	return_val_if_fail(size > 0, NULL);

	for (i = 0; i < size; i++) {
		if (unlikely(TOUPPER(src[i]) == TOUPPER(chr))) {
			ptr = &src[i];
			break;
		}
	}

	return ptr;
}

const char *x_strstr(const char *s1, const char *s2, size_t size)
{
	const char      *p = NULL;
	size_t          s1len = 0;
	size_t          s2len = 0;

	assert(s1);
	assert(s2);

	s1len = MIN(size, strlen(s1));
	s2len = strlen(s2);

	return_val_if_fail((s1len > 0) && (s2len > 0), NULL);

	for (p = s1; ((s1 + s1len) > p) && ((size_t)(s1 + s1len - p) >= s2len); p++) {
		if (unlikely((*p == *s2) && (strncmp(p, s2, s2len) == 0))) {
			return p;
		}
	}

	return 0;
}

const char *x_strcasestr(const char *s1, const char *s2, size_t size)
{
	const char      *p = NULL;
	size_t          s1len = 0;
	size_t          s2len = 0;

	assert(s1);
	assert(s2);

	s1len = MIN(size, strlen(s1));
	s2len = strlen(s2);

	return_val_if_fail(s1len > 0 && s2len > 0, NULL);

	for (p = s1; ((s1 + s1len) > p) && ((size_t)(s1 + s1len - p) >= s2len); p++) {
		if (unlikely((TOUPPER(*p) == TOUPPER(*s2)) && (strncasecmp(p, s2, s2len) == 0))) {
			return p;
		}
	}

	return 0;
}

int x_atoi(const char *nptr, int size)
{
	int     c;					/* current char */
	int     total;					/* current total */
	int     sign;					/* if '-', then negative, otherwise positive */

	/* skip whitespace */
	while (size > 0 && (ISSPACE((int)(unsigned char)*nptr))) {
		++nptr;
		--size;
	}

	return_val_if_fail(size > 0, 0);

	c = (int)(unsigned char)*nptr++;
	sign = c;				/* save sign indication */

	if ((c == '-') || (c == '+')) {
		c = (int)(unsigned char)*nptr++;			/* skip sign */

		if (unlikely(--size <= 0)) {
			return 0;
		}
	}

	total = 0;

	while (size-- > 0 && ISDIGIT(c)) {
		total = 10 * total + (c - '0');					/* accumulate digit */
		c = (int)(unsigned char)*nptr++;				/* get next char */
	}

	return (sign == '-') ? -total : total;		/* return result, negated if necessary */
}

size_t x_strlncat(char *dst, size_t len, const char *src, size_t n)
{
	size_t  slen;
	size_t  dlen;
	size_t  rlen;
	size_t  ncpy;

	slen = strnlen(src, n);
	dlen = strnlen(dst, len);

	if (dlen < len) {
		rlen = len - dlen;
		ncpy = slen < rlen ? slen : (rlen - 1);
		memcpy(dst + dlen, src, ncpy);
		dst[dlen + ncpy] = '\0';
	}

	assert(len > slen + dlen);
	return slen + dlen;
}

char *binary2hexstr(const char *buff, size_t size)
{
	const uint8_t   *ub = (const uint8_t *)buff;
	char            *ptr = NULL;
	size_t          len = 0;
	size_t          i = 0;

	assert(ub);
	return_val_if_fail(size, NULL);

	for (ptr = NULL, len = i = 0; i < size; i++) {
again:

		if (unlikely(len < (i << 1) + 3)) {
			len += 128;

			Resize(ptr, len + 1);
			AssertError(ptr, ENOMEM);

			if (unlikely(len < (i << 1) + 3)) {
				goto again;
			}
		}

		snprintf(&ptr[i << 1], len - (i << 1), "%02X", ub[i]);
	}

	return ptr;
}

char *binary2bitstr(const char *buff, size_t size)
{
	const uint8_t   *ub = (const uint8_t *)buff;
	char            *ptr = NULL;
	size_t          len = 0;
	size_t          i = 0;

	assert(buff);
	return_val_if_fail(size, NULL);

	for (ptr = NULL, len = i = 0; i < size; i++) {
		int j = 0;

again:

		if (unlikely(len < (i << 3) + 9)) {
			len += 512;

			Resize(ptr, len + 1);
			AssertError(ptr, ENOMEM);

			if (unlikely(len < (i << 3) + 9)) {
				goto again;
			}
		}

		for (j = 0; j < 8; j++) {
			//
			snprintf(&ptr[(i << 3) + j], len - ((i << 3) + j), "%c", (ub[i] >> j) & 0x01 ? '0' : '1');
		}
	}

	return ptr;
}

char *binary2bitrstr(const char *buff, size_t size)
{
	const uint8_t   *ub = (const uint8_t *)buff;
	char            *ptr = NULL;
	size_t          len = 0;
	size_t          i = 0;

	assert(buff);
	return_val_if_fail(size, NULL);

	ub = ub + size - 1;

	for (ptr = NULL, len = i = 0; i < size; i++) {
		int j = 0;

again:

		if (unlikely(len < (i << 3) + 9)) {
			len += 512;

			Resize(ptr, len + 1);
			AssertError(ptr, ENOMEM);

			if (unlikely(len < (i << 3) + 9)) {
				goto again;
			}
		}

		for (j = 0; j < 8; j++) {
			//
			snprintf(&ptr[(i << 3) + j], len - ((i << 3) + j), "%c", ub[-i] & (0x80 >> j) ? '1' : '0');
		}
	}

	return ptr;
}

