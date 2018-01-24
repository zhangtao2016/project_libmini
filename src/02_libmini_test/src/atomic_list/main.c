//
//  main.c
//  libmini
//
//  Created by 周凯 on 15/9/18.
//  Copyright © 2015年 zk. All rights reserved.
//

#include "libmini.h"

struct alist
{
	int             i;
	struct alist    *next;
};

static AO_T             g_counter = 0;
static struct alist     *g_alist = NULL;
static inline void      push_alist(int val);

static void *work(void *usr);

int main()
{
	int                     i = 0;
	pthread_t               tid[4] = {};
	struct alist            *next = NULL;
	struct ThreadSuspend    cond = NULLOBJ_THREADSUSPEND;

	for (i = 0; i < DIM(tid); i++) {
		pthread_create(&tid[i], NULL, work, &cond);
	}

	ThreadSuspendWait(&cond, DIM(tid));

	ThreadSuspendEnd(&cond);

	for (i = 0; i < DIM(tid); i++) {
		pthread_join(tid[i], NULL);
	}

	//        printf("\n");

	for (i = 1, next = g_alist; next; next = next->next, i++) {
		printf("-> %04d", next->i);

		if ((i % 5) == 0) {
			printf("\n");
		}
	}

	printf("\n");

	if (unlikely((i - 1) % DIM(tid))) {
		x_perror("TEST FAILURE ...");
	} else {
		x_printf(D, "TEST SUCCESS ...");
	}

	return EXIT_SUCCESS;
}

static inline void push_alist(int val)
{
	struct alist    *newnode = NULL;
	struct alist    **next = &g_alist;

	New(newnode);
	AssertError(newnode, ENOMEM);

	newnode->i = val;

	barrier();

	while (!ATOMIC_CASB(next, NULL, newnode)) {
		barrier();
		next = &(*next)->next;
	}
}

static void *work(void *usr)
{
	struct ThreadSuspend    *cond = (struct ThreadSuspend *)usr;
	int                     i = 20;

	ThreadSuspendStart(cond);

	while (i-- > 0) {
		int val = (int)ATOMIC_F_ADD(&g_counter, 1);
		//                printf("-> %d", val);
		sched_yield();
		sched_yield();
		push_alist(val);
	}

	return NULL;
}

