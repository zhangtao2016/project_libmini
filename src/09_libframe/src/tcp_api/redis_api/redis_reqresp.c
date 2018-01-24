//
//  redis_reqresp.c
//  supex
//
//  Created by 周凯 on 16/1/21.
//  Copyright © 2016年 zhoukai. All rights reserved.
//

#include "tcp_api/redis_api/redis_reqresp.h"
struct _redis_cbarg
{
	struct redis_parse_t  *parse;
	tcp_idle_cb             idle;
	void                    *usr;
};

static inline
void _redis_cbarg_init(struct _redis_cbarg *cbarg, struct redis_socket *redis,
					  tcp_idle_cb idle, void *usr);

static int _redis_parse_req(struct tcp_socket *tcp, void *usr);

static int _redis_parse_resp(struct tcp_socket *tcp, void *usr);

static bool _redis_idle_wrap(struct tcp_socket *tcp, void *usr);
/* ---------------------			*/

ssize_t redis_reqrecv(struct redis_socket *redis, tcp_idle_cb idle, void *usr)
{
	struct _redis_cbarg cbarg = {};
	
	_redis_cbarg_init(&cbarg, redis, idle, usr);
	return tcp_recv(redis->tcp, _redis_parse_req, idle ? _redis_idle_wrap : idle, &cbarg);
}

ssize_t redis_resprecv(struct redis_socket *redis, tcp_idle_cb idle, void *usr)
{
	struct _redis_cbarg cbarg = {};
	
	_redis_cbarg_init(&cbarg, redis, idle, usr);
	return tcp_recv(redis->tcp, _redis_parse_resp, idle ? _redis_idle_wrap : idle, &cbarg);
}

/*初始化回调参数*/
static inline
void _redis_cbarg_init(struct _redis_cbarg *cbarg, struct redis_socket *redis,
					  tcp_idle_cb idle, void *usr)
{
	assert(redis);
	ASSERTOBJ(redis->tcp);
	
	cbarg->usr = usr;
	cbarg->idle = idle;
	cbarg->parse = &redis->parse;
}

/*解析redis请求回调*/
static int _redis_parse_req(struct tcp_socket *tcp, void *usr)
{
	struct _redis_cbarg      *cbarg = usr;
	bool                    flag = false;
	
	flag = redis_parse_request(cbarg->parse);
	
	if (likely(flag && (cbarg->parse->rs.error == 0))) {
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

/*解析redis响应回调*/
static int _redis_parse_resp(struct tcp_socket *tcp, void *usr)
{
	struct _redis_cbarg      *cbarg = usr;
	bool                    flag = false;
	
	flag = redis_parse_response(cbarg->parse);
	
	if (likely(flag && (cbarg->parse->rs.error == 0))) {
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

/*空闲回调包裹*/
static bool _redis_idle_wrap(struct tcp_socket *tcp, void *usr)
{
	struct _redis_cbarg *cbarg = usr;
	
	return cbarg->idle(tcp, cbarg->usr);
}