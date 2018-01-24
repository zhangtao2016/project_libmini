//
//  main.c
//  libmini
//
//  Created by 周凯 on 15/10/2.
//  Copyright © 2015年 zk. All rights reserved.
//

#include "libmini.h"

// #define NON_THREAD_TEST

static AO_T     counter = 0;
static AO_T     specify = -1;

static void *qpush(void *arg);

static void *qpull(void *arg);

static void *qprioritypush(void *arg);

int main(int argc, char **argv)
{
	char    *qdata = NULL;
	long    qsize = 0;

	qsize = SQueueCalculateSize(20000, 32);

	NewArray0(qsize, qdata);
	AssertError(qdata, ENOMEM);

	SQueueT queue = NULL;

	queue = SQueueInit(qdata, qsize, 32, false);
	assert(queue);
#ifndef NON_THREAD_TEST
	pthread_t       tid[16] = {};
	int             i = 0;
	int             j = 12;

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
	SQueueT queue = (SQueueT)arg;
	int     i = 10;

	ASSERTOBJ(queue);

	BindCPUCore(-1);

	while (i > 0) {
		if (SQueuePush(queue, (char *)&counter, sizeof(counter), NULL)) {
			fprintf(stderr,
				LOG_E_COLOR
				"[%010ld]->PUSH"
				PALETTE_NULL
				"\n"
				, counter);
			ATOMIC_INC(&counter);
			futex_wake(&queue->nodes, -1);
#ifdef NON_THREAD_TEST
			i--;
#endif
		} else {
			futex_wait(&queue->nodes, queue->capacity, 1000);
		}
	}

	return NULL;
}

static void *qpull(void *arg)
{
	SQueueT queue = (SQueueT)arg;
	int     i = 10;

	ASSERTOBJ(queue);

	BindCPUCore(-1);

	while (i > 0) {
		AO_T counter = 0;

		if (SQueuePull(queue, (char *)&counter, sizeof(counter), NULL)) {
			if (counter >= 0) {
				fprintf(stderr,
					LOG_W_COLOR
					"PULL->[%010ld]"
					PALETTE_NULL
					"\n", counter);
			} else {
				fprintf(stderr,
					LOG_F_COLOR
					"PULL->[%010ld]"
					PALETTE_NULL
					"\n", counter);
			}

			futex_wake(&queue->nodes, -1);
#ifdef NON_THREAD_TEST
			i--;
#endif
		} else {
			futex_wait(&queue->nodes, 0, 1000);
		}
	}

	return NULL;
}

static void *qprioritypush(void *arg)
{
	SQueueT queue = (SQueueT)arg;
	// AO_T    pvalue = -1;
	int i = 2;

	ASSERTOBJ(queue);

	BindCPUCore(-1);

	while (i > 0) {
		if (SQueuePriorityPush(queue, (char *)&specify, sizeof(specify), NULL)) {
			fprintf(stderr,
				LOG_S_COLOR
				"[%010ld]->PPUSH"
				PALETTE_NULL
				"\n"
				, specify);
			ATOMIC_DEC(&specify);
			futex_wake(&queue->nodes, -1);
#ifdef NON_THREAD_TEST
			i--;
#endif
		} else {
			futex_wait(&queue->nodes, queue->capacity, 1000);
		}

		usleep(1000);
	}

	return NULL;
}

