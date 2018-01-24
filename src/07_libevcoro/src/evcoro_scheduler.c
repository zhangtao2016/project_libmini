//
//  ev_coro.c
//  libmini
//
//  Created by 周凯 on 15/12/1.
//  Copyright © 2015年 zk. All rights reserved.
//
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <errno.h>
#include "libevcoro/_ev_coro.h"
#include "libevcoro/evcoro_scheduler.h"
#include "libcoro/coro.h"

/*重用协程闩值*/
#define EVCORO_BOLT_MINSIZE     (10)
#define EVCORO_BOLT_MAXSIZE     (1000)
/*最小栈大小*/
#ifndef EVCORO_STACK_SIZE
  #define EVCORO_STACK_SIZE     (102400U)
#endif
/*描述符最大超时时间，秒*/
#define EVCORO_IO_MAXTO         (600.0f)
/*描述符最小超时时间，秒*/
#define EVCORO_IO_MINTO         (0.001f)
/*计时器最大超时时间，秒*/
#define EVCORO_TIMER_MAXTO      (3600.0f * 24 * 366)

struct evcoro_event_t
{
	int             type;	/*libev事件类型*/
	struct ev_timer timer; /*计时器libev事件*/
	struct ev_io    io; /*描述符libev事件*/
	struct ev_coro  *coro;/*自定义协程事件*/
	struct ev_loop  *loop;/*libev事件侦听器*/
	bool            flag; /*协程状态就绪或超时标志*/
};

/**切换核心函数*/
static inline void _scheduler_switch(struct evcoro_scheduler *scheduler, bool isidle);
/**检查是否是调度器创建者线程本身在调度协程*/
static inline void _scheduler_checkowner(struct evcoro_scheduler *scheduler);
/**协程事件结束处理核心函数*/
static inline void _scheduler_event_overdeal(struct evcoro_event_t *event);
/*协程计时器事件包裹函数*/
static void _scheduler_event_to(struct ev_loop *loop, struct ev_timer *timer, int event);
/*协程描述符读写事件包裹函数*/
static void _scheduler_event_io(struct ev_loop *loop, struct ev_io *io, int event);

struct evcoro_scheduler *evcoro_create(int bolt)
{
	struct evcoro_scheduler *scheduler = NULL;

	scheduler = calloc(1, sizeof(*scheduler));

	if (unlikely(!scheduler)) {
		goto error;
	}

	int flag = EVFLAG_NOENV;
#ifdef __LINUX__
	flag |= EVBACKEND_EPOLL;
#else
	flag |= EVBACKEND_SELECT;
#endif

	scheduler->listener = ev_loop_new(flag);

	if (unlikely(!scheduler->listener)) {
		goto error;
	}

	scheduler->working = evcoro_list_new(scheduler);

	if (unlikely(!scheduler->working)) {
		goto error;
	}

	/*分配一个永远也不使用的main类型的哑元作为suspending的起始标记*/
	scheduler->suspending = evcoro_list_new(scheduler);

	if (unlikely(!scheduler->suspending)) {
		goto error;
	}

	scheduler->usable = evcoro_stack_new();

	if (unlikely(!scheduler->usable)) {
		goto error;
	}

	scheduler->stat = EVCOROSCHE_STAT_INIT;

	/*存储的协程不能大于或小于设定值*/
	scheduler->bolt = bolt < EVCORO_BOLT_MINSIZE ? EVCORO_BOLT_MINSIZE : bolt;
	scheduler->bolt = scheduler->bolt > EVCORO_BOLT_MAXSIZE ? EVCORO_BOLT_MAXSIZE : scheduler->bolt;

	/*线程id*/
#if defined(SYS_thread_selfid)
	scheduler->ownerid = syscall(SYS_thread_selfid);
#elif defined(SYS_gettid)
	scheduler->ownerid = syscall(SYS_gettid);
#endif

	return scheduler;

error:
	errno = ENOMEM;

