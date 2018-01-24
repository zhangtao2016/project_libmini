//
//  squeue.c
//  libmini
//
//  Created by 周凯 on 15/9/15.
//  Copyright © 2015年 zk. All rights reserved.
//
#include <sched.h>
#include <assert.h>

#include "adt/static/squeue.h"
#include "slog/slog.h"
#include "except/exception.h"

/* --------------                       */

typedef struct SQueueNode *SQueueNodeT;

struct SQueueNode
{
	int     magic;
	int     used;
	int     _unused0;
	int     size;
	char    data[1];
};

/* ----------------                 */

/*
 * 节点在以下的状态中循环变更
 */
enum
{
	SQUEUENODE_UNUSED,	/**< 未使用*/
	SQUEUENODE_PUSHING,	/**< 正在推入*/
	SQUEUENODE_USED,	/**< 已经推入，等待推出*/
	SQUEUENODE_PULLING,	/**< 正在推出*/
};

#define calculate_node_size(size) \
	((int)(offsetof(struct SQueueNode, data) + ADJUST_SIZE((size), sizeof(long))))

#define calculate_total_size(total, size) \
	((long)((total) * calculate_node_size((size)) + sizeof(struct SQueue)))

#define calculate_node_ptr(list, i)	   \
	(assert((i) < (list)->totalnodes), \
	(SQueueNodeT)((((char *)(list)) + (list)->buffoffset) + (i) * calculate_node_size((list)->datasize)))

/* ----------------                 */
long SQueueCalculateSize(int nodetotal, int datasize)
{
	/*其中一个是哑元*/
	return calculate_total_size(nodetotal + 1, datasize);
}

SQueueT SQueueInit(char *buff, long size, int datasize, bool shared)
{
	SQueueT         queue = NULL;
	SQueueNodeT     node = NULL;
	int             i = 0;
	int             nodesize = 0;
	long            totalsize = 0;

	assert(likely(buff));
	return_val_if_fail(size > 0 && datasize > 0, NULL);

	/*至少要能容纳一个节点*/
	if (unlikely(calculate_total_size(2, datasize) > size)) {
		return NULL;
	}

	nodesize = calculate_node_size(datasize);

	queue = (SQueueT)buff;
	memset(queue, 0, sizeof(*queue));

	queue->nodes = 0;
	queue->headidx = 0;
	queue->tailidx = 0;
	queue->datasize = datasize;
	queue->totalnodes = (int)CEIL_TIMES(size - sizeof(*queue), nodesize) - 1;

	totalsize = calculate_total_size(queue->totalnodes, datasize);

	if (likely((size - totalsize) == nodesize)) {
		queue->totalnodes += 1;
	}

	queue->capacity = queue->totalnodes - 1;
	queue->buffoffset = sizeof(*queue);
	queue->totalsize = size;

	for (i = 0; i < queue->totalnodes; i++) {
		node = calculate_node_ptr(queue, i);
		node->used = SQUEUENODE_UNUSED;
		REFOBJ(node);
	}

#ifdef SQUEUE_USE_RWLOCK
	AO_SpinLockInit(&queue->rlck, true);
	AO_SpinLockInit(&queue->wlck, true);
#endif

	REFOBJ(queue);

	return queue;
}

bool SQueueCheck(char *buff, long size, int datasize, bool repair)
{
	SQueueT queue = NULL;

	assert(likely(buff));

	queue = (SQueueT)buff;

	return_val_if_fail(ISOBJ(queue), false);
	return_val_if_fail(size > 0 && queue->totalsize <= size, false);
	return_val_if_fail(queue->buffoffset == sizeof(*queue), false);
	return_val_if_fail(queue->totalnodes > 1 && queue->capacity == queue->totalnodes - 1, false);
	return_val_if_fail(queue->datasize > 0, false);
	return_val_if_fail(datasize < 1 || (datasize > 0 && queue->datasize == datasize), false);

	long expect = calculate_total_size(queue->totalnodes, queue->datasize);
	return_val_if_fail(expect <= queue->totalsize, false);

	SQueueNodeT     node = NULL;
	int             i = 0;

	for (i = 0; i < queue->totalnodes; i++) {
		node = calculate_node_ptr(queue, i);
		return_val_if_fail(ISOBJ(node), false);

		if (repair && ((node->used != SQUEUENODE_UNUSED) ||
			(node->used != SQUEUENODE_USED))) {}
	}

	if (repair) {
#ifdef SQUEUE_USE_RWLOCK
		AO_SpinLockInit(&queue->rlck, true);
		AO_SpinLockInit(&queue->wlck, true);
#endif
	}

	return true;
}

