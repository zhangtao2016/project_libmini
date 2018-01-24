//
//  dstack.c
//  libmini
//
//  Created by 周凯 on 15/11/11.
//  Copyright © 2015年 zk. All rights reserved.
//

#include "adt/dynamic/dstack.h"
#include "atomic/atomic.h"
#include "slog/slog.h"
#include "except/exception.h"

struct DStackNode
{
	intptr_t                data;
	struct DStackNode       *next;
};

DStackT DStackCreate(int capacity)
{
	DStackT volatile stack = NULL;

	TRY
	{
		New(stack);
		AssertError(stack, ENOMEM);
		stack->capacity = capacity;
		stack->nodes = 0;
		stack->data = NULL;

		REFOBJ(stack);
	}
	CATCH
	{
		Free(stack);
	}
	END;

	return stack;
}

bool DStackPush(DStackT stack, intptr_t data)
{
	struct DStackNode       *node = NULL;
	struct DStackNode       *head = NULL;
	int                     nodes = 0;

	ASSERTOBJ(stack);

	/*抢占节点数*/
	while (1) {
		nodes = stack->nodes;
		return_val_if_fail(stack->capacity <1 || stack->capacity> nodes, false);

		if (likely(ATOMIC_CASB(&stack->nodes, nodes, nodes + 1))) {
			break;
		}
	}

	New(node);

	if (unlikely(!node)) {
		ATOMIC_DEC(&stack->nodes);
		return false;
	}

	node->data = data;

	while (1) {
		head = stack->data;
		node->next = head;

		if (likely(ATOMIC_CASB(&stack->data, head, node))) {
			break;
		}
	}

	return true;
}

bool DStackPop(DStackT stack, intptr_t *data)
{
	struct DStackNode *head = NULL;

	ASSERTOBJ(stack);

	return_val_if_fail(stack->nodes > 0, false);

	while (1) {
		head = stack->data;
		return_val_if_fail(head, false);

		if (likely(ATOMIC_CASB(&stack->data, head, head->next))) {
			break;
		}
	}

	ATOMIC_DEC(&stack->nodes);

	SET_POINTER(data, head->data);
	Free(head);

	return true;
}

/**
 * 使用 NodeDestroyCB 的长度参数恒等于sizeof(intptr_t)
 */
void DStackDestroy(DStackT *stack, NodeDestroyCB destroy)
{
	assert(stack);

	return_if_fail(UNREFOBJ(*stack));

	struct DStackNode *node = (*stack)->data;

	(*stack)->nodes = 0;
	(*stack)->data = NULL;

	while (node) {
		struct DStackNode *tmp = node->next;

		if (destroy) {
			TRY { destroy((void *)node->data, sizeof(node->data)); } CATCH {} END;
		}

		Free(node);
		node = tmp;
	}

	Free(*stack);
}

