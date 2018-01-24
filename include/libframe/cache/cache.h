//
//  cache.h
//  supex
//
//  Created by 周凯 on 15/12/24.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#ifndef cache_h
#define cache_h


#include "../utils.h"

__BEGIN_DECLS

/* ----------- 参数 ----------- */

#ifndef CACHE_MAXSIZE
  #define CACHE_MAXSIZE 102400
#endif

#ifndef CACHE_INITSIZE
  #define CACHE_INITSIZE 128
#endif

#ifndef CACHE_INCSIZE
  #define CACHE_INCSIZE 64
#endif

/* ----------- 检验参数是否正确 ----------- */

#if CACHE_MAXSIZE < 1
  #error "ERROR PARAMRTER `CACHE_MAXSIZE`"
#endif

#if CACHE_INITSIZE < 1 || CACHE_INITSIZE > CACHE_MAXSIZE
  #error "ERROR PARAMRTER `CACHE_INITSIZE`"
#endif

#if CACHE_INCSIZE < 1 || CACHE_INCSIZE > CACHE_MAXSIZE
  #error "ERROR PARAMRTER `CACHE_INCSIZE`"
#endif
/* ------------------------------------- */
struct cache
{
	/* ----- private : only read ------ 	*/
	int             magic;
	/* ----- private : write & read ------  */
	unsigned        maxsize;	/**<最大容量*/
	unsigned        initsize;	/**<初始化大小*/
	unsigned        incsize;	/**<每次增长的大小*/
	/* ----- private : only read ------ 	*/
	unsigned        start;		/**<有效数据起始下标*/
	unsigned        end;		/**<有效数据结束下班*/
	unsigned        size;		/**<缓冲长度*/
	char            *buff;		/**<缓冲地址*/
	/* ----- public : write & read ------ 	*/
	char            *usr;		/**<用户层附加数据指针*/
};

/*get length of data and first pointer of data*/
#define cache_data_length(cptr)	((cptr)->end - (cptr)->start)
#define cache_data_address(cptr) (&(cptr)->buff[(cptr)->start])

static inline bool cache_init(struct cache *cache)
{
	assert(cache);
	bzero(cache, sizeof(*cache));
	cache->maxsize = CACHE_MAXSIZE;
	cache->incsize = CACHE_INCSIZE;
	cache->initsize = CACHE_INITSIZE;
	cache->size = cache->initsize;
	cache->size = MIN(cache->maxsize, cache->size);
	NewArray(cache->size, cache->buff);

	if (unlikely(!cache->buff)) {
		errno = ENOMEM;
		return false;
	}

	REFOBJ(cache);
	return true;
}

static inline void cache_finally(struct cache *cache)
{
	return_if_fail(UNREFOBJ(cache));

	if (unlikely(cache->start != cache->end)) {
		x_printf(W, "some data not be deal yet.");
	}

	Free(cache->buff);
}

/**移动内部的空闲空间*/
static inline void cache_movemem(struct cache *cache)
{
	ASSERTOBJ(cache);

	if (likely(cache->start != 0)) {
		unsigned len = 0;
		len = cache->end - cache->start;

		if (len > 0) {
			memmove(cache->buff, &cache->buff[cache->start], len);
		}

		cache->end -= cache->start;
		cache->start = 0;
	}
}

/**收缩内部空闲空间*/
static inline void cache_cutmem(struct cache *cache)
{
	cache_movemem(cache);

	if (unlikely(cache->size - cache->end > cache->initsize)) {
		char    *buff = NULL;
		unsigned  size = cache->end + cache->initsize;
		buff = realloc(cache->buff, size);

		if (likely(buff)) {
			cache->buff = buff;
			cache->size = size;
		} else {
			x_printf(W, "no more space.");
		}
	}
}

/**清除所有数据，并收缩空间*/
static inline void cache_clean(struct cache *cache)
{
	ASSERTOBJ(cache);
	cache->end = cache->start = 0;
#if 0
	cache_cutmem(cache);
#endif
}

/**
 * 增加空间
 *@param size<0时，使用默认值，＝0时，返回剩余空间
 *@return -1失败，设置错误值到errno中；>0可用空间大小，可能小于期望增长值
 */
ssize_t cache_incrmem(struct cache *cache, int size);

/**
 *将data中的数据追加到cache的尾部
 *@param size，-1时，将data视为c字符串
 *@return 返回-1，失败，并设置错误值到errno中；返回放入的数据量
 */
ssize_t cache_append(struct cache *cache, const char *data, int size);

/**
 * 以格式化的方式追加数据，但不将'\0'加入
 * @see cache_append
 */
ssize_t cache_appendf(struct cache *cache, const char *fmt, ...) __printflike(2, 3);

/**
 * 以格式化的方式追加数据
 * @see cache_appendf
 */
ssize_t cache_vappendf(struct cache *cache, const char *fmt, va_list ap);

/**
 *将data中的数据插入到cache的头部
 *@param size，-1时，将data视为c字符串
 *@return 返回-1，失败，并设置错误值到errno中；返回放入的数据量
 */
ssize_t cache_insert(struct cache *cache, const char *data, int size);

/**
 * 以格式化的方式插入数据，但不将'\0'加入
 * @see cache_insert
 */
ssize_t cache_insertf(struct cache *cache, const char *fmt, ...) __printflike(2, 3);

/**
 * 以格式化的方式插入数据
 * @see cache_insertf
 */
ssize_t cache_vinsertf(struct cache *cache, const char *fmt, va_list ap);

/**
 *按顺序取出cache中的数据到data中
 *@return 返回-1，失败，并设置错误值到errno中；返回取出到数据量
 */
ssize_t cache_get(struct cache *cache, char *data, unsigned size);

/* --------------		*/
__END_DECLS
#endif	/* cache_h */
