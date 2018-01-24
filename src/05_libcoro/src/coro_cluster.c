//
//  coro_cluster.c
//  libmini
//
//  Created by 周凯 on 15/10/14.
//  Copyright © 2015年 zk. All rights reserved.
//
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>

#if defined __GNUC__
  #define likely(x)     __builtin_expect(!!(x), 1)
  #define unlikely(x)   __builtin_expect(!!(x), 0)
#else
  #define likely(x)     (!!(x))
  #define unlikely(x)   (!!(x))
#endif

#include "coro.h"
#include "coro_cluster.h"

enum
{
		COROTASK_STAT_INIT = 0,
		COROTASK_STAT_RUNNING,
		COROTASK_STAT_EXITED,
		COROTASK_STAT_EXCEPT,
		COROTASK_STAT_TIMEDOUT,
};

/*是否启用模糊休眠机制*/
#define COROTASKK_BLURRED_IDLE
/*是否启用快速切换*/
#define COROTASK_FAST_SWITCH
/*是否重用协程*/
#define COROTASK_REUSED
/*休眠扩大倍数*/
#define COROTASK_IDLE_MUTIL(x)  (((unsigned)(x)) << 6)
/*休眠最小值*/
#define COROTASK_IDLE_MIN(x)    ((unsigned)((unlikely((x)) < 1 ? 1 : (x))))
/*重用协程闩值*/
#define COROTASK_BOLT_MINSIZE   (10)
#define COROTASK_BOLT_MAXSIZE   (1000)
/*最小栈大小*/
#ifndef COROTASK_STACK_SIZE
  #define COROTASK_STACK_SIZE   (10240U)
#endif

#define COROTASK_IS_SUBCTX(ptr)         (likely((ptr)->pool != (ptr)->main))
#define COROTASK_IS_MAINCTX(ptr)        (unlikely((ptr)->pool == (ptr)->main))
#define COROTASK_STAT_ISOK(task)        ((task)->stat == COROTASK_STAT_INIT || (task)->stat == COROTASK_STAT_RUNNING)
#define COROTASK_STAT_ISEXITED(task)    (unlikely((task)->stat == COROTASK_STAT_EXITED))
#define COROTASK_STAT_ISEXCEPT(task)    (unlikely((task)->stat == COROTASK_STAT_EXCEPT))
#define COROTASK_STAT_ISTIMEDOUT(task)  (unlikely((task)->stat == COROTASK_STAT_TIMEDOUT))

/*随机数*/

/*
 * 将时间设置为随机种子
 */
#define Rand() (srandom((unsigned int)time(NULL)))

/*
 * 随机获取整数 x ： low <= x <= high
 */
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

struct cleanup
{
		CORODESTROY     call;
		void            *usr;
		struct cleanup  *next;
};

struct coro_task
{
		struct coro_task    *prev;
		struct coro_task    *next;

		coro_context        ctx;
		coro_context        *main;

		COROTASK            call;
		void                *usr;
		int                 stat;

		bool                stoploop;	/*超时时是否停止所属循环*/
		int                 to;			/*设定的超时值*/
		struct timeval      tv;			/*超时刷新值*/
		struct cleanup      *cleanup;	/*局部清理栈，对比pthread_cleanup_push/pop*/

		struct coro_stack   stack;
		ssize_t             stss;	/*栈大小*/
};

static void _coro_task_run(void *arg);

static inline void _coro_task_timedout(struct coro_task *task);

static inline bool _coro_task_cleanup(struct coro_task *task, bool execute);

/* ---------                     */
static inline bool _coro_cluster_push(struct coro_cluster *cluster,
		struct coro_task *task, size_t ss, bool isnew);

static inline void _coro_cluster_pop(struct coro_cluster *cluster, bool reused);

static inline void _coro_cluster_checkowner(struct coro_cluster *cluster);

static inline void _coro_cluster_switch(struct coro_cluster *cluster, bool idle);

#if defined(__GNUC__)
static void _coro_cluster_init() __attribute__((constructor()));
#endif

/* --------------------           */

struct coro_cluster *coro_cluster_create(int bolt, bool idleable)
{
		struct coro_cluster *cluster = NULL;

		cluster = calloc(1, sizeof(*cluster));

		if (unlikely(!cluster)) {
				return NULL;
		}

		cluster->main = calloc(1, sizeof(*cluster->main));

		if (unlikely(!cluster->main)) {
				free(cluster);
				return NULL;
		}

		coro_create(&cluster->main->ctx, NULL, NULL, NULL, 0);

		cluster->main->prev = cluster->main;
		cluster->main->next = cluster->main;

