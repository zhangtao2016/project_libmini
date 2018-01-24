//
//  atomic.h
//  gcc内建原子操作封装
//  自旋锁、线程自旋锁声明
//  Created by 周凯 on 14/07.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __libmini_atomic_h__
#define __libmini_atomic_h__

#include "../utils.h"

__BEGIN_DECLS

#ifndef AO_T
typedef volatile long AO_T;
#endif

#if (GCC_VERSION >= 40100)

/* 内存访问栅 */
  #define barrier()             (__sync_synchronize())
/* 原子获取 */
// #define ATOMIC_GET(ptr)                 (barrier(), ( __typeof__ (*( ptr ))) ( (ATOMIC_GET)((AO_T*)( ptr ))))
  #define ATOMIC_GET(ptr)       ({ __typeof__(*(ptr)) volatile *_val = (ptr); barrier(); (*_val); })
// #define ATOMIC_GET(ptr)                 (barrier(), ( __typeof__ (*( ptr ))) ( *( ptr )))

/* 原子设置 */
  #define ATOMIC_SET(ptr, value)        ((void)__sync_lock_test_and_set((ptr), (value)))

/* 原子交换，如果被设置，则返回旧值，否则返回设置值 */
  #define ATOMIC_SWAP(ptr, value)       ((__typeof__(*(ptr)))__sync_lock_test_and_set((ptr), (value)))

/* 原子比较交换，如果当前值等于旧值，则新值被设置，返回旧值，否则返回新值*/
  #define ATOMIC_CAS(ptr, comp, value)  ((__typeof__(*(ptr)))__sync_val_compare_and_swap((ptr), (comp), (value)))

/* 原子比较交换，如果当前值等于旧指，则新值被设置，返回真值，否则返回假 */
  #define ATOMIC_CASB(ptr, comp, value) (__sync_bool_compare_and_swap((ptr), (comp), (value)) != 0 ? true : false)

/* 原子清零 */
  #define ATOMIC_CLEAR(ptr)             ((void)__sync_lock_release((ptr)))

/* maths/bitop of ptr by value, and return the ptr's new value */
  #define ATOMIC_ADD_F(ptr, value)      ((__typeof__(*(ptr)))__sync_add_and_fetch((ptr), (value)))
  #define ATOMIC_SUB_F(ptr, value)      ((__typeof__(*(ptr)))__sync_sub_and_fetch((ptr), (value)))
  #define ATOMIC_OR_F(ptr, value)       ((__typeof__(*(ptr)))__sync_or_and_fetch((ptr), (value)))
  #define ATOMIC_AND_F(ptr, value)      ((__typeof__(*(ptr)))__sync_and_and_fetch((ptr), (value)))
  #define ATOMIC_XOR_F(ptr, value)      ((__typeof__(*(ptr)))__sync_xor_and_fetch((ptr), (value)))

  #define ATOMIC_F_ADD(ptr, value)      ((__typeof__(*(ptr)))__sync_fetch_and_add((ptr), (value)))
  #define ATOMIC_F_SUB(ptr, value)      ((__typeof__(*(ptr)))__sync_fetch_and_sub((ptr), (value)))
  #define ATOMIC_F_OR(ptr, value)       ((__typeof__(*(ptr)))__sync_fetch_and_or((ptr), (value)))
  #define ATOMIC_F_AND(ptr, value)      ((__typeof__(*(ptr)))__sync_fetch_and_and((ptr), (value)))
  #define ATOMIC_F_XOR(ptr, value)      ((__typeof__(*(ptr)))__sync_fetch_and_xor((ptr), (value)))

#else

  #error "can not supported atomic operate by gcc(v4.0.1+) buildin function."
#endif	/* if (GCC_VERSION >= 40100) */

#define ATOMIC_INC(ptr)                 ((void)ATOMIC_ADD_F((ptr), 1))
#define ATOMIC_DEC(ptr)                 ((void)ATOMIC_SUB_F((ptr), 1))
#define ATOMIC_ADD(ptr, val)            ((void)ATOMIC_ADD_F((ptr), (val)))
#define ATOMIC_SUB(ptr, val)            ((void)ATOMIC_SUB_F((ptr), (val)))

/* set some bits on(1) of ptr, and return the ptr's new value */
#define ATOMIC_BIT_ON(ptr, mask)        ATOMIC_OR((ptr), (mask))