	if (likely(scheduler)) {
		if (scheduler->listener) {
			ev_loop_destroy(scheduler->listener);
		}

		evcoro_list_destroy(scheduler->working, NULL);
		evcoro_list_destroy(scheduler->suspending, NULL);
		evcoro_stack_destroy(scheduler->usable);
		free(scheduler);
	}

	return NULL;
}

void evcoro_destroy(struct evcoro_scheduler *scheduler, evcoro_destroycb destroy)
{
	_scheduler_checkowner(scheduler);
	evcoro_list_destroy(scheduler->working, destroy);
	evcoro_list_destroy(scheduler->suspending, destroy);
	evcoro_stack_destroy(scheduler->usable);
	ev_loop_destroy(scheduler->listener);
	free(scheduler);
}

/*当前工作协程数*/
int evcoro_workings(struct evcoro_scheduler *scheduler)
{
	assert(likely(scheduler));
	return evcoro_list_counter(scheduler->working);
}

/*当前空闲协程数*/
int evcoro_suspendings(struct evcoro_scheduler *scheduler)
{
	assert(likely(scheduler));
	return evcoro_list_counter(scheduler->suspending);
}

void evcoro_loop(struct evcoro_scheduler *scheduler, evcoro_taskcb idle, void *usr)
{
	_scheduler_checkowner(scheduler);

	struct ev_coro_t        *working = scheduler->working;
	struct ev_coro_t        *suspending = scheduler->suspending;
	struct ev_coro          *head = evcoro_list_head(working);
	struct ev_coro          *cursor = NULL;

	scheduler->stat = EVCOROSCHE_STAT_LOOP;

	/*没有可执行或可侦听的任务，则返回*/
	while (likely((idle ||
		evcoro_list_counter(working) ||
		evcoro_list_counter(suspending)) &&
		evcoro_isactive(scheduler))) {
		/*当前调度队列中的光标位移到下一个，准备切换到其中的协程上下文*/
		evcoro_list_movenext(working);
		cursor = evcoro_list_cursor(working);
		/*如果此上下文是主协程，即当前运行的上下文，则进行调度器控制处理流程*/
		if (_evcoro_mainctx(cursor)) {
			/*
			 * 至此表明已循环一次
			 * 先运行idle，再检测是否有就绪的协程
			 */
			if (likely(idle)) {
				idle(scheduler, usr);
			}

			if (unlikely(evcoro_list_counter(suspending) > 0)) {
				/*ev_loop()中的循环仅一次*/
				int flag = EVRUN_ONCE;
				/*是否阻塞直到有事件触发*/
#if 1
				if (likely(evcoro_list_counter(working) > 0)) {
					/*如果有可以调度的协程，则不阻塞调用ev_loop()*/
					flag |= EVRUN_NOWAIT;
				}

#else
				flag |= EVRUN_NOWAIT;
#endif
				ev_loop(scheduler->listener, flag);
			}
		} else {
			assert(_evcoro_stat_running(cursor));
			/*切入到指定协程，作为当前运行协程*/
			coro_transfer(&head->ctx, &cursor->ctx);
			/*运行到此处，表明有协程运行完毕或主动切出（交出控制权）*/
			cursor = evcoro_list_cursor(working);

			/*如果当前运行协程是子协程，则判断是否正常运行完毕退出，还是主动切出的*/
			if (_evcoro_subctx(cursor)) {
				if (_evcoro_stat_exited(cursor)) {
					/*如果当前工作协程退出，则弹出调度队列*/
					cursor = evcoro_list_pull(working);
					/*并销毁协程*/
					_evcoro_destroy(cursor, true);
				}
			}
		}
	}
}

bool evcoro_push(struct evcoro_scheduler *scheduler, evcoro_taskcb call, void *usr, size_t ss)
{
	struct ev_coro *coro = NULL;

	assert(likely(call));
	_scheduler_checkowner(scheduler);

	ss = ss < EVCORO_STACK_SIZE ? EVCORO_STACK_SIZE : ss;
	coro = _evcoro_create(scheduler, call, usr, ss);

	if (unlikely(!coro)) {
		return false;
	}
	/*加入调度队列*/
	evcoro_list_push(scheduler->working, coro);

	return true;
}

