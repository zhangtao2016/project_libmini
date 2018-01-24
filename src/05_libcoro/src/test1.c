//
//  test2.c
//  libmini
//  测试递归协程集群
//  Created by 周凯 on 15/11/14.
//  Copyright © 2015年 zk. All rights reserved.
//

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include "coro_cluster.h"

#define RandInt(low, high) \
		((long)((((double)random()) / ((double)RAND_MAX + 1)) * (high - low + 1)) + low)

/*
 * 随机获取小数 x ： low <= x < high
 */
#define RandReal(low, high)	\
		((((double)random()) / ((double)RAND_MAX + 1)) * (high - low) + low)

/*
 * 测试一次概率为 p(分数) 的事件
 */
#define RandChance(p) (RandReal(0, 1) < (double)(p) ? true : false)

static int mainid = 0;
static int subid = 0;

struct maintask
{};

static void maincoro_idle(struct coro_cluster *coro, void *usr);

static int maincoro_work(struct coro_cluster *coro, void *usr);

static void maincoro_end(void *usr);

static void subcoro_idle(struct coro_cluster *coro, void *usr);

static int subcoro_work(struct coro_cluster *coro, void *usr);

static void subcoro_end(void *usr);

int main()
{
		struct coro_cluster *maincoro = NULL;

		maincoro = coro_cluster_create(-1, true);
		assert(maincoro);

		coro_cluster_startup(maincoro, maincoro_idle, NULL);

		coro_cluster_destroy(maincoro, maincoro_end);
		return EXIT_SUCCESS;
}

static void maincoro_idle(struct coro_cluster *coro, void *usr)
{
		struct timeval tv = {};

		gettimeofday(&tv, NULL);

		printf("\033[1;31m" "<<< %ld.%06d : MAIN IDLE, tasks [%ld] >>>" "\033[m" "\n",
				tv.tv_sec, tv.tv_usec, coro->tasks);

		if (coro->tasks < 4) {
				/* code */
				coro_cluster_add(coro, maincoro_work, (void *)(intptr_t)mainid++, 0);
		}
}

static int maincoro_work(struct coro_cluster *coro, void *usr)
{
		int                             mid = (int)(intptr_t)usr;
		volatile int                    sid = subid++;
		volatile int                    i = 0;
		struct coro_cluster *volatile   subcoro = NULL;

		printf("\033[1;32m" "MAIN ID [%d] START" "\033[m" "\n", mid);

		subcoro = coro_cluster_create(-1, false);
		subcoro->user = (void *)(intptr_t)sid;

		if (!subcoro) {
				printf("\033[1;33m" "MAIN ID [%d] EXIT" "\033[m" "\n", mid);
				return -1;
		}

		long cnt = RandInt(2, 5);

		for (i = 0; i < cnt; i++) {
				bool flag = false;
				flag = coro_cluster_add(subcoro, subcoro_work, (void *)(intptr_t)i, 0);

				if (!flag) {
						coro_cluster_destroy(subcoro, subcoro_end);
						printf("\033[1;32m" "MAIN ID [%d] EXIT" "\033[m" "\n", mid);
						return -1;
				}
		}

		printf("\033[1;33m" "SUB ID [%d] LOOP" "\033[m" "\n", sid);
		coro_cluster_startup(subcoro, subcoro_idle, coro);

		coro_cluster_destroy(subcoro, NULL);

		printf("\033[1;33m" "MAIN ID [%d] END" "\033[m" "\n", mid);
		return 0;
}

static void maincoro_end(void *usr)
{
		int id = (int)(intptr_t)usr;

		printf("\033[1;33m" "MAIN ID [%d] DESTROY" "\033[m" "\n", id);
}

static void subcoro_idle(struct coro_cluster *coro, void *usr)
{
		int             sid = (int)(intptr_t)coro->user;
		struct timeval  tv = {};

		gettimeofday(&tv, NULL);
		printf("\033[1;31m" ">>> %ld.%06d : SUB [%d] IDLE, tasks [%ld] <<<" "\033[m" "\n",
				tv.tv_sec, tv.tv_usec, sid, coro->tasks);

		/*切换到主协程集合*/
		coro_cluster_switch(usr);
}

static int subcoro_work(struct coro_cluster *coro, void *usr)
{
		volatile int    i = 0;
		volatile long   loops = 0;
		int             id = (int)(intptr_t)usr;
		int             sid = (int)(intptr_t)coro->user;

		loops = RandInt(3, 6);

		for (i = 0; i < loops; i++) {
				if (RandChance(1.0f / 8.0f)) {
						coro_cluster_switch(coro);
				} else {
						coro_cluster_idleswitch(coro);
				}

#if 0
				usleep(500000);
#endif
				printf("\033[1;32m" "SUB ID [%d], TASK ID [%d:%ld:%d]" "\033[m" "\n", sid, id, loops, i);
		}

		return 0;
}

static void subcoro_end(void *usr)
{}
