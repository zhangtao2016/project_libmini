//
//  redis_reqresp.h
//  supex
//
//  Created by 周凯 on 16/1/21.
//  Copyright © 2016年 zhoukai. All rights reserved.
//

#ifndef redis_reqresp_h
#define redis_reqresp_h

#include "../tcp_io.h"
#include "redis_parse.h"

__BEGIN_DECLS

struct redis_socket {
	struct tcp_socket	 *tcp;
	struct redis_parse_t parse;
};

/* ---------------------------------------	*\
 *初始化之前需要自行填充tcp成员
 *发送之前需要自行填充其中的数据
\* ---------------------------------------  */

/**
 * 初始化redis_socket
 * 在接收新的协议数据之前或使用协议数据之后调用
 */
static inline
void redis_init(struct redis_socket *redis)
{
	assert(redis);
	cache_clean(&redis->tcp->cache);
	redis_parse_init(&redis->parse,
					&redis->tcp->cache.buff,
					&redis->tcp->cache.end);
}

/**
 * 发送http请求
 * @param redis操作句柄，需要手动初始化
 * @param idle发送空闲回调，可以为null
 * @param usr空闲回调参数
 * @return 发送字节数
 * @see tcp_send
 */
static inline
ssize_t redis_reqsend(struct redis_socket *redis, tcp_idle_cb idle, void *usr)
{
	return tcp_send(redis->tcp, idle, usr);
}

/**
 * 发送redis响应
 * @see redis_reqsend
 * @see tcp_send
 */
static inline
ssize_t redis_respsend(struct redis_socket *redis, tcp_idle_cb idle, void *usr)
{
	return tcp_send(redis->tcp, idle, usr);
}

/* -------------													*\
 * 在接收新的redis之前要调用redis_init()								*
 * 在解析redis出现解析错误时要调用redis_init()							*
\* -------------													*/

/**
 * 接受http请求
 * @see http_reqsend
 * @see tcp_recv
 */
ssize_t redis_reqrecv(struct redis_socket *redis, tcp_idle_cb idle, void *usr);

/**
 * 接受http响应
 * @see http_reqsend
 * @see tcp_recv
 */
ssize_t redis_resprecv(struct redis_socket *redis, tcp_idle_cb idle, void *usr);

__END_DECLS


#endif /* redis_reqresp_h */
