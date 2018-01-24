//
//  lj_http.c
//  supex
//
//  Created by 周凯 on 15/12/26.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include "../libevcoro/evcoro_scheduler.h"
#include "cache/cache.h"
#include "tcp_api/tcp_base.h"
#include "tcp_api/http_api/http_reqresp.h"
#include "tcp_api/connection_pool/connection_pool.h"
#include "lua_expand/lj_http.h"

static bool _lj_http_idle(struct tcp_socket *tcp, void *usr);

static int _lj_http_req(lua_State *L, const char *host, const char *port,
	const char *data, int size,
	int rcvto, int connto, void *S);

int lj_http_init_pool(lua_State *L)
{
	int top = lua_gettop(L);
	
	if (unlikely(top != 2)) {
		luaL_error(L, "expected 4 arguments: <host> <service> <#max-elements> <is-sync>");
	}
	
	int sync = lua_toboolean(L, 4);
	int max = luaL_checknumber(L, 3);
	if (unlikely(max < 1)) {
		luaL_error(L, "expected 3th arguments greate than 0.");
	}
	
	const char      *port = lua_tostring(L, 2);
	const char      *host = luaL_checkstring(L, 1);
	
	int flag = connpool_create(host, port, max, sync != 0 ? false : true);
	if (unlikely(flag < 0)) {
		luaL_error(L, "initalize `%s:%s` connection pool failed : %s.", host, port, x_strerror(errno));
	}
	
	return 0;
}

int lj_http_req(lua_State *L)
{
	int top = lua_gettop(L);
	
	if (unlikely(top != 4 && top != 5)) {
		luaL_error(L, "expected 4 or 5 arguments : [schedule] <host> <port> <data> (timeout)");
	}
	
	int			to = -1;
	size_t		size = 0;
	const char	*data = lua_tolstring(L, 4, &size);
	const char	*port = lua_tostring(L, 3);
	const char	*host = luaL_checkstring(L, 2);
	void		*S = NULL;
	
	if (!lua_isnil(L, 1)) {
		S = lua_touserdata(L, 1);
	}
	if (top == 5) {
		to = luaL_checkint(L, 5);
	}
	
	return _lj_http_req(L, host, port, data, (int)size, to, to, S);
}

static bool _lj_http_idle(struct tcp_socket *tcp, void *usr)
{
	if (usr) {
		evcoro_fastswitch(usr);
	}
	return true;
}

static int _lj_http_req(lua_State *L, const char *host, const char *port,
	const char *data, int size,
	int rcvto, int connto, void *S)
{
	/*连接主机*/
#if 1
	struct connpool cp = {};
	
	int flag = 0;
	flag = connpool_pull(host, port, &cp);
	if (unlikely(flag < 0)) {
		luaL_error(L, "connect `%s : %d` failed : %s !", host, port, x_strerror(errno));
	}
	
	cp.conn->timeout = rcvto;
	struct http_socket http = { .tcp = cp.conn };
	
	/*初始化*/
	http_init(&http);
	/*装填数据*/
	ssize_t n = -1;
	n = cache_append(&cp.conn->cache, data, size);
	if (unlikely(flag < 0)) {
		/*归还连接*/
		connpool_push(&cp);
		luaL_error(L, "add memory space : %s !", x_strerror(errno));
	}
	
	/*开始发送*/
	n = http_reqsend(&http, _lj_http_idle, S);
	if (unlikely(flag < 0)) {
		/*销毁失效的连接*/
		connpool_disconn(&cp);
		luaL_error(L, "send data to `%s : %d` failed : %s !", host, port, x_strerror(errno));
	}
	
	/*初始化*/
	http_init(&http);
	/*接收回应*/
	n = http_reqrecv(&http, _lj_http_idle, S);
	if (unlikely(flag < 0)) {
		/*销毁失效的连接*/
		connpool_disconn(&cp);
		luaL_error(L, "recv data to `%s : %d` failed : %s !", host, port, x_strerror(errno));
	}
	
	/*将接收到的数据返回*/
	lua_pushlstring(L, cache_data_address(&cp.conn->cache),
					cache_data_length(&cp.conn->cache));
	
	connpool_push(&cp);
	
#else
	struct tcp_socket *tcp = NULL;

	tcp = tcp_connect(host, port, connto);

	if (unlikely(!tcp)) {
		luaL_error(L, "connect `%s : %d` failed : %s !", host, port, x_strerror(errno));
	}

	tcp->timeout = rcvto;
	struct http_socket http = { .tcp = tcp };
	/*开始发送*/
	http_init(&http);
	
	/*装填数据*/
	ssize_t n = -1;
	n = cache_append(&tcp->cache, data, size);

	if (unlikely(n < 0)) {
		tcp_destroy(tcp);
		luaL_error(L, "add memory space : %s !", x_strerror(errno));
	}
	
	n = http_reqsend(&http, _lj_http_idle, S);

	if (unlikely(n < 0)) {
		tcp_destroy(tcp);
		luaL_error(L, "send data to `%s : %d` failed : %s !", host, port, x_strerror(errno));
	}

	/*接收回应*/
	http_init(&http);
	n = http_reqrecv(&http, _lj_http_idle, S);

	if (unlikely(n < 0)) {
		tcp_destroy(tcp);
		luaL_error(L, "recv data to `%s : %d` failed : %s !", host, port, x_strerror(errno));
	}

	/*将接收到的数据返回*/
	lua_pushlstring(L, cache_data_address(&tcp->cache),
					cache_data_length(&tcp->cache));

	/*销毁连接*/
	tcp_destroy(http.tcp);
#endif
	
	return 1;
}