		cluster->pool = cluster->main;
		cluster->stat = COROSTAT_INIT;
		cluster->idleable = idleable;
		cluster->bolt = bolt < COROTASK_BOLT_MINSIZE ? COROTASK_BOLT_MINSIZE : bolt;
		cluster->bolt = cluster->bolt > COROTASK_BOLT_MAXSIZE ? COROTASK_BOLT_MINSIZE : cluster->bolt;

#if defined(SYS_thread_selfid)
		cluster->owner = syscall(SYS_thread_selfid);
#elif defined(SYS_gettid)
		cluster->owner = syscall(SYS_gettid);
#endif

		if (unlikely(cluster->owner < 0)) {
				free(cluster->main);
				free(cluster);
				return NULL;
		}

		return cluster;
}

void coro_cluster_destroy(struct coro_cluster *cluster, CORODESTROY destroy)
{
		if (unlikely(!cluster)) {
				return;
		}

		_coro_cluster_checkowner(cluster);

		while (unlikely(cluster->tasks > 0)) {
				if (COROTASK_IS_SUBCTX(cluster)) {
						/*销毁任务的用户层数据*/
						void *usr = cluster->pool->usr;
						_coro_cluster_pop(cluster, false);

						if (destroy) {
								destroy(usr);
						}
				} else {
						cluster->pool = cluster->pool->next;
				}
		}

#ifdef COROTASK_REUSED
		while (unlikely(cluster->unused)) {
				cluster->cnter--;
				struct coro_task *task = cluster->unused;
				cluster->unused = task->next;
				coro_destroy(&task->ctx);
				coro_stack_free(&task->stack);
				free(task);
		}
#endif

		while (unlikely(_coro_task_cleanup(cluster->main, true))) {}

		coro_destroy(&cluster->main->ctx);
		free(cluster->main);
		free(cluster);
}

bool coro_cluster_add(struct coro_cluster *cluster, COROTASK call, void *usr, size_t ss)
{
		struct coro_task    *task = NULL;
		bool                flag = false;
		bool                isnew = false;

		_coro_cluster_checkowner(cluster);

		if (unlikely(!call)) {
				return false;
		}

#ifdef COROTASK_REUSED
		/*重用任务*/
		task = cluster->unused;

		if (likely(task && (task->stss >= ss))) {
				cluster->unused = task->next;
				cluster->cnter--;
		} else
#endif
		{
				task = calloc(1, sizeof(*task));

				if (unlikely(!task)) {
						return false;
				}

				isnew = true;
		}

		task->call = call;
		task->usr = usr;

		flag = _coro_cluster_push(cluster, task, ss, isnew);

		if (unlikely(!flag && isnew)) {
				free(task);
		}

		return flag;
}

bool coro_cluster_startup(struct coro_cluster *cluster, COROIDLE idle, void *usr)
{
		_coro_cluster_checkowner(cluster);
#if 1
		if (unlikely(!(cluster->tasks > 0) &&
				!cluster->idleable)) {
				return false;
		}

#else
		assert(cluster->tasks > 0 || cluster->idleable);
#endif

		cluster->result = true;
		cluster->stat = COROSTAT_RUN;

		while (likely((cluster->tasks > 0 || cluster->idleable) &&
				cluster->stat == COROSTAT_RUN)) {
				cluster->pool = cluster->pool->next;

				if (COROTASK_IS_MAINCTX(cluster)) {
						/*循环完成一次*/
						if (likely(idle)) {
								idle(cluster, usr);
						} else if (likely(cluster->tasks > 0)) {
#if 0
								usleep(100);
#else
								sched_yield();
#endif
						} else {
								break;
						}

#ifdef COROTASKK_BLURRED_IDLE
						/*模糊休眠*/
						if (unlikely(cluster->idlecnt > 0)) {
								float p = ((float)cluster->idlecnt) / cluster->tasks;
  #ifdef DEBUG
								printf("\033[1;32m" "coro [%p] calculate [%f = %d / %d] for sleep." "\033[m" "\n",
										cluster, p, cluster->idlecnt, cluster->tasks);
  #endif

								/*掷骰子*/
								if (unlikely(RandChance(p))) {
										/*毫秒级随机休眠*/
										long ms = RandInt(1, cluster->idlecnt);
										/*扩大2^n倍*/
										ms = COROTASK_IDLE_MUTIL(ms);
  #ifdef DEBUG
										printf("\033[1;32m" "coro [%p] sleep %ld." "\033[m" "\n", cluster, ms);
  #endif
										/*休眠*/
										usleep((unsigned)ms);
								}

								cluster->idlecnt = 0;
						}
#endif					/* ifdef COROTASKK_BLURRED_IDLE */
				} else {
						assert(COROTASK_STAT_ISOK(cluster->pool));
						coro_transfer(&cluster->main->ctx, &cluster->pool->ctx);

						/*检查是否有协同结束*/
						if (COROTASK_IS_SUBCTX(cluster)) {
								if (COROTASK_STAT_ISEXCEPT(cluster->pool)) {
										/*异常退出，结束运行*/
										_coro_cluster_pop(cluster, true);
										cluster->result = false;
										break;
								} else if (COROTASK_STAT_ISTIMEDOUT(cluster->pool)) {
										/*运行栈在函数中途退出，不可重用*/
										_coro_cluster_pop(cluster, false);

										/*设置为中途退出，则停止循环*/
										if (unlikely(cluster->pool->stoploop)) {
												cluster->result = false;
												break;
										}
								} else if (COROTASK_STAT_ISEXITED(cluster->pool)) {
										_coro_cluster_pop(cluster, true);
								}
						}
				}
		}

		//      coro_cluster_stop(cluster);

		return cluster->result;
}

