//
//  futex.h
//  在非linux平台上只能用于线程间的同步，不能用于进程间的同步。
//
//  Created by 周凯 on 15/9/2.
//
//

#ifndef __minilib___futex__
#define __minilib___futex__

#include "../common.h"

__BEGIN_DECLS

/* -------------                        */

/*
 * fast user - condition, wait until [*uaddr] = [until]
 * the interval time is 1s for each test
 */
bool futex_cond_wait(int *uaddr, int until, int trys);

/*
 * fast user - set signal, set [*uptr] = cond and wake up a waiter on [uptr]
 */
#define futex_set_signal(uptr, cond, waiters)	       \
	({					       \
		bool _ret = false;		       \
		int _old = -1;			       \
		_old = ATOMIC_SWAP((uptr), (cond));    \
		if (likely(_old != (cond))) {	       \
			futex_wake((uptr), (waiters)); \
			_ret = true;		       \
		}				       \
		_ret;				       \
	})

/*
 * fast user - add signal, increment [*uptr]++  and wake up a waiter on [uptr]
 */
#define futex_add_signal(uptr, inc, waiters) \
	STMT_BEGIN			     \
	ATOMIC_ADD((uptr), (inc));	     \
	futex_wake((uptr), (waiters));	     \
	STMT_END

/*
 * fast user - sub signal, decrease [*uptr]--  and wake up a waiter on [uptr]
 */
#define futex_sub_signal(uptr, dec, waiters) \
	STMT_BEGIN			     \
	ATOMIC_SUB((uptr), (dec));	     \
	futex_wake((uptr), (waiters));	     \
	STMT_END

/* -------------                        */

/**
 * 在uaddr指向的这个地址上挂起等待（仅当*uaddr == val时，挂起）
 * @timeout 超时毫秒数，timeout < 0则挂起进程直到测试地址的值变化
 * @return -1 挂起前*uaddr的值在挂起前已不等于val；0 超时；1 被唤醒
 */
int futex_wait(int *uaddr, int val, int timeout);

/**
 * 唤醒n个在uaddr指向的地址上挂起等待的进程
 * @return 被唤醒的进程数
 */
int futex_wake(int *uaddr, int n);

/* ------------                         */
__END_DECLS
#endif	/* defined(__minilib___futex__) */

