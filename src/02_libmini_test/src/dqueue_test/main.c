//
//  main.c
//  libmini
//
//  Created by 周凯 on 15/11/12.
//  Copyright © 2015年 zk. All rights reserved.
//

#include <stdio.h>
#include "libmini.h"

// #define NON_THREAD_TEST

static AO_T     counter = 0;
static AO_T     specify = -1;

static void *qpush(void *arg);

static void *qpull(void *arg);

static void *qprioritypush(void *arg);

int main(int argc, char **argv)
{
	DQueueT queue = NULL;

	queue = DQueueCreate(200);
	assert(queue);

#ifndef NON_THREAD_TEST
	pthread_t       tid[16] = {};
	int             i = 0;
	int             j = 10;

	for (i = 0; i < j; i++) {
		pthread_create(&tid[i], NULL, qpush, queue);
	}

	//        sleep(1);

	for (; i < j + 1; i++) {
		pthread_create(&tid[i], NULL, qprioritypush, queue);
	}

	//        sleep(1);

	for (; i < DIM(tid); i++) {
		pthread_create(&tid[i], NULL, qpull, queue);
	}

	for (i = 0; i < DIM(tid); i++) {
		pthread_join(tid[i], NULL);
	}

#else
	qpush(queue);
	qprioritypush(queue);
	qpull(queue);

	qpush(queue);
	qprioritypush(queue);
	qpull(queue);
#endif	/* ifndef NON_THREAD_TEST */
	return EXIT_SUCCESS;
}

static void *qpush(void *arg)
{
	DQueueT queue = (DQueueT)arg;
	int     i = 10;

	ASSERTOBJ(queue);

	BindCPUCore(-1);

	while (i > 0) {
		AO_T cnt = ATOMIC_F_ADD(&counter, 1);

		if (DQueuePush(queue, cnt)) {
			fprintf(stderr,
				LOG_E_COLOR
				"[%010ld]->PUSH"
				PALETTE_NULL
				"\n"
				, cnt);
			futex_wake(&queue->nodes, -1);
#ifdef NON_THREAD_TEST
			i--;
#endif
		} else {
			futex_wait(&queue->nodes, queue->capacity, 100);
		}

		//                usleep(5000);
	}

	return NULL;
}

static void *qpull(void *arg)
{
	DQueueT queue = (DQueueT)arg;
	int     i = 10;

	ASSERTOBJ(queue);

	BindCPUCore(-1);

	while (i > 0) {
		intptr_t cnt = 0;

		if (DQueuePull(queue, &cnt)) {
			if (likely(cnt >= 0)) {
				fprintf(stderr,
					LOG_W_COLOR
					"PULL->[%010ld]"
					PALETTE_NULL
					"\n", cnt);
			} else {
				fprintf(stderr,
					LOG_F_COLOR
					"PULL->[%010ld]"
					PALETTE_NULL
					"\n", cnt);
			}

			futex_wake(&queue->nodes, -1);
#ifdef NON_THREAD_TEST
			i--;
#endif
		} else {
			futex_wait(&queue->nodes, 0, 100);
		}

		// usleep(10500);
	}

	return NULL;
}

static void *qprioritypush(void *arg)
{
	DQueueT queue = (DQueueT)arg;
	int     i = 2;

	ASSERTOBJ(queue);

	BindCPUCore(-1);

	while (i > 0) {
		intptr_t cnt = ATOMIC_F_SUB(&specify, 1);

		if (DQueuePriorityPush(queue, cnt)) {
			fprintf(stderr,
				LOG_S_COLOR
				"[%010ld]->PPUSH"
				PALETTE_NULL
				"\n"
				, cnt);
			futex_wake(&queue->nodes, -1);
#ifdef NON_THREAD_TEST
			i--;
#endif
		} else {
			futex_wait(&queue->nodes, queue->capacity, 100);
		}

		usleep(5000);
	}

	return NULL;
}

