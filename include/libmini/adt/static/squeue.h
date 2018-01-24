//
//  squeue.h
//  libmini
//
//  Created by 周凯 on 15/9/15.
//  Copyright © 2015年 zk. All rights reserved.
//

#ifndef __libmini_squeue_h__
#define __libmini_squeue_h__

#include "../adt_proto.h"
#include "../../atomic/atomic.h"

__BEGIN_DECLS

#ifndef SQUEUE_USR_DATA_SIZE
  #define SQUEUE_USR_DATA_SIZE (128)
#endif

/*使用原子锁做读写*/
// #define SQUEUE_USE_RWLOCK

/*使用一个64位的字段存储索引*/
// #define SQUEUE_USE_64BIT_IDX
typedef struct SQueue *SQueueT;

struct SQueue
{
	/*!!!以下成员全是只读模式，不可裸修改!!!*/
	int             magic;
	unsigned        headidx;			/**< 头节点下标*/
	unsigned        tailidx;			/**< 尾节点下标*/
	//	uint64_t		index;				/**< 高32位为头节点下标，低32位为尾节点下标*/
	int             datasize;			/**< 单个节点中数据大小*/
	int             totalnodes;			/**< 总节点数数*/
	int             capacity;			/**< 容量*/
	int             nodes;				/**< 当前节点数，永远比totalnodes小1*/
	int             refs;				/**< 关联数*/
	unsigned long   buffoffset;			/**< 数据buff偏移*/
	long            totalsize;			/**< 总数据大小，包括控制头*/
#ifdef SQUEUE_USE_RWLOCK
	AO_SpinLockT    rlck;				/**< 读锁*/
	AO_SpinLockT    wlck;				/**< 写锁*/
#endif
	/*以下成员可以裸修改*/
	char            usr[SQUEUE_USR_DATA_SIZE];	/**< 调用层数据*/
};

/**
 * 初始化静态队列
 * @param buff 外部提供的数据块
 * @param size 数据块的长度
 * @param datasize 每个节点的大小
 */
SQueueT SQueueInit(char *buff, long size, int datasize, bool shared);

/**
 * 检查映射的内存中的数据是否符合队列的数据结构
 * @param size 被检查数据块的实际长度，当队列内部使用长度可以小于该值
 * @param datasize 每个节点的大小，当小于或等于0时，由内部断言
 */
bool SQueueCheck(char *buff, long size, int datasize, bool repair);

bool SQueuePush(SQueueT queue, char *data, int size, int *effectsize);

bool SQueuePriorityPush(SQueueT queue, char *data, int size, int *effectsize);

bool SQueuePull(SQueueT queue, char *data, int size, int *effectsize);

/**
 * 计算返回要容纳指定数据量和数据大小的链表的总长度
 */
long SQueueCalculateSize(int nodetotal, int datasize);

__END_DECLS
#endif	/* __libmini_squeue_h__ */

