//
//  atomic.c
//  gcc内建原子操作封装
//  自旋锁、线程自旋锁实现
//  Created by 周凯 on 14/07.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#include <signal.h>
#include <sched.h>
#include <sys/syscall.h>
#include "atomic/atomic.h"
#include "ipc/futex.h"
#include <assert.h>

#ifndef _AO_CPU_NUMS
  #define _AO_CPU_NUMS (1)
#endif

#ifndef _AO_SPIN_CHECKBOLT
  #define _AO_SPIN_CHECKBOLT (2046)
#endif

#ifndef _AO_SPIN_SPINMAX
  #define _AO_SPIN_SPINMAX      (256)
#endif

#define _AO_SPIN_UNLOCK         (AO_LOCK_UNLOCK)

/*
 * 高位存pid／低位存lock-value
 * 进行了算数右移，但由于高位32必然是一个正数，所以不影响。
 */
#define _CalculateLock(value)					   \
	((((int64_t)GetProcessID()) << 32) & 0xffffffff00000000) | \
	((int32_t)(value) & 0x00000000ffffffff)

#define _GetPIDByLock(lock) \
	(int32_t)((((int64_t)(lock)) & 0xffffffff00000000) >> 32)

#define _GetValueByLock(lock) \
	(int32_t)(((int64_t)(lock)) & 0x00000000ffffffff)

#define _AO_SPIN_LOCKOP(l, v) \
	(ATOMIC_CASB(&(l)->lock, _AO_SPIN_UNLOCK, _CalculateLock((v))))

#define _AO_SPIN_UNLOCKOP(l, v)	\
	(ATOMIC_CASB(&(l)->lock, _CalculateLock((v)), _AO_SPIN_UNLOCK))

static __thread unsigned long g_AOLockChecker = 0;

static bool _CheckAndRepairLock(AO_SpinLockT *lock, AO_T val)
{
	/*
	 * 每隔一定的时间检查一次
	 */
	if (likely(lock->shared)) {
		g_AOLockChecker++;

		if (unlikely((g_AOLockChecker % _AO_SPIN_CHECKBOLT) == 0)) {
			int64_t old = 0;
			int32_t pid = 0;

			old = lock->lock;
			barrier();
			pid = _GetPIDByLock(old);

#ifdef DEBUG
			fprintf(stderr, LOG_S_COLOR
				">>> CHECK LOCK <<<"
				PALETTE_NULL "\n");
#endif

			/*判定锁持有进程是否存活*/
			if (likely(pid > 0)) {
				int flag = 0;

				flag = kill(pid, 0);

				if (unlikely((flag < 0) && (errno == ESRCH))) {
					/*
					 * 持有进程已关闭
					 * 此时lock->pid被设置成有效的pid，已保证只有一个检查进程成功重置死锁
					 */
					bool ret = 0;
					ret = ATOMIC_CASB(&lock->lock, old, _CalculateLock(val));

					if (likely(ret)) {
#ifdef DEBUG
						fprintf(stderr, LOG_S_COLOR
							">>> REPAIR LOCK <<<"
							PALETTE_NULL "\n");
#endif
						ATOMIC_CLEAR(&lock->counter);
						g_AOLockChecker = 0;
						return true;
					}
				}
			}
		}
	}

	sched_yield();

	return false;
}

/*
 * 如果当前调用者已拥有锁，返回-1；否则，返回当前线程id
 */
static inline int32_t _CheckLockOwner(AO_SpinLockT *lock)
{
	int32_t threadid = -1;

	ASSERTOBJ(lock);

	threadid = (int32_t)GetThreadID();
	return _GetValueByLock(lock->lock) == threadid ? -1 : threadid;
}

void (AO_SpinLockInit)(AO_SpinLockT *lock, bool shared)
{
	assert(lock);

	if (unlikely(ISOBJ(lock))) {
		if (likely(lock->shared)) {
			return;
		}
	}

	lock->shared = !!shared;
	ATOMIC_CLEAR(&lock->counter);
	ATOMIC_SET(&lock->lock, _AO_SPIN_UNLOCK);
	REFOBJ(lock);
}

bool (AO_SpinTryLock)(AO_SpinLockT *lock, AO_T val)
{
	bool flag = false;

	ASSERTOBJ(lock);

	flag = _AO_SPIN_LOCKOP(lock, val);

	if (unlikely(!flag)) {
		flag = _CheckAndRepairLock(lock, val);
	} else {
		g_AOLockChecker = 0;
	}

	return flag;
}

