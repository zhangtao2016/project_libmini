//
//  thread.c
//  minilib
//
//  Created by 周凯 on 15/8/24.
//  Copyright (c) 2015年 zk. All rights reserved.
//
#include "thread/thread.h"
#include "atomic/atomic.h"
#include "slog/slog.h"
#include "time/time.h"
#include "except/exception.h"

int create_detach_pthread(void *(*func)(void *), void *data)
{
	int             flag = 0;
	pthread_t       tid;
	pthread_attr_t  attr = {};

	if (!func) {
		return 0;
	}

	pthread_attr_init(&attr);

	pthread_attr_setdetachstate(&attr, 1);

	flag = pthread_create(&tid, &attr, func, data);

	if (unlikely(flag)) {
		x_printf(S, "create deattch thread fail %s.", strerror(flag));
	}

	pthread_attr_destroy(&attr);

	return flag;
}

/* ----------------------------                 */

void ThreadSuspendStart(struct ThreadSuspend *cond)
{
	ASSERTOBJ(cond);

	pthread_mutex_lock(&cond->lock);

	cond->count++;

	pthread_cond_broadcast(&cond->cond);

	x_printf(W, "thread (%10ld) has been suspending.", GetThreadID());

	while (unlikely(!cond->reset)) {
		struct timespec tm = {};
		TM_FillAbsolute(&tm, 50);
		pthread_cond_timedwait(&cond->cond, &cond->lock, &tm);
	}

	cond->count--;

	x_printf(W, "thread (%10ld) has been continuning.", GetThreadID());

	pthread_cond_broadcast(&cond->cond);

	pthread_mutex_unlock(&cond->lock);
}

void ThreadSuspendWait(struct ThreadSuspend *cond, int count)
{
	ASSERTOBJ(cond);

	pthread_mutex_lock(&cond->lock);

	while (unlikely(cond->count != count)) {
		struct timespec tm = {};
		TM_FillAbsolute(&tm, 50);
		pthread_cond_timedwait(&cond->cond, &cond->lock, &tm);
	}

	pthread_mutex_unlock(&cond->lock);
}

void ThreadSuspendEnd(struct ThreadSuspend *cond)
{
	ASSERTOBJ(cond);

	pthread_mutex_lock(&cond->lock);

	cond->reset = true;
	pthread_cond_broadcast(&cond->cond);

	while (unlikely(cond->count > 0)) {
		struct timespec tm = {};
		TM_FillAbsolute(&tm, 50);
		pthread_cond_timedwait(&cond->cond, &cond->lock, &tm);
	}

	pthread_mutex_unlock(&cond->lock);
}

bool ThreadSuspendInit(struct ThreadSuspend *cond)
{
	int flag = 0;

	assert(cond);

	cond->reset = false;
	cond->count = 0;

	flag = pthread_mutex_init(&cond->lock, NULL);

	if (unlikely(flag > 0)) {
		x_printf(E, "pthread_mutex_init() fail : %s", x_strerror(flag));
		return false;
	}

	flag = pthread_cond_init(&cond->cond, NULL);

	if (unlikely(flag > 0)) {
		x_printf(E, "pthread_cond_init() fail : %s", x_strerror(flag));

		pthread_mutex_destroy(&cond->lock);

		return false;
	}

	REFOBJ(cond);
	return true;
}

void ThreadSuspendFinally(struct ThreadSuspend *cond)
{
	UNREFOBJ(cond);
	pthread_mutex_destroy(&cond->lock);
	pthread_cond_destroy(&cond->cond);
}

