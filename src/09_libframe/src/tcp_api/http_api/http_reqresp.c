//
//  http_request.c
//  supex
//
//  Created by 周凯 on 15/12/25.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include "tcp_api/http_api/http_reqresp.h"

struct _http_cbarg
{
	struct http_parse_t  *parse;
	tcp_idle_cb             idle;
	void                    *usr;
};

static inline
void _http_cbarg_init(struct _http_cbarg *cbarg, struct http_socket *http,
	tcp_idle_cb idle, void *usr);

static int _http_parse_req(struct tcp_socket *tcp, void *usr);

static int _http_parse_resp(struct tcp_socket *tcp, void *usr);

static bool _http_idle_wrap(struct tcp_socket *tcp, void *usr);
/* ---------------------			*/

ssize_t http_reqrecv(struct http_socket *http, tcp_idle_cb idle, void *usr)
{
	struct _http_cbarg cbarg = {};
	ssize_t nbytes = 0;
	
	_http_cbarg_init(&cbarg, http, idle, usr);
	nbytes = tcp_recv(http->tcp, _http_parse_req,
					  idle ? _http_idle_wrap : idle, &cbarg);
	
	if (unlikely(nbytes == 0)) {
		nbytes = -1;
		errno = ECONNRESET;
	}
	
	return nbytes;
}

ssize_t http_resprecv(struct http_socket *http, tcp_idle_cb idle, void *usr)
{
	struct _http_cbarg cbarg = {};
	ssize_t nbytes = 0;
	_http_cbarg_init(&cbarg, http, idle, usr);
	nbytes = tcp_recv(http->tcp, _http_parse_resp,
					idle ? _http_idle_wrap : idle, &cbarg);
	
	if (unlikely(nbytes == 0)) {
		nbytes = -1;
		errno = ECONNRESET;
	}
	
	return nbytes;
}

/*初始化回调参数*/
static inline
void _http_cbarg_init(struct _http_cbarg *cbarg, struct http_socket *http,
	tcp_idle_cb idle, void *usr)
{
	assert(http);
	ASSERTOBJ(http->tcp);
	
	cbarg->usr = usr;
	cbarg->idle = idle;
	cbarg->parse = &http->parse;
}

/*解析http请求回调*/
static int _http_parse_req(struct tcp_socket *tcp, void *usr)
{
	struct _http_cbarg      *cbarg = usr;
	bool                    flag = false;

	flag = http_parse_request(cbarg->parse);

	if (likely(flag && (cbarg->parse->hs.err == HPE_OK))) {
		/*已完成，上一级函数正常退出*/
		return 1;
	} else if (likely(!flag)) {
		/*仍需要解析后续数据，上一级函数继续循环*/
		return 0;
	} else {
		/*解析数据出错，上一级函数继续异常退出*/
		return -1;
	}

	return -1;
}

/*解析http响应回调*/
static int _http_parse_resp(struct tcp_socket *tcp, void *usr)
{
	struct _http_cbarg      *cbarg = usr;
	bool                    flag = false;

	flag = http_parse_response(cbarg->parse);

	if (likely(flag && (cbarg->parse->hs.err == HPE_OK))) {
		/*已完成，上一级函数正常退出*/
		return 1;
	} else if (likely(!flag)) {
		/*仍需要解析后续数据，上一级函数继续循环*/
		return 0;
	} else {
		/*解析数据出错，上一级函数异常退出*/
		return -1;
	}

	return -1;
}

/*空闲回调包裹*/
static bool _http_idle_wrap(struct tcp_socket *tcp, void *usr)
{
	struct _http_cbarg *cbarg = usr;

	return cbarg->idle(tcp, cbarg->usr);
}

