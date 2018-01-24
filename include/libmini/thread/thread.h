//
//  thread.h
//  minilib
//
//  Created by 周凯 on 15/8/24.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __minilib__thread__
#define __minilib__thread__

#include <pthread.h>
#include "../utils.h"

__BEGIN_DECLS

/* ----------------------------                 */

/*
 * 挂起线程
 */
#pragma pack(push, 8)
struct ThreadSuspend
{
	int             magic;
	int             count;
	bool            reset;
	pthread_mutex_t lock;
	pthread_cond_t  cond;
};
#pragma pack(pop)

#define NULLOBJ_THREADSUSPEND	     \
	{ OBJMAGIC, 0, false,	     \
	  PTHREAD_MUTEX_INITIALIZER, \
	  PTHREAD_COND_INITIALIZER }

/**
 * “子”线程挂起
 */
void ThreadSuspendStart(struct ThreadSuspend *cond);

/*
 * “父”线程等待一定数量的“子”线程挂起
 */
void ThreadSuspendWait(struct ThreadSuspend *cond, int count);

/**
 * “父”线程恢复挂起的“子”线程
 */
void ThreadSuspendEnd(struct ThreadSuspend *cond);

/*
 * 初始化、销毁动态分配的挂起条件
 */
bool ThreadSuspendInit(struct ThreadSuspend *cond);

void ThreadSuspendFinally(struct ThreadSuspend *cond);

/* ----------------------------                 */
__END_DECLS
#endif	/* defined(__minilib__thread__) */

