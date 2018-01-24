//
//  slist.h
//  libmini
//
//  Created by 周凯 on 15/7/16.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __libmini_slist_h__
#define __libmini_slist_h__

#include "../adt_proto.h"
#include "../../atomic/atomic.h"

__BEGIN_DECLS
/* ----------------                 */
#ifndef SLIST_USR_DATA_SIZE
  #define SLIST_USR_DATA_SIZE (128)
#endif

/*
 * 静态链表
 */

typedef struct SList *SListT;

struct SList
{
	/*!!!以下成员全是只读模式，不可裸修改!!!*/
	int             magic;
	int             headidx;			/**< 头节点下标*/
	int             tailidx;			/**< 尾节点下标*/
	int             unusedidx;			/**< 未使用节点下标*/
	int             datasize;			/**< 单个节点中数据大小*/
	int             totalnodes;			/**< 总节点数数*/
	int             nodes;				/**< 当前节点数*/
	int             refs;				/**< 关联数*/
	unsigned long   buffoffset;			/**< 数据buff偏移*/
	long            totalsize;			/**< 总数据大小，包括控制头*/
	AO_SpinLockT    lock;				/**< 同步锁*/
	/*以下成员可以裸修改*/
	char            usr[SLIST_USR_DATA_SIZE];	/**< 调用层数据*/
};

/**
 * 初始化静态链表
 * @param buff 外部提供的数据块
 * @param size 数据块的长度
 * @param datasize 每个节点的大小
 */
SListT SListInit(char *buff, long size, int datasize, bool shared);

/* ----------------                 */

/*
 * 说明：
 *      1.内部节点分布：
 *                    HEAD-NODE                                       TAIL-NODE
 *              [-1]<-[prev]<-[prev]<-[prev]<-[prev]<-[prev]<-[prev]<-[prev]
 *                    [next]->[next]->[next]->[next]->[next]->[next]->[next]->[-1]
 *                    [0001]  [0002]  [0003]  [0004]  [0005]  [0006]  [0007]
 *      2.内部节点数据指针：
 *              多个接口都可以获取／操纵内部节点数据指针，
 *              此时一定要用SListLock()/SListUnlock()将多个操作合为一个原子操作，
 *              否则获取的数据可能无效或者修改的数据可能破坏链表自身的结构。
 *      3.优化了头部插入和尾部删除
 *      4.优化了在当前节点和下一个节点（下一个节点存在）之间插入
 *      5.优化了非头部删除
 */

/* ----------------                 */

/**
 * 在头部插入一份数据的拷贝
 * @param list 静态链表
 * @param Buff 被插入的数据
 * @param size 被插入数据的长度（可以是0长度的数据）
 * @param effectsize 已插入数据的长度，如果插入的数据长度过长，会被截断到静态链表可容纳的长度。
 * @param inptr 如果操作成功且不为NULL时，返回链表内部的数据首地址
 * @return false/true
 */
bool SListAddHead(SListT list, char *buff, int size, int *effectsize, void **inptr);

/**
 * 在尾部插入数据
 * @see SListAddHead()
 */
bool SListAddTail(SListT list, char *buff, int size, int *effectsize, void **inptr);

/**
 * 查看头部的数据/查看尾部的数据
 * @param effectsize 当buff为NULL时，返回数据本身的长度
 * @see SListAddHead()
 */
bool SListCheckHead(SListT list, char *buff, int size, int *effectsize, void **inptr);

bool SListCheckTail(SListT list, char *buff, int size, int *effectsize, void **inptr);

/**
 * 删除头数据、尾数据
 * @param Buff 不为空时，被删除的数据将被复制到其中
 * @param size Buff的长度
 * @param effectsize 被删除数据的长度
 * @return true成功删除，false可能没有可删除的数据而失败
 */
bool SListDelHead(SListT list, char *buff, int size, int *effectsize);

bool SListDelTail(SListT list, char *buff, int size, int *effectsize);

/* ----------------                 */

/**
 * 反向与正向遍历链表
 * 反向tail-->>head
 * 正向head-->>tail
 * @param travel 遍历回调函数（data 为链表中的数据首地址，
 *      size 为其长度， user 调用者传递的数据， 返回false时会停止遍历）
 * @param user 调用者的数据
 */
void SListReverseTravel(SListT list, NodeTravelCB travel, void *user);

void SListTravel(SListT list, NodeTravelCB travel, void *user);

void SListTravelDel(SListT list, NodeCompareCB compare, void *user, int usrsize);

/* ----------------                 */

/**
 * 查找数据，返回匹配数据的在链表中的首地址和长度
 * @param compare 比较回调函数（data 链表中的数据首地址，
 *      datasize 为其长度， user 调用者比较数据的首地址，
 *      usrsize 其长度， 返回0时则视为相等数据，停止遍历）
 * @param user 调用者比较数据的首地址
 * @param usrsize 比较的长度
 * @param datasize 当比较成功时，返回链表中数据的长度
 * @return NULL 没有找到匹配的数据， 否则成功匹配，返回链表中数据的地址
 */
const char *SListLookup(SListT list, NodeCompareCB compare, void *user, int usrsize, int *datasize);

/* ----------------                 */

/**
 * 删除查找到的数据
 * @param data 必须是SListLookup()返回的地址
 */
void SListDelData(SListT list, const char *nodedata);

/* ----------------                 */

/**
 * 通过节点数据在其后新增一个节点
 */
bool SListInsertNextData(SListT list, const char *nodedata, char *buff, int size, int *effectsize);

/**
 * 通过节点数据在其前新增一个节点
 */
bool SListInsertPrevData(SListT list, const char *nodedata, char *buff, int size, int *effectsize);

/**
 * 通过节点数据获取当前上一个节点数据
 * @param data 必须是SListLookup()返回的地址
 */
const char *SListGetNextData(SListT list, const char *nodedata);

/**
 * 通过节点数据获取当前下一个节点数据
 * @param data 必须是SListLookup()返回的地址
 */
const char *SListGetPrevData(SListT list, const char *nodedata);

/* ----------------                 */

/**
 * 计算返回要容纳指定容量的数据的链表中实际节点的长度
 */
long SListCalculateNodeSize(int datasize);

/**
 * 计算返回要容纳指定数据量和数据大小的链表的总长度
 */
long SListCalculateSize(int nodetotal, int datasize);

/* ----------------                 */
/** 锁链表 */
void SListLock(SListT list);

/** 解锁链表 */
void SListUnlock(SListT list);

/** 增加关联数 */
void SListAddRefs(SListT list);

/** 减少关联数 */
int SListDecRefs(SListT list);

/** 获取关联数 */
int SListGetRefs(SListT list);

/* ----------------                 */
__END_DECLS
#endif	/* ifndef __libmini_slist_h__ */