bool SQueuePush(SQueueT queue, char *data, int size, int *effectsize)
{
	SQueueNodeT node = NULL;

	ASSERTOBJ(queue);
	assert(likely(data && size > -1));

#if 1
	return_val_if_fail(size > 0 && size < queue->datasize + 1, false);
#endif

#ifdef SQUEUE_USE_RWLOCK
	AO_SpinLock(&queue->wlck);
#endif

	/*
	 * 判断容量
	 */
	while (1) {
		unsigned head = queue->headidx;
		barrier();
		unsigned next = (head + 1) % queue->totalnodes;

		/*判断容量*/
		if (unlikely((queue->capacity <= queue->nodes) || (next == queue->tailidx))) {
			x_printf(W, "no more space.");
#ifdef SQUEUE_USE_RWLOCK
			AO_SpinUnlock(&queue->wlck);
#endif
			return false;
		}

		/*原子改变头索引*/
		if (likely(ATOMIC_CASB(&queue->headidx, head, next))) {
			ATOMIC_INC(&queue->nodes);
			node = calculate_node_ptr(queue, head);
			break;
		}
	}

#ifdef SQUEUE_USE_RWLOCK
	AO_SpinUnlock(&queue->wlck);
#endif

	ASSERTOBJ(node);

	/*等待pull消费数据，并改变节点状态，以准备拷入数据*/
	while (unlikely(!ATOMIC_CASB(&node->used, SQUEUENODE_UNUSED, SQUEUENODE_PUSHING))) {
#if 0
		sched_yield();
#else
		_cpu_pause();
#endif
		x_printf(D, "waiting pull");
	}

	if (likely(data && (size > 0))) {
		node->size = MIN(queue->datasize, size);
		memcpy(node->data, data, node->size);
	} else {
		node->size = 0;
	}

#if 0
	ATOMIC_CASB(&node->used, SQUEUENODE_PUSHING, SQUEUENODE_USED);
#else
	assert(likely(ATOMIC_CASB(&node->used, SQUEUENODE_PUSHING, SQUEUENODE_USED)));
#endif
	SET_POINTER(effectsize, node->size);

	return true;
}

bool SQueuePriorityPush(SQueueT queue, char *data, int size, int *effectsize)
{
	SQueueNodeT node = NULL;

	ASSERTOBJ(queue);
	assert(likely(data && size > -1));
#if 1
	return_val_if_fail(size > 0 && size < queue->datasize + 1, false);
#endif

#ifdef SQUEUE_USE_RWLOCK
	AO_SpinLock(&queue->wlck);
	AO_SpinLock(&queue->rlck);
#endif

	while (1) {
		unsigned tail = queue->tailidx;
		barrier();
		unsigned next = (tail + queue->totalnodes - 1) % queue->totalnodes;

		if (unlikely((queue->capacity <= queue->nodes) || (queue->headidx == next))) {
			x_printf(W, "no more space.");
#ifdef SQUEUE_USE_RWLOCK
			AO_SpinUnlock(&queue->rlck);
			AO_SpinUnlock(&queue->wlck);
#endif
			return false;
		}

		if (likely(ATOMIC_CASB(&queue->tailidx, tail, next))) {
			ATOMIC_INC(&queue->nodes);
			node = calculate_node_ptr(queue, next);
			break;
		}
	}

#ifdef SQUEUE_USE_RWLOCK
	AO_SpinUnlock(&queue->rlck);
	AO_SpinUnlock(&queue->wlck);
#endif

	ASSERTOBJ(node);

	while (unlikely(!ATOMIC_CASB(&node->used, SQUEUENODE_UNUSED, SQUEUENODE_PUSHING))) {
#if 0
		sched_yield();
#else
		_cpu_pause();
#endif
		x_printf(D, "waiting pull ......");
	}

	if (likely(data && (size > 0))) {
		node->size = MIN(queue->datasize, size);
		memcpy(node->data, data, node->size);
	} else {
		node->size = 0;
	}

#if 0
	ATOMIC_CASB(&node->used, SQUEUENODE_PUSHING, SQUEUENODE_USED);
#else
	assert(likely(ATOMIC_CASB(&node->used, SQUEUENODE_PUSHING, SQUEUENODE_USED)));
#endif

	SET_POINTER(effectsize, node->size);

	return true;
}

bool SQueuePull(SQueueT queue, char *data, int size, int *effectsize)
{
	SQueueNodeT node = NULL;

	ASSERTOBJ(queue);

#if 0
	return_val_if_fail(data && size > 0, false);
#endif

#ifdef SQUEUE_USE_RWLOCK
	AO_SpinLock(&queue->rlck);
#endif

	while (1) {
		unsigned tail = queue->tailidx;
		barrier();
		unsigned next = (tail + 1) % queue->totalnodes;

		if (unlikely((queue->nodes <= 0) || (tail == queue->headidx))) {
#ifdef SQUEUE_USE_RWLOCK
			AO_SpinUnlock(&queue->rlck);
#endif
			return false;
		}

		/*原子修改尾索引*/
		if (likely(ATOMIC_CASB(&queue->tailidx, tail, next))) {
			ATOMIC_DEC(&queue->nodes);
			node = calculate_node_ptr(queue, tail);
			break;
		}
	}

#ifdef SQUEUE_USE_RWLOCK
	AO_SpinUnlock(&queue->rlck);
#endif

	ASSERTOBJ(node);

	/*等待push产生数据，并改变状态，以准备拷出数据*/
	while (unlikely(!ATOMIC_CASB(&node->used, SQUEUENODE_USED, SQUEUENODE_PULLING))) {
#if 0
		sched_yield();
#else
		_cpu_pause();
#endif
		x_printf(D, "waiting push ......");
	}

	if (likely(data && (size > 0))) {
		size = MIN(node->size, size);

		if (likely(size > 0)) {
			memcpy(data, node->data, size);
		}
	} else {
		size = 0;
	}

#if 0
	ATOMIC_CASB(&node->used, SQUEUENODE_PULLING, SQUEUENODE_UNUSED);
#else
	assert(likely(ATOMIC_CASB(&node->used, SQUEUENODE_PULLING, SQUEUENODE_UNUSED)));
#endif
	SET_POINTER(effectsize, size);

	return true;
}

