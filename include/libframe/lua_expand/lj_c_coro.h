//
//  lj_c_coro.h
//  supex
//
//  Created by 周凯 on 16/1/7.
//  Copyright © 2016年 zhoukai. All rights reserved.
//

#ifndef lj_c_coro_h
#define lj_c_coro_h

#include <stdio.h>
#include "lj_base.h"

__BEGIN_DECLS

/**scco协程库切换*/
#ifdef __LINUX__
int lj_scco_switch(lua_State *L);
#endif
/**coro协程库切换*/

__END_DECLS


#endif /* lj_c_coro_h */
