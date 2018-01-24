//
//  pool.c
//  supex
//
//  Created by 周凯 on 16/1/7.
//  Copyright © 2016年 zhoukai. All rights reserved.
//

#include "pool_api/pool.h"
struct pool
{
	int             magic;
	unsigned        capacity;
	unsigned        counter;
	bool            sync;
	char            *name;
	pool_callback   create;
	pool_callback   destroy;
	pool_callback   checker;
	DQueueT         data;
};

#ifndef POOL_HASH_SIZE
  #define POOL_HASH_SIZE 100
#endif

#if POOL_HASH_SIZE < 1
  #error "ERROR PARAMRTER `POOL_HASH_SIZE`"
#endif

static DHashT g_pool_hash = NULL;

static void _pool_hash_init_() __attribute__((constructor()));

static struct pool      *_pool_new(unsigned max, bool sync, pool_callback create,
	pool_callback destroy, pool_callback checker);

/* -------			*/
int pool_create(const char *name, unsigned max, bool sync, pool_callback create,
	pool_callback destroy, pool_callback checker)
{
	assert(name);

	struct pool *volatile pool = NULL;

	TRY
	{
		bool flag = false;
		pool = _pool_new(max, sync, create, destroy, checker);
		flag = DHashSet(g_pool_hash, name, -1, (char *)&pool, sizeof(pool));
		AssertRaise(flag, EXCEPT_SYS);

		pool->name = x_strdup(name);

		ReturnValue(0);
	}
	CATCH
	{
		x_perror("Pool `%s` create fail", name);

		if (pool) {
			DQueueDestroy(&pool->data, NULL);
			Free(pool);
		}
	}
	END;

	return -1;
}

void pool_destroy(const char *name, void *usr)
{
	assert(name);
	
	struct pool *volatile pool = NULL;
	
	TRY
	{
		bool flag = false;
		unsigned size = sizeof(pool);
		flag = DHashGet(g_pool_hash, name, -1, (char *)&pool, &size);
		AssertRaise(flag, EXCEPT_SYS);
		flag = DHashDel(g_pool_hash, name, -1);
		AssertRaise(flag, EXCEPT_SYS);
	}
	CATCH
	{
		x_perror("Pool `%s` destory fail", name);
	}
	FINALLY
	{
		if (UNREFOBJ(pool)) {
			DQueueT queue = pool->data;
			intptr_t element = 0;
			while (DQueuePull(queue, &element)) {
				TRY
				{
					pool->destroy(&element, usr);
				}
				CATCH
				{}
				END;
			}
			Free(pool);
		}
	}
	END;
}

struct pool *pool_gain(const char *name)
{
	struct pool     *pool = NULL;
	unsigned        size = sizeof(pool);
	bool            flag = false;

	flag = DHashGet(g_pool_hash, name, -1, (char *)&pool, &size);

	if (unlikely(!flag)) {
		x_perror("Pool `%s` not exists.", name);
		pool = NULL;
	} else {
		assert(size == sizeof(pool));
	}

	return pool;
}

int pool_element_pull(struct pool *pool, intptr_t *element, void *usr)
{
	ASSERTOBJ(pool);
	assert(element);

	do {
		/* 从队列获取 */
		while (likely(DQueuePull(pool->data, element))) {
			if (pool->checker) {
				/*检查成员，检查失败则释放成员，继续从队列中获取*/
				if (unlikely(pool->checker(element, usr) != 0)) {
					pool_element_free(pool, *element, usr);
					continue;
				}
			}

			return 0;
		}
		/*队列中没有可用成员，则开始创造一个新的成员，消耗一个计数*/
		unsigned        idx = pool->counter;
		bool            ok = false;

		/*判断池是否已满，如果是同步方式，则等待，否则失败退出*/
		if (unlikely(idx == pool->capacity)) {
			if (pool->sync) {
				/*等待一会，归还成员或计数都将被唤醒*/
				futex_wait((int *)&pool->counter, pool->capacity, 100);
				/*继续从头再次尝试获取*/
				continue;
			} else {
				x_printf(E, "Pool `%s` is full!", pool->name);
				errno = ENOMEM;
				return -1;
			}
		}

		/*抢占计数，如果成功，则创造一个新的成员*/
		ok = ATOMIC_CASB(&pool->counter, idx, idx + 1);

		if (likely(ok)) {
			break;
		}
	} while (1);

	/*获得了计数，创造成员*/
	if (unlikely(pool->create(element, usr) != 0)) {
		/*创造失败，释放已占计数*/
		ATOMIC_DEC(&pool->counter);
		/*唤醒等待的消费者*/
		futex_wake((int *)&pool->counter, 1);
		return -1;
	}

	return 0;
}

void pool_element_push(struct pool *pool, intptr_t element)
{
	ASSERTOBJ(pool);
	assert(DQueuePush(pool->data, element));
	/*唤醒等待的消费者*/
	futex_wake((int *)&pool->counter, 1);
}

int pool_element_free(struct pool *pool, intptr_t element, void *usr)
{
	ASSERTOBJ(pool);
	/*释放计数*/
	assert(ATOMIC_F_SUB(&pool->counter, 1) > 0);
	/*唤醒等待的消费者*/
	futex_wake((int *)&pool->counter, 1);

	return pool->destroy(&element, usr);
}

/* -------			*/
static struct pool *_pool_new(unsigned max, bool sync, pool_callback create,
	pool_callback destroy, pool_callback check)
{
	assert(max && create && destroy);

	struct pool *volatile pool = NULL;
	TRY
	{
		New(pool);
		AssertError(pool, ENOMEM);

		pool->data = DQueueCreate(max);
		AssertError(pool->data, ENOMEM);
		pool->capacity = max;
		pool->counter = 0;
		pool->create = create;
		pool->destroy = destroy;
		pool->checker = check;
		pool->sync = sync;

		REFOBJ(pool);
	}
	CATCH
	{
		if (pool) {
			DQueueDestroy(&pool->data, NULL);
			Free(pool);
		}

		RERAISE;
	}
	END;

	return pool;
}

/* -------			*/

static void _pool_hash_init_()
{
	g_pool_hash = DHashCreate(POOL_HASH_SIZE, HashTime33, NULL);

	assert(g_pool_hash);
}