void coro_cluster_switch(struct coro_cluster *cluster)
{
		_coro_cluster_switch(cluster, false);
}

void coro_cluster_idleswitch(struct coro_cluster *cluster)
{
		_coro_cluster_switch(cluster, true);
}

/**
 * 刷新当前运行的任务超时时间，任务默认没有超时
 */
void coro_cluster_timeout(struct coro_cluster *cluster, int timeout, bool stoploop)
{
		_coro_cluster_checkowner(cluster);

		if (COROTASK_IS_MAINCTX(cluster)) {
				return;
		}

		cluster->pool->to = timeout;

		if (timeout > 0) {
				size_t ms = timeout * 1000;

				gettimeofday(&cluster->pool->tv, NULL);
				ms += cluster->pool->tv.tv_usec;
				cluster->pool->tv.tv_usec = ms % 1000000;
				cluster->pool->tv.tv_sec += ms / 1000000;

				cluster->pool->stoploop = stoploop;
		}
}

/**
 * 对当前运行的任务压入一个退出回调
 */
bool coro_cluster_cleanpush(struct coro_cluster *cluster, CORODESTROY call, void *usr)
{
		_coro_cluster_checkowner(cluster);
#if 0
		if (COROTASK_IS_MAINCTX(cluster)) {
				return false;
		}
#endif

		if (unlikely(!call)) {
				return false;
		}

		struct cleanup *cleanup = NULL;
		cleanup = calloc(1, sizeof(*cleanup));

		if (unlikely(!cleanup)) {
				return false;
		}

		cleanup->next = cluster->pool->cleanup;
		cleanup->call = call;
		cleanup->usr = usr;

		cluster->pool->cleanup = cleanup;

		return true;
}

/**
 * 对当前运行的任务弹出一个退出回调，可以主动调用，也可以忽略
 */
void coro_cluster_cleanpop(struct coro_cluster *cluster, bool execute)
{
		_coro_cluster_checkowner(cluster);
#if 0
		if (COROTASK_IS_MAINCTX(cluster)) {
				return;
		}
#endif
		_coro_task_cleanup(cluster->pool, execute);
}

/* --------------------           */
static inline void _coro_cluster_switch(struct coro_cluster *cluster, bool idle)
{
		_coro_cluster_checkowner(cluster);

		if (unlikely(idle)) {
				/*空闲切换，增加计数器*/
				cluster->idlecnt++;
		}

		if (COROTASK_IS_MAINCTX(cluster)) {
				/*在主协程中切换，则置零计数器*/
				if (likely(!idle)) {
						cluster->idlecnt = 0;
				}

				return;
		}

		_coro_task_timedout(cluster->pool);

#ifdef COROTASK_FAST_SWITCH
		/*步进到一个将要执行携程指针*/
		cluster->pool = cluster->pool->next;

		/*快速切到下一个协同*/
		if (COROTASK_IS_SUBCTX(cluster)) {
				coro_transfer(&cluster->pool->prev->ctx, &cluster->pool->ctx);
		} else {
				/*退一步，为了对应coro_cluster_startup()的步进方式*/
				cluster->pool = cluster->pool->prev;
				coro_transfer(&cluster->pool->ctx, &cluster->main->ctx);
		}

#else
		/*每次都切到主协同*/
		coro_transfer(&cluster->pool->ctx, &cluster->main->ctx);
#endif
}

static void _coro_task_run(void *arg)
{
		struct coro_cluster *cluster = arg;
		struct coro_task    *task = NULL;
		volatile int        result = -1;

		assert(cluster);

		do {
				task = cluster->pool;
				assert(task != cluster->main);
				assert(task->stat == COROTASK_STAT_INIT);

				task->stat = COROTASK_STAT_RUNNING;

				result = task->call(cluster, task->usr);

				if (unlikely(result < 0)) {
						task->stat = COROTASK_STAT_EXCEPT;
				} else {
						task->stat = COROTASK_STAT_EXITED;
				}

				/*切回主协同*/
				coro_transfer(&task->ctx, task->main);
#ifdef COROTASK_REUSED
				/*再次利用*/
		} while (1);
#else
				/*仅使用一次*/
		} while (0);
