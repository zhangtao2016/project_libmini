//
//  lj_base.h
//  supex
//
//  Created by 周凯 on 15/12/28.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#ifndef lj_base_h
#define lj_base_h

#include <stdio.h>
//#include <lua.h>
//#include <lauxlib.h>
//#include <lualib.h>
#include "luajit-2.0/lua.h"
#include "luajit-2.0/lauxlib.h"
#include "luajit-2.0/lualib.h"


#include "../utils.h"

__BEGIN_DECLS

/*
 * @param reg注册函数集合
 * @param scripath初始化脚本路径名
 * 注册函数集合，必须以{ NULL, NULL }结尾
 */
lua_State *lj_create(luaL_Reg reg[], const char *scripath);

__END_DECLS
#endif	/* lj_base_h */

