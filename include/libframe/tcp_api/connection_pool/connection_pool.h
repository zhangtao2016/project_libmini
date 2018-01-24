//
//  connection_pool.h
//  supex
//
//  Created by 周凯 on 16/1/8.
//  Copyright © 2016年 zhoukai. All rights reserved.
//

#ifndef connection_pool_h
#define connection_pool_h

#include "../tcp_base.h"

__BEGIN_DECLS

struct pool;
struct connpool {
	int 				magic;
	struct tcp_socket 	*conn;
	struct pool 		*pool;
};

int connpool_create(const char *host, const char *server, unsigned max, bool sync);
void connpool_destroy(const char *host, const char *server);

int connpool_pull(const char *host, const char *server, struct connpool *pool);
void connpool_push(struct connpool *pool);
void connpool_disconn(struct connpool *pool);

__END_DECLS

#endif /* connection_pool_h */