void evcoro_fastswitch(struct evcoro_scheduler *scheduler)
{
	_scheduler_checkowner(scheduler);
	_scheduler_switch(scheduler, false);
}

bool evcoro_idleswitch(struct evcoro_scheduler *scheduler, const union evcoro_event *watcher, int event)
{
	struct ev_coro          *cursor = NULL;
	struct evcoro_event_t   idle = { .io.fd = -1 };

	assert(watcher);
	_scheduler_checkowner(scheduler);

	cursor = evcoro_list_cursor(scheduler->working);

	idle.flag = true;
	idle.coro = cursor;
	idle.io.data = &idle;
	idle.timer.data = &idle;
	idle.type = event;
	idle.loop = scheduler->listener;
	
	switch (event) {
		case EVCORO_TIMER:
			if (likely(watcher->timer > 0)) {
				ev_tstamp to = watcher->timer;
				to = unlikely(to > EVCORO_TIMER_MAXTO) ? EVCORO_TIMER_MAXTO : to;
				to = unlikely(to < EVCORO_IO_MINTO) ? EVCORO_IO_MINTO : to;
				ev_timer_init(&idle.timer, _scheduler_event_to, to, .0);
				ev_timer_start(idle.loop, &idle.timer);
			}
			break;
		case EVCORO_READ:
		case EVCORO_WRITE:
			/*初始化读写*/
			ev_io_init(&idle.io, _scheduler_event_io, watcher->io._fd,
					   event == EVCORO_READ ? EV_READ : EV_WRITE);
			ev_tstamp to = watcher->io._to;
			
			to = to < 0 ? EVCORO_TIMER_MAXTO : to;
			to = unlikely(to > EVCORO_TIMER_MAXTO) ? EVCORO_TIMER_MAXTO : to;
			to = unlikely(to < EVCORO_IO_MINTO) ? EVCORO_IO_MINTO : to;
			
			ev_timer_init(&idle.timer, _scheduler_event_to, to, .0);
			
			ev_io_start(idle.loop, &idle.io);
			ev_timer_start(idle.loop, &idle.timer);
			break;
		default:
			fprintf(stderr, "\033[1;33m" "Undefined kind of event.\naborting..." "\033[m" "\n");
			abort();
			break;
	}
	
	/*主协程，不需要切换*/
	if (_evcoro_mainctx(cursor)) {
		/*等待主协程超时或就绪*/
		ev_loop(idle.loop, EVRUN_ONCE);
		/*由于主协程的事件可能没有被触发，所以要手动停掉注册的事件*/
		ev_timer_stop(idle.loop, &idle.timer);
		ev_io_stop(idle.loop, &idle.io);
	} else {
		_scheduler_switch(scheduler, true);
	}

	return idle.flag;
}

bool evcoro_cleanup_push(struct evcoro_scheduler *scheduler, evcoro_destroycb call, void *usr)
{
	assert(likely(call));
	_scheduler_checkowner(scheduler);
	return _evcoro_cleanup_push(evcoro_list_cursor(scheduler->working), call, usr);
}

void evcoro_cleanup_pop(struct evcoro_scheduler *scheduler, bool execute)
{
	_scheduler_checkowner(scheduler);
	bool flag = _evcoro_cleanup_pop(evcoro_list_cursor(scheduler->working), execute);
	assert(likely(flag));
}

