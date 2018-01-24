//
//  memqueue.h
//  libmini
//
//  Created by 周凯 on 15/9/14.
//  Copyright © 2015年 zk. All rights reserved.
//

#ifndef memqueue_h
#define memqueue_h

#include "squeue.h"

__BEGIN_DECLS

/* ---------------                  */

/*
 * 创建共享内存队列
 */
typedef struct MemQueue *MemQueueT;

struct MemQueue
{
	int     magic;
	char    *name;
	SQueueT data;
};

/**
 * 初始化
 */
MemQueueT MEM_QueueCreate(const char *name, int nodetotal, int nodesize);

bool MEM_QueuePush(MemQueueT queue, char *buff, int size, int *effectsize);

bool MEM_QueuePriorityPush(MemQueueT queue, char *buff, int size, int *effectsize);

bool MEM_QueuePull(MemQueueT queue, char *buff, int size, int *effectsize);

void MEM_QueueDestroy(MemQueueT *queue, bool shmrm, NodeDestroyCB destroy);

/* ----------------                 */

__END_DECLS
#endif	/* memqueue_h */

