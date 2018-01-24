//
//  connection_pool.c
//  supex
//
//  Created by 周凯 on 16/1/8.
//  Copyright © 2016年 zhoukai. All rights reserved.
//

#include "pool_api/pool.h"
#include "tcp_api/connection_pool/connection_pool.h"

#ifndef CONNPOOL_TIMEDOUT
#define CONNPOOL_TIMEDOUT 500
#endif

static const char _conn_format[] = "conn_pool$%s$%s";

struct _conn_arg {
	const char *host;
	const char *server;
};

static int _connpool_connect(struct tcp_socket **tcp, struct _conn_arg *arg);
static int _connpool_checker(struct tcp_socket **tcp, struct _conn_arg *arg);
static int _connpool_distconn(struct tcp_socket **tcp, struct _conn_arg *arg);


int connpool_create(const char *host, const char *server, unsigned max, bool sync)
{
	assert(host && server);
	assert(host[0] && server[0]);
	
	char buff[128] = { };
	
	snprintf(buff, sizeof(buff), _conn_format, host, server);
	int ret = 0;
	ret = pool_create(buff, max, sync,
					  (pool_callback)_connpool_connect,
					  (pool_callback)_connpool_distconn,
					  (pool_callback)_connpool_checker);
	if (unlikely(ret < 0)) {
		x_perror("create connection pool `%s:%s` fail : %s", host, server, x_strerror(errno));
	}
	return ret;
}

void connpool_destroy(const char *host, const char *server)
{
	assert(host && server);
	assert(host[0] && server[0]);
	
	char buff[128] = { };
	
	snprintf(buff, sizeof(buff), _conn_format, host, server);
	pool_destroy(buff, NULL);
}

int connpool_pull(const char *host, const char *server, struct connpool *pool)
{
	assert(pool);
	
	if (unlikely(ISOBJ(pool))) {
		x_printf(W, "this connection have not been return back to this pool yet.");
		errno = EINVAL;
		return -1;
	}
	
	char buff[128] = { };
	
	snprintf(buff, sizeof(buff), _conn_format, host, server);
	
	pool->pool = pool_gain(buff);
	if (unlikely(!pool->pool)) {
		x_perror("operate connection pool `%s:%s` fail : %s", host, server, x_strerror(errno));
		return -1;
	}
	
	int ret = 0;
	struct _conn_arg arg = { host, server };
	ret = pool_element_pull(pool->pool, (void*)&pool->conn, &arg);
	if (unlikely(ret < 0)) {
		x_perror("operate connection pool `%s:%s` fail : %s", host, server, x_strerror(errno));
	} else {
		REFOBJ(pool);
	}
	return ret;
}

void connpool_push(struct connpool *pool)
{
	return_if_fail(UNREFOBJ(pool));
	pool_element_push(pool->pool, (intptr_t)pool->conn);
}

void connpool_disconn(struct connpool *pool)
{
	return_if_fail(UNREFOBJ(pool));
	pool_element_free(pool->pool, (intptr_t)pool->conn, NULL);
}

static int _connpool_connect(struct tcp_socket **tcp, struct _conn_arg *arg)
{
	*tcp = tcp_connect(arg->host, arg->server, CONNPOOL_TIMEDOUT);
	return unlikely(!*tcp) ? -1 : 0;
}

static int _connpool_checker(struct tcp_socket **tcp, struct _conn_arg *arg)
{
	ASSERTOBJ(*tcp);
	int ret = 0;
#if 1
	struct stat st = { };
	ret = fstat((*tcp)->fd, &st);
	if (unlikely(ret < 0 || !S_ISSOCK(st.st_mode))) {
		x_printf(W, "the file descriptor `%d` is not socket type", (*tcp)->fd);
		return -1;
	}
#else
	ret = FD_CheckWrite((*tcp)->fd, 0);
	if (unlikely(ret < 0)) {
		int code = errno;
		if (unlikely(code != EAGAIN && code != ETIMEDOUT)) {
			return -1;
		}
	}
#endif
	/*重置缓存*/
	cache_clean(&(*tcp)->cache);
	cache_cutmem(&(*tcp)->cache);
	return 0;
}

static int _connpool_distconn(struct tcp_socket **tcp, struct _conn_arg *arg)
{
	return_val_if_fail(tcp, -1);
	tcp_destroy(*tcp);
	return 0;
}
