//
//  http_request.h
//  supex
//
//  Created by 周凯 on 15/12/25.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#ifndef http_reqresp_h
#define http_reqresp_h

#include "../tcp_io.h"
#include "http_parse.h"

__BEGIN_DECLS

struct http_socket
{
	struct tcp_socket    *tcp;
	struct http_parse_t  parse;
};

/* ---------------------------------------	*\
 * 初始化之前需要自行填充tcp成员
 * 发送之前需要自行填充其中的数据
\* ---------------------------------------  */

/**
 * 初始化http_socket
 * 在接收新的协议数据之前或使用协议数据之后调用
 */
static inline
void http_init(struct http_socket *http)
{
	assert(http && http->tcp);
	cache_clean(&http->tcp->cache);
	http_parse_init(&http->parse,
					&http->tcp->cache.buff,
					&http->tcp->cache.end);
}

/**
 * 发送http请求
 * @param http操作句柄，需要手动初始化
 * @param idle发送空闲回调，可以为null
 * @param usr空闲回调参数
 * @return 发送字节数
 * @see tcp_send
 */
static inline
ssize_t http_reqsend(struct http_socket *http, tcp_idle_cb idle, void *usr)
{
#if 0
	struct http_parse_t parse;
	http_parse_init(&parse, &http->tcp->cache.buff, &http->tcp->cache.end);
	bool flag = http_parse_request(&parse);
	return_val_if_fail(flag, 0);
	
	if (unlikely(parse.hs.err != HPE_OK)) {
		errno = EPROTO;
		return -1;
	};
#endif
	return tcp_send(http->tcp, idle, usr);
}

/**
 * 发送http响应
 * @see http_reqsend
 * @see tcp_send
 */
static inline
ssize_t http_respsend(struct http_socket *http, tcp_idle_cb idle, void *usr)
{
#if 1
	struct http_parse_t parse;
	http_parse_init(&parse, &http->tcp->cache.buff, &http->tcp->cache.end);
	bool flag = http_parse_response(&parse);
	return_val_if_fail(flag, 0);
	
	if (unlikely(parse.hs.err != HPE_OK)) {
		errno = EPROTO;
		return -1;
	};
#endif
	return tcp_send(http->tcp, idle, usr);
}

/* -------------													*\
 * 在接收新的http之前要调用http_init()									*
 * 在解析http出现解析错误时要调用http_init()								*
\* -------------													*/

/**
 * 接受http请求
 * @see http_reqsend
 * @see tcp_recv
 */
ssize_t http_reqrecv(struct http_socket *http, tcp_idle_cb idle, void *usr);

/**
 * 接受http响应
 * @see http_reqsend
 * @see tcp_recv
 */
ssize_t http_resprecv(struct http_socket *http, tcp_idle_cb idle, void *usr);

__END_DECLS
#endif	/* http_reqresp_h */

