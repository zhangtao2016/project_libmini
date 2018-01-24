//
//  coro_cluster.h
//  libmini
//
//
//  Created by 周凯 on 15/10/14.
//  Copyright © 2015年 zk. All rights reserved.
//

#ifndef coro_cluster_h
#define coro_cluster_h

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#if __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------ */

struct coro_cluster;
struct coro_task;

/**
 * 任务回调接口函数
 * @param coroptr 集群句柄
 * @param usr 调用层传递的数据指针
 * @return 小于0时，协程集群会停止循环
 */
typedef int (*COROTASK)(struct coro_cluster *coroptr, void *usr);

/**
 * 循环空闲接口函数
 */
typedef void (*COROIDLE)(struct coro_cluster *coroptr, void *usr);

/**
 * 销毁用户层数据接口函数
 */
typedef void (*CORODESTROY)(void *usr);

/**
 * 协程集群句柄
 */
struct coro_cluster
{
	struct coro_task        *pool;
	struct coro_task        *main;

	struct coro_task        *unused;			/*可用协程首地址*/
	int                     cnter;				/*可用协程数量*/
	int                     bolt;				/*可用协程闩值*/

	bool                    idleable;			/*是否可以空转*/
	int                     idlecnt;			/*空闲协程计数器*/

	int                     tasks;				/*可执行任务数量*/
	bool                    result;

	enum
	{
		COROSTAT_INIT,
		COROSTAT_RUN,
		COROSTAT_STOP,
	}                       stat;

	long                    owner;

	void                    *user;			/*用户层数据*/
};

/* ------------------------------------------------------ */

/**
 * 创建一个协程集群
 * @param bolt 协程数保留闩值
 * @param idleable 当没有任务时，且有idle回调时，协程集群能否空转
 * @return 协程集群句柄，NULL时，表示不可用。
 */
struct coro_cluster     *coro_cluster_create(int bolt, bool idleable);

/**
 * 销毁协程集群，只能在携程集群创建线程中调用该函数
 * @param cluster 需要消耗的集群
 * @param destroy 对每个未完成的任务的调用层数据回调该函数
 */
void coro_cluster_destroy(struct coro_cluster *cluster, CORODESTROY destroy);

/**
 * 添加一个任务，只能在携程集群创建线程中调用该函数
 * @param cluster 目标集群
 * @param task 任务回调函数
 * @param usr 任务回调函数的调用层数据
 * @param stacksize 所使用的栈大小，要大于等于回调函数中的所有局部变量的总大小（包括回调函数中的其它递归调用函数）
 * @return false/true
 */
bool coro_cluster_add(struct coro_cluster *cluster, COROTASK task, void *usr, size_t stacksize);

/**
 * 刷新当前运行的任务超时时间，任务默认没有超时，指定超时时是否停止循环
 * 在主协中调用视为无效
 */
void coro_cluster_timeout(struct coro_cluster *cluster, int timeout, bool stoploop);

/**
 * 对当前运行的任务压入一个退出回调
 * 在主协中调用时，仅在协程集群句柄被销毁时，被动弹出销毁
 */
bool coro_cluster_cleanpush(struct coro_cluster *cluster, CORODESTROY call, void *usr);

/**
 * 对当前运行的任务弹出一个退出回调，可以主动调用，也可以忽略
 */
void coro_cluster_cleanpop(struct coro_cluster *cluster, bool execute);

/**
 * 启动集群，只能在携程集群创建线程中调用该函数
 * 如果集群不能空转，且当没有可执行任务时，将停止。
 * @param cluster
 * @param idle 每次循环的idle回调函数
 * @param usr idle回调函数的调用层数据
 * @return 如果有任务返回了小于0的值，则返回false，否则返回true
 */
bool coro_cluster_startup(struct coro_cluster *cluster, COROIDLE idle, void *usr);

/**
 * 在任务回调函数中交出控制权，切换到下一个任务中
 * 并表示当前协程是忙状态
 * 在idle回调中，忙切换则清空空闲计数器
 */
void coro_cluster_fastswitch(struct coro_cluster *cluster);

/**
 * 切换
 * 并表示当前协程是空闲状态
 */
void coro_cluster_idleswitch(struct coro_cluster *cluster);

/* ---------------- 以下宏可以在其它线程中调用 ---------------- */

/*
 * 判断集群循环是否因为调用了coro_cluster_stop()而停止的
 */
#define coro_cluster_isactive(cluster) \
	(!!((cluster)->stat == COROSTAT_RUN))

/*
 * 当前任务数
 */
#define coro_cluster_tasks(cluster) \
	((cluster)->tasks)

/*
 * 是否有未完成的任务
 */
#define coro_cluster_hastask(cluster) \
	(!!((cluster)->tasks > 0))

/*
 * 手动停止集群循环
 */
#define coro_cluster_stop(cluster) \
	((cluster)->stat = COROSTAT_STOP)

/* ------------------------------------------------------ */
#if __cplusplus
}
#endif
#endif	/* coro_cluster_h */

