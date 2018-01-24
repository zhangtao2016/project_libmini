//
//  ev_coro.h
//  libmini
//
//  Created by 周凯 on 15/12/1.
//  Copyright © 2015年 zk. All rights reserved.
//

#ifndef ev_coro_scheduler_h
#define ev_coro_scheduler_h

#include <stdbool.h>
#include "../libev/ev.h"
#include "_evcoro_common.h"

#if __cplusplus
extern "C" {
#endif
/* ------------------------------------------------------ */

/*
 * 超时时间单位为秒，可以是小数
 */
enum
{
	EVCORO_TIMER = 1,	/*超时事件*/
	EVCORO_READ,	/*读事件*/
	EVCORO_WRITE,	/*写事件*/
};

/*空闲切换事件模型*/
union evcoro_event
{
	struct
	{
		ev_tstamp       _to;	/*读写空闲切换超时时间*/
		int             _fd;	/*读写描述符*/
	}               io;
	ev_tstamp       timer;	/*超时空闲切换超时时间*/
	struct
	{
		int     *_addr;
		int     _until;
	}               notify;
};

/*初始化读写切换事件*/
#define evcoro_io_init(event, fd, to)   do { (event)->io._fd = (fd); (event)->io._to = (to); } while (0)

/*初始化超时切换事件*/
#define evcoro_timer_init(event, to)    do { (event)->timer = (to); } while (0)

/*初始化通知事件*/

/* ------------------------------------------------------ *\
 * 协程调度器可以比作一个内核线程调度器，只不过协程可以中运行的函数
 * 可以保存自己的状态，当下次被调度时，会表现为在切出函数处切回，并
 * 接着运行下一条指令，控制权切换分两种，空闲或快速切换，空闲切换时
 * 调度器回将与一个事件结合，并将该协程踢出调度队列，等到该事件触发
 * 时在加入到调度队列。调度队列将依次调度每一个协程。被调度的协程会
 * 持续运行直到通过切换函数主动交出控制权或运行完毕。
\* ------------------------------------------------------ */
struct ev_coro;
struct ev_coro_t;
struct evcoro_scheduler;

/**
 * 任务回调接口函数
 * @param coroptr 调度器句柄
 * @param usr 协程对象任务参数的
 */
typedef void (*evcoro_taskcb)(struct evcoro_scheduler *scheduler, void *usr);

/**
 * 销毁每个协程对象任务参数的接口函数
 */
typedef void (*evcoro_destroycb)(void *usr);

/**
 * 协程调度器模型
 */
struct evcoro_scheduler
{
	/*private:read only*/
	struct ev_coro_t        *working;	/**<执行协程对象链表，调度队列*/
	struct ev_coro_t        *suspending;	/**<挂起协程对象链表，挂起队列*/
	struct ev_coro_t        *usable;	/**<可用协程对象栈，协程池*/
	int                     bolt;		/**<可用协程对象栈数量闩值，协程池大小*/
	enum
	{
		EVCOROSCHE_STAT_INIT = 0xf1, /*已初始化*/
		EVCOROSCHE_STAT_LOOP,	/*已在循环调度中*/
		EVCOROSCHE_STAT_STOP, /*已被停止*/
	}                       stat;	/**<调度器状态*/

	long                    ownerid; /**< 创建者线程id*/
	struct ev_loop          *listener;	/**<就绪协程侦听器*/
	/*public:write and read*/
	void                    *user;		/**<用户层数据*/
};

/* ------------------------------------------------------ */
/**
 * 创建调度器
 * @param bolt 可用缓存协程对象的数量 -1则使用默认值
 * @return 返回调度器句柄
 */
struct evcoro_scheduler *evcoro_create(int bolt);

/**
 * 销毁调度器
 * @param scheduler 调度器
 * @param destroy 如果有挂起的协程（任务函数），则将其每个任务入口函数参数使用此回调函数调用
 */
void evcoro_destroy(struct evcoro_scheduler *scheduler, evcoro_destroycb destroy);

/* ------------------------------------------------------ */
/**
 * 开始循环调度，如果没有加入协程（任务函数），将空转；
 * 每循环一次，就会掉用idle回调，如果没有任务，则需要在idle函数中做适当休眠
 * @param scheduler 调度器
 * @param idle 空闲回调函数，每循环一圈就会调用
 * @param usr 空闲回调函数的入参
 */
void evcoro_loop(struct evcoro_scheduler *scheduler, evcoro_taskcb idle, void *usr);

/**
 * 加入一个协程（任务），新加入的协程要待下一次循环才会调度。
 * 如果底层是用信号运行栈和长跳转实现，则不能在协程（任务）中调用此函数，mac os x系统就是此种实现
 * @param scheduler 调度器
 * @param call 协程任务回调函数
 * @param ss 协程函数运行所需要的栈大小，如果函数中用大数据块的局部变量，则应该加大此值，0则使用默认值。
 */
bool evcoro_push(struct evcoro_scheduler *scheduler, evcoro_taskcb call, void *usr, size_t ss);

/* ------------------------------------------------------ */
/**
 * 快速切换，不从调度队列中踢出
 */
void evcoro_fastswitch(struct evcoro_scheduler *scheduler);

/**
 * 空闲切换
 * 如果是空闲回调函数中调用此函数，则不会发生切换，仅在事件循环器中注册了一个事件，
 * 并且会调度器挂起，直到事件被触发或发生超时。
 * 标明当前切出的协程是空闲状态，并注册一个事件，待事件触发或超时后才会被再度调度
 * 描述符类型的空闲切换，返回值：true就绪，false超时
 * 计时器类型的空闲切换，返回值永远都是false
 */
bool evcoro_idleswitch(struct evcoro_scheduler *scheduler, const union evcoro_event *watcher, int event);

/* ------------------------------------------------------ */
/**
 * 推入协程清理回调，将在协程退出时调用回调函数
 * 模拟posix的pthread_cleanup_push()，必须与evcoro_cleanup_pop()成对出现
 * @param call回调函数
 * @param usr回调函数的参数
 */
bool evcoro_cleanup_push(struct evcoro_scheduler *scheduler, evcoro_destroycb call, void *usr);
/**
 * 弹出协程清理回调，可以选择是否执行回调函数
 * 模拟posix的pthread_cleanup_pop()，必须与evcoro_cleanup_push()成对出现
 * @param execute false不执行回调
 */
void evcoro_cleanup_pop(struct evcoro_scheduler *scheduler, bool execute);
/* ------------------------------------------------------ *\
 * 以上函数只能在一个固定的线程中调用
\* ------------------------------------------------------ */

/*
 * 判断调度器是否还在调度协程
 */
#define evcoro_isactive(scheduler) \
	(!!((scheduler)->stat == EVCOROSCHE_STAT_LOOP))

/**
 * 手动停止循环调度
 */
static inline void evcoro_stop(struct evcoro_scheduler *scheduler)
{
	(scheduler)->stat = EVCOROSCHE_STAT_STOP;
}

/**当前工作协程数*/
int evcoro_workings(struct evcoro_scheduler *scheduler);

/**当前空闲协程数*/
int evcoro_suspendings(struct evcoro_scheduler *scheduler);

/*设置携带数据*/
#define evcoro_set_usrdata(scheduler, usr) ((scheduler)->user = (usr))

/*获取携带数据*/
#define evcoro_get_usrdata(scheduler) ((scheduler)->user)

#define evcoro_get_evlistener(scheduler) ((scheduler)->listener)
/* ------------------------------------------------------ */
#if __cplusplus
}
#endif
#endif	/* ev_coro_scheduler_h */