#endif
}

static inline void _coro_task_timedout(struct coro_task *task)
{
		/*
		 * 如果超时则切回到，如果需要停止循环，
		 * 则置状态为COROTASK_STAT_EXCEPT，
		 * 否则置为COROTASK_STAT_EXITED
		 */
		if (task->to > 0) {
				struct timeval tv = {};
				gettimeofday(&tv, NULL);

				if (unlikely((task->tv.tv_sec < tv.tv_sec) ||
						((task->tv.tv_sec == tv.tv_sec) &&
						(task->tv.tv_usec < tv.tv_usec)))) {
						/*超时*/
						task->stat = COROTASK_STAT_TIMEDOUT;
#ifdef DEBUG
						long ss = tv.tv_sec - task->tv.tv_sec;
						int us = tv.tv_usec - task->tv.tv_usec;

						if (us < 0) {
								us += 1000000;
								ss -= 1;
						}

						ss += task->to / 1000;
						us += (task->to % 1000) * 1000;

						printf("\033[1;33m" "task [%p] timed out : %d ~= %ld.%06d." "\033[m" "\n", task, task->to, ss, us);
#endif
						/*切回主协同，必须回收调此协程*/
						coro_transfer(&task->ctx, task->main);
				}
		}
}

static inline bool _coro_task_cleanup(struct coro_task *task, bool execute)
{
		if (unlikely(task->cleanup)) {
				struct cleanup *cleanup = task->cleanup;
				task->cleanup = task->cleanup->next;

				if (execute) {
						cleanup->call(cleanup->usr);
				}

				free(cleanup);
		}

		return unlikely(task->cleanup) ? true : false;
}

static inline bool _coro_cluster_push(struct coro_cluster *cluster,
		struct coro_task *task, size_t ss, bool isnew)
{
		bool flag = false;

		/*加入循环链表尾端*/
		task->next = cluster->pool;
		task->prev = cluster->pool->prev;
		cluster->pool->prev->next = task;
		cluster->pool->prev = task;
		
#if 1
        /*从最后加入的任务运行*/
#endif
    
		cluster->tasks++;

		task->main = &cluster->main->ctx;
		task->stat = COROTASK_STAT_INIT;

		/*非新创建*/
		if (likely(!isnew)) {
				return true;
		}

		ss = ss < COROTASK_STACK_SIZE ? COROTASK_STACK_SIZE : ss;
		task->stss = ss;
		/*创建数据，顺序不能乱*/
		flag = coro_stack_alloc(&task->stack, (unsigned)ss);

		if (unlikely(!flag)) {
				return false;
		}

		coro_create(&task->ctx, _coro_task_run, cluster, task->stack.sptr, task->stack.ssze);

		return true;
}

static inline void _coro_cluster_pop(struct coro_cluster *cluster, bool reused)
{
	
		if (COROTASK_IS_MAINCTX(cluster)) {
				return;
		}

		/*正常退出，删除当前协同*/
		struct coro_task *del = NULL;
		/*回退一个元素*/
		cluster->pool = cluster->pool->prev;
		/*删除下一个*/
		del = cluster->pool->next;
		del->next->prev = del->prev;
		del->prev->next = del->next;

		/*弹出并执行所有清理任务*/
		while (unlikely(_coro_task_cleanup(del, true))) {}

		cluster->tasks--;
#ifdef COROTASK_REUSED
		if (reused && (cluster->cnter + 1 <= cluster->bolt)) {
				cluster->cnter++;
				del->next = cluster->unused;
				cluster->unused = del;
				return;
		}
#endif

		/*释放数据，顺序不能乱，因为协同的栈在没有释放之前都是在应用的*/
		coro_destroy(&del->ctx);
		coro_stack_free(&del->stack);
		free(del);
}

static inline void _coro_cluster_checkowner(struct coro_cluster *cluster)
{
		long tid = 0;

#if defined(SYS_thread_selfid)
		tid = syscall(SYS_thread_selfid);
#elif defined(SYS_gettid)
		tid = syscall(SYS_gettid);
#endif

		if (unlikely((tid < 0) || (tid != cluster->owner))) {
				/*不是创建线程不能释放*/
				fprintf(stderr,
						"\033[1;31m"
						"This handle can't used to multi-thread."
						"\033[m" "\n");
				abort();
		}
}

#if defined(__GNUC__)
static void _coro_cluster_init()
{
		Rand();
}
#endif