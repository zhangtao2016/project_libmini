//
//  main.c
//  libmini
//
//  Created by 周凯 on 15/11/11.
//  Copyright © 2015年 zk. All rights reserved.
//

#include <stdio.h>
#include "libmini.h"

static AO_T conter = 0;

void *pop(void *);

void *push(void *);

int main()
{
	volatile DStackT stack = NULL;

	pthread_t ptid[16];

	TRY
	{
		stack = DStackCreate(10000);

		int     i = 0;
		int     j = DIM(ptid) - 10;

		for (i = 0; i < j; i++) {
			pthread_create(&ptid[i], NULL, pop, stack);
		}

		for (; i < DIM(ptid); i++) {
			pthread_create(&ptid[i], NULL, push, stack);
		}
	}
	FINALLY
	{
		int i = 0;

		for (i = 0; i < DIM(ptid); i++) {
			pthread_join(ptid[i], NULL);
		}

		DStackDestroy((DStackT *)&stack, NULL);
	}
	END;

	return EXIT_SUCCESS;
}

void *pop(void *usr)
{
	DStackT stack = usr;

	ASSERTOBJ(stack);

	intptr_t val = 0;

	while (1) {
		if (likely(DStackPop(stack, &val))) {
			printf(LOG_W_COLOR
				"%ld-->POP"
				PALETTE_NULL
				"\n", val);

			if (unlikely(stack->nodes + 5 > stack->capacity)) {
				futex_wake(&stack->nodes, -1);
			}
		} else {
			futex_wait(&stack->nodes, 0, 10);
		}
	}

	return NULL;
}

void *push(void *usr)
{
	DStackT stack = usr;

	ASSERTOBJ(stack);

	intptr_t val = 0;

	while (1) {
		val = RandInt(100, 10000);

		if (likely(DStackPush(stack, val))) {
			printf(LOG_I_COLOR
				"PUSH->%ld"
				PALETTE_NULL
				"\n", val);

			if (unlikely(stack->nodes < 5)) {
				futex_wake(&stack->nodes, -1);
			}
		} else {
			futex_wait(&stack->nodes, 0, 10);
		}
	}

	return NULL;
}

