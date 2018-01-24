//
//  pool.h
//  supex
//
//  Created by 周凯 on 16/1/7.
//  Copyright © 2016年 zhoukai. All rights reserved.
//

#ifndef pool_h
#define pool_h

#include "../utils.h"

__BEGIN_DECLS

/**
 * 该接口为池提供创造，检查，销毁成员所有
 * @return =0，成功； <0，失败
 */
typedef int (*pool_callback)(intptr_t *element, void *usr);

struct pool;

/**
 * 通过名称创造池对象
 * @param name 池名称
 * @param max 池最大容量
 * @param sync 是否阻塞获取成员，如果创造成员失败也会中断获取
 * @param create 创造成员回调，当池中没有成员，且池未满时调用
 * @param destroy 销毁成员回调，当成员状态错误，不需要时调用
 * @param checker 检查成员回调，当从池中获取已有的成员时调用
 * @return 返回0时，表示成功，<0时，表示失败，且设置errno，一般为ENOMEM(内存不足)，或EEXISTS(池已经存在)
 */
int pool_create(const char *name, unsigned max, bool sync, pool_callback create,
	pool_callback destroy, pool_callback checker);

/**
 * 销毁池
 * @param name 池名称
 * @param usr 销毁池中对象的回调入参
 */
void pool_destroy(const char *name, void *usr);

/**
 * 通过名称获取池对象
 * @param name 池名称
 * @return 池对象，返回null，则表示不存在，但不会设置errno
 * 不能主动释放获取到的指针
 */
struct pool     *pool_gain(const char *name);

/**
 * 通过池对象获取成员
 * @param pool 池对象
 * @param element 如果成功，成员被存入在此地址中
 * @param usr 如果池中没有成员且池未满，则此参数传入创造函数或检查函数，然后调用
 * @return 返回0时，表示成功，<0时，表示失败，且设置errno，一般为ENOMEM(池已满)，或创造成员函数设置的错误
 * 如果池被设置为阻塞获取，则一直阻塞到获取一个成员，或创造成员失败退出
 */
int pool_element_pull(struct pool *pool, intptr_t *element, void *usr);

/**
 * 通过池对象归还成员
 * @param pool 池对象
 * @param element 归还的成员
 */
void pool_element_push(struct pool *pool, intptr_t element);

/**
 * 通过池对象销毁成员，在成员状态错误且不能再用时调用此函数
 * @param pool 池对象
 * @param element 归还的成员
 * @param usr 此参数传入销毁函数，然后调用
 * @return 返回值为销毁函数的返回值
 */
int pool_element_free(struct pool *pool, intptr_t element, void *usr);

/**
 * 通过池名称获取成员
 */
static inline
int pool_gain_element(const char *name, intptr_t *element, void *usr)
{
	struct pool *pool = NULL;

	pool = pool_gain(name);

	if (unlikely(!pool)) {
		return -1;
	}

	return pool_element_pull(pool, element, usr);
}

__END_DECLS
#endif	/* pool_h */
