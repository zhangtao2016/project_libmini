//
//  cache.c
//  supex
//
//  Created by 周凯 on 15/12/24.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include "cache/cache.h"

#undef CACHE_FMTSIZE
#define CACHE_FMTSIZE	(128)

ssize_t cache_incrmem(struct cache *cache, int size)
{
	return_val_if_fail(size != 0, 0);
	ASSERTOBJ(cache);
	ssize_t need = 0;
	need = unlikely(size < 0) ? cache->incsize : size;
	unsigned remain = 0;
	remain = cache->size - cache->end;

	if (likely(remain >= need)) {
		return remain;
	}

	/*too many space*/
	if (unlikely(cache->end == cache->maxsize)) {
		errno = ENOMEM;
		return -1;
	}

	/*must be less than virtual size of remain space*/
	need = MIN(need, (cache->maxsize - cache->end));
	/*calculate size of space what will be need*/
	ssize_t incsize = ADJUST_SIZE(need, cache->incsize);
	/*must be less than size of max*/
	ssize_t total = MIN(cache->size + incsize, cache->maxsize);
	/*request size may be zero*/
	if (likely(cache->size < total)) {
		char *ptr = NULL;
		ptr = realloc(cache->buff, total);

		if (unlikely(!ptr)) {
			total = cache->size + need;
			ptr = realloc(cache->buff, total);

			if (unlikely(!ptr)) {
				x_printf(W, "no more space, realloc() failed.");
				errno = ENOMEM;
				return -1;
			}
		}

		cache->buff = ptr;
		cache->size = (unsigned)total;
	} else {
		need = remain;
	}

	return need;
}

ssize_t cache_append(struct cache *cache, const char *data, int size)
{
	ssize_t len = 0;

	ASSERTOBJ(cache);
	assert(data);
	size = size < 0 ? (int)strlen(data) : size;
	return_val_if_fail(size, 0);

	/*increment size of cache*/
	len = cache_incrmem(cache, size);
	/*not enough space or occured some error*/
	if (unlikely(len < size)) {
		if (likely(len > 0)) {
			errno = EMSGSIZE;
		}
		return -1;
	}

	memcpy(&cache->buff[cache->end], data, size);
	cache->end += size;

	return size;
}

ssize_t cache_vappendf(struct cache *cache, const char *fmt, va_list ap)
{
	ssize_t len = 0;
	int size = CACHE_FMTSIZE;
	va_list apcopy;
	
	ASSERTOBJ(cache);
	assert(fmt);
	
	/*increment default size of cache to format*/
	len = cache_incrmem(cache, size);
	if (likely(len >= size)) {
		va_copy(apcopy, ap);
		size = vsnprintf(&cache->buff[cache->end], len, fmt, apcopy);
		va_end(apcopy);
		return_val_if_fail(size > 0, size);
		if (likely(size <= len)) {
			cache->end += size;
			return size;
		}
	}
	
	/*allocate a fixed space*/
	if (unlikely(size == CACHE_FMTSIZE)) {
		va_copy(apcopy, ap);
		size = vsnprintf(NULL, 0, fmt, apcopy);
		va_end(apcopy);
		return_val_if_fail(size > 0, size);
	}
	
	len = cache_incrmem(cache, size + 1);
	/*not enough space or occured some error*/
	if (unlikely(len < size + 1)) {
		if (likely(len > 0)) {
			errno = EMSGSIZE;
		}
		return -1;
	}
	
	va_copy(apcopy, ap);
	/*try to format by length of default*/
	size = vsnprintf(&cache->buff[cache->end], len, fmt, apcopy);
	va_end(apcopy);
	/*maybe fail because it did not got enough memory*/
	if (likely(size > 0)) {
		cache->end += size;
	}
	
	return size;
}

ssize_t cache_appendf(struct cache *cache, const char *fmt, ...)
{
	ssize_t size = 0;
	va_list ap;
	
	assert(fmt);
	
	va_start(ap, fmt);
	size = cache_vappendf(cache, fmt, ap);
	va_end(ap);
	
	return size;
}

ssize_t cache_insert(struct cache *cache, const char *data, int size)
{
	ssize_t len = 0;
	
	ASSERTOBJ(cache);
	assert(data);
	size = size < 0 ? (int)strlen(data) : size;
	return_val_if_fail(size, 0);
	
	/*increment size of cache*/
	len = cache_incrmem(cache, size);
	/*not enough space or occured some error*/
	if (unlikely(len < size)) {
		if (likely(len > 0)) {
			errno = EMSGSIZE;
		}
		return -1;
	}
	/*move all of data from origin position to new position*/
	unsigned mlen = cache->end - cache->start;
	if (likely(mlen > 0)) {
		memmove(&cache->buff[cache->start + size], &cache->buff[cache->start], mlen);
	}
	/*copy data*/
	memcpy(&cache->buff[cache->start], data, size);
	cache->end += size;
	
	return size;
}

ssize_t cache_vinsertf(struct cache *cache, const char *fmt, va_list ap)
{
	ssize_t len = 0;
	int size = CACHE_FMTSIZE;
	va_list apcopy;
	
	ASSERTOBJ(cache);
	assert(fmt);
	
	/*calculate and allocate a fixed space*/
	va_copy(apcopy, ap);
	size = vsnprintf(NULL, 0, fmt, apcopy);
	va_end(apcopy);
	return_val_if_fail(size > 0, size);
	len = cache_incrmem(cache, size + 1);
	/*not enough space or occured some error*/
	if (unlikely(len < size + 1)) {
		if (likely(len > 0)) {
			errno = EMSGSIZE;
		}
		return -1;
	}
	
	/*move all of data from origin position to new position*/
	unsigned mlen = cache->end - cache->start;
	if (likely(mlen > 0)) {
		memmove(&cache->buff[cache->start + size], &cache->buff[cache->start], mlen);
	}
	/*backup charactor at border position*/
	char ch = cache->buff[cache->start + size];
	va_copy(apcopy, ap);
	size = vsnprintf(&cache->buff[cache->start], size + 1, fmt, apcopy);
	va_end(apcopy);
	/*restore charactor at border position*/
	cache->buff[cache->start + size] = ch;
	/*maybe fail because it did not got enough memory*/
	if (unlikely(size < 0)) {
		/*repair start position*/
		cache->start += size;
	}
	cache->end += size;
	
	return size;
}

ssize_t cache_insertf(struct cache *cache, const char *fmt, ...)
{
	ssize_t size = 0;
	va_list ap;
	
	assert(fmt);
	
	va_start(ap, fmt);
	size = cache_vinsertf(cache, fmt, ap);
	va_end(ap);
	
	return size;
}

ssize_t cache_get(struct cache *cache, char *data, unsigned size)
{
	ssize_t len = 0;

	ASSERTOBJ(cache);
	assert(data);
	return_val_if_fail(size, 0);

	len = cache->end - cache->start;

	/*not any valid data*/
	if (unlikely(len == 0)) {
		errno = EAGAIN;
		return -1;
	}

	len = MIN(size, len);
	memcpy(data, &cache->buff[cache->start], len);
	cache->start += len;

	return len;
}
