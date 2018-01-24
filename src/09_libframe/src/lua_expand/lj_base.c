//
//  lj_base.c
//  supex
//
//  Created by 周凯 on 15/12/28.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include "lua_expand/lj_base.h"

lua_State *lj_create(luaL_Reg reg[], const char *scripath)
{
	lua_State *volatile L = NULL;

	TRY
	{
		L = luaL_newstate();
		AssertError(L, ENOMEM);

		luaopen_base(L);
		luaL_openlibs(L);

		int i = 0;

		for (i = 0; reg && (reg[i].name && reg[i].func); i++) {
			assert(reg[i].name[0]);
			lua_register(L, reg[i].name, reg[i].func);
		}

		if (scripath) {
			int     top = 0;
			int     code = 0;
			top = lua_gettop(L);
			code = luaL_loadfile(L, scripath);

			if (likely(code == 0)) {
				code = lua_pcall(L, 0, LUA_MULTRET, 0);

				if (unlikely(code != 0)) {
					x_printf(W, "%s", lua_tostring(L, -1));
					errno = (code == LUA_ERRMEM ? ENOMEM : EINVAL);
					RAISE(EXCEPT_SYS);
				}
			} else {
				x_printf(W, "%s", lua_tostring(L, -1));

				if (code == LUA_ERRFILE) {
					errno = ENOENT;
				} else if (code == LUA_ERRMEM) {
					errno = ENOMEM;
				} else {
					errno = EINVAL;
				}

				RAISE(EXCEPT_SYS);
			}

			lua_settop(L, top);
		}
	}
	CATCH
	{
		lua_close(L);
		L = NULL;
	}
	END;

	return L;
}

