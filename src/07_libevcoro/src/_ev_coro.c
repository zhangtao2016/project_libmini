//
//  _ev_coro_task.c
//  libmini
//
//  Created by 周凯 on 15/12/1.
//  Copyright © 2015年 zk. All rights reserved.
//
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include "libevcoro/_ev_coro.h"
#include "libcoro/coro.h"

/*包裹函数入口函数*/
static void _evcoro_run(void *arg);


struct ev_coro_t *evcoro_list_new(struct evcoro_scheduler *scheduler)
{
	struct ev_coro_t *list = NULL;

	list = calloc(1, sizeof(*list));

	if (unlikely(!list)) {
		return NULL;
	}

	list->head = calloc(1, sizeof(*list->head));

	if (unlikely(!list->head)) {
		free(list);
		return NULL;
	}

	/*初始化主协程对象*/
	list->cnt = 0;
	list->head->main = NULL;
	list->head->scheduler = scheduler;
	list->head->stack.sptr = NULL;
	list->head->stack.ssze = 0;
	/*初始化环形结构*/
	list->head->next = list->head;
	list->head->prev = list->head;
	/*初始化协程上下文，用于调度器运行*/
	coro_create(&list->head->ctx, NULL, NULL, NULL, 0);
	/*当前运行上下文指向新创建的协程对象*/
	list->cursor = list->head;

	return list;
}

void evcoro_list_destroy(struct ev_coro_t *list, evcoro_destroycb destroy)
{
	if (unlikely(!list)) {
		return;
	}

	struct ev_coro *coro = NULL;
	/*遍历链表，销毁子协程对象*/
	while (evcoro_list_counter(list) > 0) {
		coro = evcoro_list_cursor(list);

		if (_evcoro_subctx(coro)) {
			/*从链表中断开关联*/
			evcoro_list_pull(list);

			if (likely(destroy)) {
				destroy(coro->usr);
			}

			_evcoro_destroy(coro, false);
		}

		evcoro_list_movenext(list);
	}
	/*销毁主协程*/
	_evcoro_destroy(list->head, false);
	free(list);
}

struct ev_coro_t *evcoro_stack_new()
{
	struct ev_coro_t *list = NULL;

	list = calloc(1, sizeof(*list));

	if (unlikely(!list)) {
		return NULL;
	}
	/*初始化栈管理器，栈管理器没有主协程*/
	list->cursor = NULL;
	list->head = NULL;
	list->cnt = 0;

	return list;
}

void evcoro_stack_destroy(struct ev_coro_t *stack)
{
	if (unlikely(!stack)) {
		return;
	}

	struct ev_coro *coro = NULL;
	/*遍历栈依次销毁栈对象*/
	while (evcoro_list_counter(stack) > 0) {
		coro = evcoro_list_cursor(stack);
		evcoro_list_movenext(stack);
		_evcoro_destroy(coro, false);
		evcoro_list_counter(stack)--;
	}

	free(stack);
}

struct ev_coro *_evcoro_create(struct evcoro_scheduler *scheduler,
	evcoro_taskcb call, void *usr, size_t ss)
{
	bool            isnew = false;
	/*获取缓存协程对象的头指针*/
	struct ev_coro  *coro = evcoro_list_cursor(scheduler->usable);

	if (likely(coro && (coro->stss >= ss))) {
		/*如果满足条件，则从栈管理器中弹出*/
		assert(likely(evcoro_list_counter(scheduler->usable) > 0));
		evcoro_list_movenext(scheduler->usable);
		evcoro_list_counter(scheduler->usable)--;
	} else {
		/*创建一个新的对象，并初始化上下文空间*/
		coro = calloc(1, sizeof(*coro));

		if (unlikely(!coro)) {
			errno = ENOMEM;
			return NULL;
		}

		isnew = true;
		int flag = false;
		/*创建栈空间*/
		flag = coro_stack_alloc(&coro->stack, (unsigned)ss);

		if (unlikely(flag == 0)) {
			free(coro);
			return NULL;
		}

		coro->stss = ss;
	}

	/*初始化对象的状态和入口函数（参数）等*/
	coro->main = &evcoro_list_head(scheduler->working)->ctx;
	coro->stat = EVCORO_STAT_INIT;
	coro->call = call;
	coro->usr = usr;
	coro->scheduler = scheduler;
	coro->cleanup = NULL;

	if (unlikely(isnew)) {
		/*如果是新创建的协程对象，则创建上下文*/
		coro_create(&coro->ctx, _evcoro_run, coro, coro->stack.sptr, coro->stack.ssze);
	}

	/*切换到_evcoro_run()包裹函数的内部并切换回到此处，等待下次调度器调度切换*/
	coro_transfer(&evcoro_list_cursor(scheduler->working)->ctx, &coro->ctx);

	return coro;
}

void _evcoro_destroy(struct ev_coro *coro, bool cache)
{
	struct evcoro_scheduler *scheduler = NULL;

	/*弹出所有的清理数据，并执行清理函数*/
	while (unlikely(_evcoro_cleanup_pop(coro, true))) {}

	scheduler = coro->scheduler;

	if (likely(cache && (evcoro_list_counter(scheduler->usable) < scheduler->bolt))) {
		/*如果没有达到缓存上限，则压栈缓存*/
		coro->next = evcoro_list_cursor(scheduler->usable);
		evcoro_list_cursor(scheduler->usable) = coro;
		evcoro_list_counter(scheduler->usable)++;
	} else {
		/*否则销毁协程上下文和上下文空间，顺序不能颠倒*/
		coro_destroy(&coro->ctx);
		coro_stack_free(&coro->stack);
		free(coro);
	}
}

static void _evcoro_run(void *arg)
{
	struct ev_coro          *coro = arg;
	struct evcoro_scheduler *scheduler = NULL;

	assert(likely(arg));
	do {
		scheduler = coro->scheduler;
		assert(likely(scheduler));
		assert(_evcoro_subctx(coro));
		assert(_evcoro_stat_init(coro));
		coro->stat = EVCORO_STAT_RUNNING;

		assert(likely(coro->call));
		/*切回到启动协程中，等待下次调用者切换*/
		coro_transfer(&coro->ctx, &evcoro_list_cursor(scheduler->working)->ctx);
		coro->call(scheduler, coro->usr);
		coro->stat = EVCORO_STAT_EXITED;
		/*切回主协同*/
		coro_transfer(&coro->ctx, coro->main);
		/*等待再次利用或被释放*/
	} while (1);
}

