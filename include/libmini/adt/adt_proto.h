//
//  adt_proto.h
//
//
//  Created by 周凯 on 15/9/1.
//
//

#ifndef __libmini__adt_proto__
#define __libmini__adt_proto__

#include "../utils.h"

__BEGIN_DECLS

/* -------------                        */

/**
 * 节点数据遍历接口
 * 应用规则：当返回false时停止遍历
 */
typedef bool (*NodeTravelCB)(const char *data, int size, void *user);

/**
 * 节点数据比较接口
 *   ＊ 应用规则：内部数据小于／等于／大于外部数据分别返回-1／0／1。
 */
typedef int (*NodeCompareCB)(const char *data, int datasize, void *user, int usrsize);

/**
 * 节点数据销毁接口
 */
typedef void (*NodeDestroyCB)(const char *data, int size);

/**
 * 散列函数接口
 */
typedef uint32_t (*HashCB)(const char *key, int keyLen);
/* -------------                        */

/*
 * 常用hash辅助函数/容量计算
 */

/**
 * key键默认比较函数，作为整数处理
 */
int HashCompareDefaultCB(const char *data, int datasize, void *user, int usrsize);

/**
 * hash默认计算函数，作为整数处理
 */
uint32_t HashDefaultCB(const char *key, int keyLen);

/*
 * 哈希值计算函数
 * @param data 被计算的键，可以为NULL
 * @param dataLen 被计算的键的长度，如果为负数，则将data看作字符串
 * @return 0 ~ 0xffffffffU的哈希值
 */
uint32_t HashFlower(const char *key, int keyLen);

uint32_t HashTime33(const char *key, int keyLen);

uint32_t HashIgnoreCaseTime33(const char *key, int keyLen);

uint32_t HashReduceBit(const char *key, int keyLen);

uint32_t HashPJW(const char *key, int keyLen);

/**
 * 判断给定的数值是否为质数
 */
bool IsPrime(unsigned candidate);

/**
 * 返回给定数值对应的下一质数
 */
int NextPrime(unsigned seed);

/**
 * 计算哈希表的长度
 */
int CalculateTableSize(unsigned hintsize);

/* -------------                        */
__END_DECLS
#endif	/* defined(__libmini__adt_proto__) */

