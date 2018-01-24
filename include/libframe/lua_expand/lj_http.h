//
//  lj_http.h
//  supex
//
//  Created by 周凯 on 15/12/26.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#ifndef lj_http_h
#define lj_http_h

#include <stdio.h>
//#include <lua.h>
//#include <lauxlib.h>
//#include <lualib.h>
#include "../luajit-2.0/lua.h"
#include "../luajit-2.0/lauxlib.h"
#include "../luajit-2.0/lualib.h"

__BEGIN_DECLS

/**
 * 初始化特定主机和端口的http连接池
 * lua调用func(host, port, max-element, is-sync)
 * 失败时会抛出错误，成功时无提示。
 */
int lj_http_init_pool(lua_State *L);

/**
 * http数据请求
 * lua调用func(scheduler, host, port, data, timeout)
 * 当schedule为nil时表示需要同步请求，否则异步请求
 * 当timeout为nil或者不传入时，表示阻塞请求；如果为0仅尝试请求一次。
 * 返回响应。
 */
int lj_http_req(lua_State *L);


/* --------------		*/

__END_DECLS
#endif	/* lj_http_h */

