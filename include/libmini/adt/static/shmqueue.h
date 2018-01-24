//
//  shmqueue.h
//  libmini
//
//  Created by 周凯 on 15/8/21.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __libmini_shmqueue_h__
#define __libmini_shmqueue_h__

#include "squeue.h"

__BEGIN_DECLS

/* ---------------                  */

/*
 * 创建共享内存队列
 */
typedef struct ShmQueue *ShmQueueT;

struct ShmQueue
{
	int     magic;
	int     shmid;
	key_t   shmkey;
	char    *path;
	SQueueT data;
};

/**
 * 初始化
 * @param shmkey KEY名
 * @param nodetotal 节点数量，如果小于等于0，则从已存在的文件中加载
 * @param nodesize 节点大小，如果小于等于0，则从已存在的文件中加载
 */
ShmQueueT SHM_QueueCreate(key_t shmkey, int nodetotal, int nodesize);

ShmQueueT SHM_QueueCreateByPath(const char *path, int nodetotal, int nodesize);

bool SHM_QueuePush(ShmQueueT queue, char *buff, int size, int *effectsize);

bool SHM_QueuePriorityPush(ShmQueueT queue, char *buff, int size, int *effectsize);

bool SHM_QueuePull(ShmQueueT queue, char *buff, int size, int *effectsize);

void SHM_QueueDestroy(ShmQueueT *queue, bool shmrm, NodeDestroyCB destroy);

/* ----------------                 */

__END_DECLS
#endif	/* ifndef __libmini_shmqueue_h__ */