/* ------------------------------------------------------ */
static inline void _scheduler_switch(struct evcoro_scheduler *scheduler, bool isidle)
{
	struct ev_coro  *cursor = NULL;
	struct ev_coro  *next = NULL;

	cursor = evcoro_list_cursor(scheduler->working);

	if (_evcoro_subctx(cursor)) {
		if (unlikely(isidle)) {
			/*将当前协程从工作协程链表中移除，步进到上一个协程，并加入挂起协程链表中*/
			evcoro_list_push(scheduler->suspending, evcoro_list_pull(scheduler->working));
		}

		/*
		 * 为了与evcoro_loop()的步进方式保持一致，
		 * 判断当前协程是否时循环链表的尾节点(一下个是主协程)
		 * 如果是尾节点（子协程）则不再移动工作节点
		 */
		next = evcoro_list_movenext(scheduler->working);

		if (_evcoro_mainctx(next)) {
			evcoro_list_moveprev(scheduler->working);
		}

		/*切出到其他协程*/
		coro_transfer(&cursor->ctx, &next->ctx);
		/*下次切回时，从此处继续运行*/
	}
}

static inline void _scheduler_checkowner(struct evcoro_scheduler *scheduler)
{
	long tid = 0;

	assert(likely(scheduler));

#if defined(SYS_thread_selfid)
	tid = syscall(SYS_thread_selfid);
#elif defined(SYS_gettid)
	tid = syscall(SYS_gettid);
#endif

	if (unlikely((tid < 0) || (tid != scheduler->ownerid))) {
		/*不能在其他线程使用该句柄*/
		fprintf(stderr,
			"\033[1;31m"
			"This handle can't used to multi-thread.\naborting..."
			"\033[m" "\n");
		abort();
	}
}

static inline void _scheduler_event_overdeal(struct evcoro_event_t *event)
{
	/*在此添加事件完成处理方式*/
	switch (event->type) {
  		case EVCORO_TIMER:
			ev_timer_stop(event->loop, &event->timer);
			break;
		case EVCORO_READ:
		case EVCORO_WRITE:
			ev_io_stop(event->loop, &event->io);
			ev_timer_stop(event->loop, &event->timer);
			break;
  		default:
			fprintf(stderr, "\033[1;33m" "Undefined kind of event.\naborting..." "\033[m" "\n");
			abort();
			break;
	}
	
	if (_evcoro_subctx(event->coro)) {
		struct evcoro_scheduler *scheduler = NULL;
		scheduler = event->coro->scheduler;
		assert(likely(scheduler));

		/*从挂起链表中移除*/
		_evcoro_pull(event->coro);
		evcoro_list_counter(scheduler->suspending)--;
		/*加入工作链表*/
		evcoro_list_push(scheduler->working, event->coro);
	}
}

static void _scheduler_event_to(struct ev_loop *loop, struct ev_timer *timer, int event)
{
	struct evcoro_event_t *eventptr = timer->data;

	assert(likely(eventptr));

#ifdef DEBUG
	if (eventptr->io.fd > -1) {
		printf("\033[1;33m" "[%p] : file descriptor [%d] timed out." "\033[m" "\n", eventptr->coro, eventptr->io.fd);
	} else {
		printf("\033[1;33m" "[%p] : complete one timed-out request." "\033[m" "\n", eventptr->coro);
	}
#endif

	eventptr->flag = false;
	_scheduler_event_overdeal(eventptr);
}

static void _scheduler_event_io(struct ev_loop *loop, struct ev_io *io, int event)
{
	struct evcoro_event_t *eventptr = io->data;

	assert(likely(eventptr));

#ifdef FEED_EVENT
	/*如果读写事件是一个边缘触发，比如zmq之类的io，则需要主动种植完成动作*/
	if (event == EV_READ) {
		ev_feed_fd_event(loop, io->fd, EV_READ);
	} else {
		ev_feed_fd_event(loop, io->fd, EV_WRITE);
	}
#endif
	ev_timer_stop(loop, &eventptr->timer);
	ev_io_stop(loop, io);

#ifdef DEBUG
	printf("\033[1;32m" "[%p] : file descriptor [%d] standby." "\033[m" "\n", eventptr->coro, eventptr->io.fd);
#endif

	eventptr->flag = true;
	_scheduler_event_overdeal(eventptr);
}