/* set some bits off(0) of ptr, and return the ptr's new value */
#define ATOMIC_BIT_OFF(ptr, mask)       ATOMIC_AND((ptr), ~(mask))

/* exchange some bits(1->0/0->1) of ptr, and return the ptr's new value */
#define ATOMIC_BIT_XCHG(ptr, mask)      ATOMIC_XOR((ptr), (mask))

/* -------------------                  */

/*
 * 魔数
 * 结构体中设置一个magic的成员变量，已检查结构体是否被正确初始化
 */
#if !defined(OBJMAGIC)
  #define OBJMAGIC (0xfedcba98)
#endif

/*原子的设置魔数*/
#undef REFOBJ
#define REFOBJ(obj)						     \
	({							     \
		int _old = 0;					     \
		bool _ret = false;				     \
		if (likely((obj))) {				     \
			_old = ATOMIC_SWAP(&(obj)->magic, OBJMAGIC); \
		}						     \
		_ret = (_old == OBJMAGIC ? false : true);	     \
		_ret;						     \
	})

/*原子的重置魔数*/
#undef UNREFOBJ
#define UNREFOBJ(obj)							\
	({								\
		bool _ret = false;					\
		if (likely((obj))) {					\
			_ret = ATOMIC_CASB(&(obj)->magic, OBJMAGIC, 0);	\
		}							\
		_ret;							\
	})

/*原子的验证魔数*/
#undef ISOBJ
#define ISOBJ(obj) ((obj) && ATOMIC_GET(&(obj)->magic) == OBJMAGIC)

/*断言魔数*/
#undef ASSERTOBJ
#define ASSERTOBJ(obj) (assert(ISOBJ((obj))))

/* -------------------                  */

/*
 * 原子自旋锁
 */
typedef struct
{
	volatile int32_t        magic;
	volatile int16_t        shared;
	volatile int16_t        counter;
	volatile int64_t        lock;
} AO_SpinLockT;

#define AO_LOCK_INLOCK          (1)
#define AO_LOCK_UNLOCK          (0)

/*
 * 静态初始化对象宏
 */
#define NULLOBJ_AO_SPINLOCK     { OBJMAGIC, 0, 0, AO_LOCK_UNLOCK }

void    (AO_SpinLockInit)(AO_SpinLockT *lock, bool shared);
bool    (AO_SpinTryLock)(AO_SpinLockT *lock, AO_T val);
void    (AO_SpinLock)(AO_SpinLockT *lock, AO_T val);
void    (AO_SpinUnlock)(AO_SpinLockT *lock, AO_T val);

/*
 * 自旋原子锁，临界区短。
 * 设置shared为true，可以用于进程间同步的死锁（进程持有锁退出的情况）自检修正。
 */
#define AO_SpinLockInit(l, s) \
	((AO_SpinLockInit)((l), (s)))

#define AO_SpinTryLock(l) \
	((AO_SpinTryLock)((l), (AO_T)AO_LOCK_INLOCK))

#define AO_SpinLock(l) \
	((AO_SpinLock)((l), (AO_T)AO_LOCK_INLOCK))

#define AO_SpinUnlock(l) \
	((AO_SpinUnlock)((l), (AO_T)AO_LOCK_INLOCK))

/*
 * 线程级别同步自旋锁，可以实现递归调用，临界区短。
 */
#define AO_ThreadSpinLockInit(l, s) \
	((AO_SpinLockInit)((l), (s)))

bool AO_ThreadSpinTryLock(AO_SpinLockT *lock);

void AO_ThreadSpinLock(AO_SpinLockT *lock);

void AO_ThreadSpinUnlock(AO_SpinLockT *lock);

/* ----------------                    */

/*
 * 使用 Linux 2.45 以上内核提供的 futex 实现一种原子锁，临界区长。
 */
#define AO_LockInit(l, s) \
	((AO_SpinLockInit)((l), (s)))

typedef AO_SpinLockT AO_LockT;
bool AO_TryLock(AO_LockT *lock);

void AO_Lock(AO_LockT *lock);

void AO_Unlock(AO_LockT *lock);

/* ----------------                    */
__END_DECLS
#endif	/* ifndef __libmini_atomic_h__ */