void (AO_SpinLock)(AO_SpinLockT *lock, AO_T val)
{
	ASSERTOBJ(lock);

	do {
		if (likely(_AO_SPIN_LOCKOP(lock, val))) {
			g_AOLockChecker = 0;
			return;
		}

#ifdef _AO_SPIN_SPINMAX
		int i = 0;

		for (i = 0; i < _AO_SPIN_SPINMAX; i++) {
			int j = 0;

			for (j = 0; j < i; j += 1) {
				/*原子锁忙等待算法*/
  #if 0
				_cpu_pause();
  #else
    #if 1
				if (unlikely(j & 0x111)) {
					sched_yield();
				} else {
					_cpu_pause();
				}

    #else
				if (likely(!!(j % 2))) {
					_cpu_pause();
				} else {
					sched_yield();
				}
    #endif
  #endif
			}

			if (likely(_AO_SPIN_LOCKOP(lock, val))) {
				g_AOLockChecker = 0;
				return;
			}
		}
#endif		/* ifdef _AO_SPIN_SPINMAX */
	} while (!_CheckAndRepairLock(lock, val));
}

void (AO_SpinUnlock)(AO_SpinLockT *lock, AO_T val)
{
	bool flag = false;

	flag = _AO_SPIN_UNLOCKOP(lock, val);

	if (unlikely(!flag)) {
#ifdef DEBUG
		fprintf(stderr, LOG_S_COLOR
			"In the absence of lock to unlock."
			PALETTE_NULL
			"\n");
#endif
		abort();
	}
}

bool AO_ThreadSpinTryLock(AO_SpinLockT *lock)
{
	bool    flag = false;
	int32_t value = 0;

	value = _CheckLockOwner(lock);

	if (value != -1) {
		flag = (AO_SpinTryLock)(lock, value);

		if (unlikely(!flag)) {
			return false;
		}
	}

	lock->counter++;
	return true;
}

void AO_ThreadSpinLock(AO_SpinLockT *lock)
{
	int32_t value = 0;

	value = _CheckLockOwner(lock);

	if (value != -1) {
		(AO_SpinLock)(lock, value);
	}

	lock->counter++;
}

void AO_ThreadSpinUnlock(AO_SpinLockT *lock)
{
#if 1
	int32_t value = 0;
	value = _CheckLockOwner(lock);

	if (unlikely(value != -1)) {
  #ifdef DEBUG
		fprintf(stderr, LOG_W_COLOR
			"In the absence of lock to unlock."
			PALETTE_NULL
			"\n");
  #endif
		abort();
	}
#endif

	if (lock->counter-- == 1) {
		(AO_SpinUnlock)(lock, _GetValueByLock(lock->lock));
	}
}

/* ---------------------                        */
#ifndef _AO_WAITTIME
  #define _AO_WAITTIME (50)
#endif

#ifndef _AO_WAKEWAITERS
  #define _AO_WAKEWAITERS (2)
#endif

#define _int64ptroffset(ptr, offset) \
	((int32_t *)(((char *)(ptr)) + ((offset) / sizeof(char))))

static inline volatile int32_t *_GetValuePtrByLockPtr(volatile int64_t *lockptr)
{
	if (likely(ISLITTLEEND())) {
		return _int64ptroffset(lockptr, 0);
	}

	return _int64ptroffset(lockptr, sizeof(int32_t));
}

bool AO_TryLock(AO_LockT *lock)
{
	return AO_ThreadSpinTryLock(lock);
}

void AO_Lock(AO_LockT *lock)
{
	bool flag = false;

again:
	flag = AO_ThreadSpinTryLock(lock);

	if (unlikely(!flag)) {
		int32_t                 lckval = 0;
		volatile int64_t        value = 0;
		volatile int32_t        *ptr = NULL;

		flag = AO_ThreadSpinTryLock(lock);

		if (likely(flag)) {
			return;
		}

		value = lock->lock;
		barrier();
		lckval = _GetValueByLock(value);
		ptr = _GetValuePtrByLockPtr(&lock->lock);

		futex_wait((int *)ptr, lckval, _AO_WAITTIME);

		goto again;
	}
}

void AO_Unlock(AO_LockT *lock)
{
	int16_t counter = 0;

	counter = lock->counter;
	AO_ThreadSpinUnlock(lock);

	if (counter == 1) {
		volatile int32_t *ptr = NULL;
		ptr = _GetValuePtrByLockPtr(&lock->lock);
		futex_wake((int *)ptr, _AO_WAKEWAITERS);
	}
}

