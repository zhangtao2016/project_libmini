//
//  dqueue.c
//  libmini
//
//  Created by 周凯 on 15/11/12.
//  Copyright © 2015年 zk. All rights reserved.
//

#include "adt/dynamic/dqueue.h"
#include "atomic/atomic.h"
#include "slog/slog.h"
#include "except/exception.h"

/*
 * head
 * [next]->[next]->[next]...[next]
 *							tail
 * push : modify tail node, add node
 * pop : modify head node, delete node
 * priority-push : modify head node, add node
 */

struct DQueueNode
{
	intptr_t                data;
	struct DQueueNode       *next;
};

DQueueT DQueueCreate(int capacity)
{
	volatile DQueueT        queue = NULL;
	volatile AO_SpinLockT   *lock = NULL;

	TRY
	{
		New(queue);
		AssertError(queue, ENOMEM);

		New(lock);
		AssertError(lock, ENOMEM);

		AO_SpinLockInit((AO_SpinLockT *)lock, false);

		queue->lock = (void *)lock;
		queue->capacity = capacity;
		queue->head = NULL;
		queue->tail = NULL;
		queue->nodes = 0;

		REFOBJ(queue);
	}
	CATCH
	{
		Free(lock);
		Free(queue);
	}
	END;

	return queue;
}

bool DQueuePush(DQueueT queue, intptr_t data)
{
	bool ret = false;

	ASSERTOBJ(queue);

	AO_SpinLock(queue->lock);

	if (likely((queue->capacity < 0) || (queue->capacity > queue->nodes))) {
		struct DQueueNode *node = NULL;
		New(node);

		if (likely(node)) {
			node->next = NULL;
			node->data = data;

			if (unlikely(!queue->tail)) {
				/*第一个*/
				queue->head = node;
			} else {
				((struct DQueueNode *)queue->tail)->next = node;
			}

			/*修改tail*/
			queue->tail = node;
			queue->nodes++;
			ret = true;
		}
	} else {
		x_printf(W, "no more space");
	}

	AO_SpinUnlock(queue->lock);

	return ret;
}

bool DQueuePriorityPush(DQueueT queue, intptr_t data)
{
	bool ret = false;

	ASSERTOBJ(queue);

	AO_SpinLock(queue->lock);

	if (likely((queue->capacity < 0) || (queue->capacity > queue->nodes))) {
		struct DQueueNode *node = NULL;
		New(node);

		if (likely(node)) {
			node->data = data;

			if (unlikely(!queue->head)) {
				/*第一个*/
				queue->tail = node;
			}

			node->next = queue->head;
			queue->head = node;
			queue->nodes++;
			ret = true;
		}
	} else {
		x_printf(W, "no more space");
	}

	AO_SpinUnlock(queue->lock);

	return ret;
}

bool DQueuePull(DQueueT queue, intptr_t *data)
{
	bool ret = false;

	ASSERTOBJ(queue);

	AO_SpinLock(queue->lock);

	if (likely(queue->nodes > 0)) {
		struct DQueueNode *head = queue->head;

		if (unlikely(head == queue->tail)) {
			/*最后一个*/
			queue->tail = NULL;
		}

		queue->head = head->next;

		SET_POINTER(data, head->data);
		Free(head);
		queue->nodes--;
		ret = true;
	}

	AO_SpinUnlock(queue->lock);

	return ret;
}

/**
 * 使用 NodeDestroyCB 的长度参数恒等于sizeof(intptr_t)
 */
void DQueueDestroy(DQueueT *queue, NodeDestroyCB destroy)
{
	assert(queue);

	return_if_fail(UNREFOBJ(*queue));

	AO_SpinLock((*queue)->lock);

	struct DQueueNode       *next = (*queue)->head;
	int                     i = 0;

	for (i = 0; i < (*queue)->nodes; i++) {
		struct DQueueNode *tmp = next->next;

		if (destroy) {
			TRY
			{
				destroy((void *)next->data, sizeof(next->data));
			}
			CATCH
			{}
			END;
		}

		Free(next);
		next = tmp;
	}

	Free((*queue)->lock);
	Free(*queue);
}

