//
//  dstack.h
//  libmini
//
//  Created by 周凯 on 15/11/11.
//  Copyright © 2015年 zk. All rights reserved.
//

#ifndef __libmini_dstack_h__
#define __libmini_dstack_h__

#include <stdio.h>
#include "../adt_proto.h"

__BEGIN_DECLS

typedef struct DStack *DStackT;

struct DStack
{
	int     magic;
	int     nodes;
	int     capacity;	/*小于1则容量为无限制*/
	void    *data;
};

DStackT DStackCreate(int capacity);

bool DStackPush(DStackT stack, intptr_t data);

bool DStackPop(DStackT stack, intptr_t *data);

/**
 * 使用 NodeDestroyCB 的长度参数恒等于sizeof(intptr_t)
 */
void DStackDestroy(DStackT *stack, NodeDestroyCB destroy);

__END_DECLS
#endif	/* __libmini_dstack_h__ */

