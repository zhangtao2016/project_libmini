//
//  dqueue.h
//  libmini
//
//  Created by 周凯 on 15/11/12.
//  Copyright © 2015年 zk. All rights reserved.
//

#ifndef __libmini_dqueue_h__
#define __libmini_dqueue_h__

#include <stdio.h>
#include "../adt_proto.h"

__BEGIN_DECLS

typedef struct DQueue *DQueueT;

struct DQueue
{
	int     magic;
	int     nodes;
	int     capacity;
	void    *head;
	void    *tail;
	void    *lock;
};

DQueueT DQueueCreate(int capacity);

bool DQueuePush(DQueueT queue, intptr_t data);

bool DQueuePriorityPush(DQueueT queue, intptr_t data);

bool DQueuePull(DQueueT queue, intptr_t *data);

/**
 * 使用 NodeDestroyCB 的长度参数恒等于sizeof(intptr_t)
 */
void DQueueDestroy(DQueueT *queue, NodeDestroyCB destroy);

__END_DECLS
#endif	/* __libmini_dqueue_h__ */

