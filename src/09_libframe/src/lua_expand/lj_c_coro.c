//
//  lj_c_coro.c
//  supex
//
//  Created by 周凯 on 16/1/7.
//  Copyright © 2016年 zhoukai. All rights reserved.
//


#include "lua_expand/lj_c_coro.h"

#ifdef __LINUX__
#include "scco.h"
int lj_scco_switch(lua_State *L)
{
	struct schedule *S = (struct schedule *)(intptr_t)lua_tointeger(L, 1);
	
	if (unlikely(!S)) {
		return luaL_error(L, "%s", x_strerror(EINVAL));
	}

	x_printf(D, "LUA coroutine switch happen!");
	S->_switch_(S);
	
	return 0;
}
#endif