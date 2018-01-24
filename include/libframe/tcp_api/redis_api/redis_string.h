//
//  redis_string.h
//  supex
//
//  Created by 周凯 on 16/1/11.
//  Copyright © 2016年 zhoukai. All rights reserved.
//

#ifndef redis_string_h
#define redis_string_h

#include "../../utils.h"
#include "../../cache/cache.h"

__BEGIN_DECLS


/**
 * 使用字符串构造redis协议字符串
 * 每个字段以格式字符串中的空格为分界，字段数据中支持空格。
 * 格式支持(h{0,2}|l{0,2}|z)?[diouxX]的整型转换；
 * 格式支持(l{0,1}|L)?[aAeEfFgG]的浮点型转换；
 * 格式支持[spc]的字符串，指针，字符转换；
 * 格式支持带（参数传入）宽度和精度
 * 如果fmt为null则追加"*-1\r\n"到缓冲
 * 如果fmt为""则追加"*0\r\n"到缓冲
 * @param cache 缓冲，其中可以容纳多条组装数据
 * @return 返回附加数据量
 */
ssize_t redis_stringf(struct cache *cache, const char *fmt, ...) __printflike(2, 3);

/**
 * 格式化redis协议字符串
 * @param cache 缓冲，其中可以容纳多条组装数据
 * @see redis_stringf
 */
ssize_t redis_vstringf(struct cache *cache, const char *fmt, va_list ap);

/* ------------------------------------------------ *\
 * 请在组装数据后，使用数据后，使用cache的接口自行管理空间 	*
 * 需要自行填充cache成员，并且cache中只能容纳一条组装数据	*
 * 不能使用下列分段协议串构造函数组装"*-1\r\n"或"*0\r\n" 	*
\* ------------------------------------------------ */
struct redis_string
{
	int 			magic; /**<魔数*/
	unsigned		fields;	/**<携带字段数*/
	struct cache    *cache;	/**<目标缓冲*/
};

/**
 * 初始化用于请求的redis
 * 在初次初始化或掉用redis_string_complete()后
 */
static inline void redis_string_init(struct redis_string *req)
{
	assert(req);
	if (unlikely(!REFOBJ(req))) {
		x_printf(W, "these data are waitting for calling redis_string_complete().");
	}
	
	cache_clean(req->cache);
	req->fields = 0;
}

/* ---- 出错后需要重新初始化struct redis_request结构，并重新填充所有数据 ---- */

/**
 * 增加后续字段数据，如果需要的话
 * @param field 如果为null，则追加一个redis的nil字段
 * @param size 字段长度，如果为0，则字段数据将被忽略，如果为负数，字段被视作字符串
 * @return 返回本次添加的数据量；< 0则表示出错，并设置errno
 */
ssize_t redis_string_append(struct redis_string *req, const char *field, int size);

/**
 * 增加后续字段数据，如果需要的话
 * @return 返回本次添加的数据量；< 0则表示出错，并设置errno
 * @see redis_string_append()
 */
ssize_t redis_string_appendf(struct redis_string *req, const char *fmt, ...) __printflike(2, 3);

/**
 * 完成一次完整的请求填充
 * @return 返回请求字符串长度；< 0则表示出错，并设置errno
 */
ssize_t redis_string_complete(struct redis_string *req);


/* ---------------------------------------------------------------- *\
 * 以上函数用于单个字段的组装命令请求和多块响应的响应						*
\* ---------------------------------------------------------------- */

/**
 * 构造一个单块redis响应字符串
 * 如果fmt为null则追加"*-1\r\n"到缓冲
 * 如果fmt为""则追加"*0\r\n"到缓冲
 * @param cache 缓冲，其中可以容纳多条组装数据
 */
ssize_t redis_string_bulk(struct cache *cache, const char *fmt, ...) __printflike(2, 3);

/**
 * 构造普通消息响应字符串
 * @param cache 缓冲，其中可以容纳多条组装数据
 */
ssize_t redis_string_info(struct cache *cache, const char *fmt, ...) __printflike(2, 3);

/**
 * 构造普通消息响应字符串
 * @param cache 缓冲，其中可以容纳多条组装数据
 */
ssize_t redis_string_error(struct cache *cache, const char *fmt, ...) __printflike(2, 3);

/**
 * @param cache 缓冲，其中可以容纳多条组装数据
 * 构造普通消息响应字符串
 */
ssize_t redis_string_number(struct cache *cache, long number);



__END_DECLS
#endif	/* redis_string_h */
