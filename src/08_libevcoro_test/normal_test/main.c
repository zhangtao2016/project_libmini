//
//  test.c
//  libmini
//
//  Created by 周凯 on 15/12/2.
//  Copyright © 2015年 zk. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "evcoro_scheduler.h"

#define RandInt(low, high) \
	((long)((((double)random()) / ((double)RAND_MAX + 1)) * (high - low + 1)) + low)

/*
 * 随机获取小数 x ： low <= x < high
 */
#define RandReal(low, high) \
	((((double)random()) / ((double)RAND_MAX + 1)) * (high - low) + low)

/*
 * 测试一次概率为 p(分数) 的事件
 */
#define RandChance(p) (RandReal(0, 1) < (double)(p) ? true : false)
static int task_id = 0;

struct task
{
	int     id;
	int     cycle;
	int     rc;
	bool    m;
};

void work(struct evcoro_scheduler *scheduler, void *usr)
{
	struct task     *arg = usr;
	int             i = 0;

	printf("\033[1;33m" "ID %d start ..." "\033[m" "\n", arg->id);

	if (arg->cycle < 1) {
		printf("\033[1;31m" "ID %d exit ..." "\033[m" "\n", arg->id);
		return;
	}

	for (i = 0; i < arg->cycle; i++) {
		evcoro_fastswitch(scheduler);
		printf("\033[1;32m" "ID %d : %d" "\033[m" "\n", arg->id, i + 1);
	}

	printf("\033[1;33m" "ID %d end ..." "\033[m" "\n", arg->id);

	free(usr);
}

void idle(struct evcoro_scheduler *scheduler, void *usr)
{
	struct timeval tv = {};

	gettimeofday(&tv, NULL);

	printf("\033[1;31m" "<<< %ld.%06ld : IDLE, tasks [%d] >>>" "\033[m" "\n",
		tv.tv_sec, tv.tv_usec, evcoro_workings(scheduler));

	if (unlikely(evcoro_workings(scheduler) < 1)) {
		evcoro_stop(scheduler);
	}
}

int main(int argc, char **argv)
{
	struct evcoro_scheduler *scheduler = NULL;

	scheduler = evcoro_create(-1);
	assert(scheduler);

	int i = 0;

	for (i = 0; i < 10; i++) {
		struct task *task = calloc(1, sizeof(*task));
		assert(task);
		task->cycle = (int)RandInt(6, 10);
		task->id = i;
		bool flag = evcoro_push(scheduler, work, task, 0);
		assert(flag);
	}

	evcoro_loop(scheduler, idle, NULL);

	evcoro_destroy(scheduler, free);

	return EXIT_SUCCESS;
}

