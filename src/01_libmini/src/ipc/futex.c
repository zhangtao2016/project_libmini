//
//  futex.c
//
//
//  Created by 周凯 on 15/9/2.
//
//

#include "ipc/futex.h"
#include "time/time.h"
#include "atomic/atomic.h"
#include "slog/slog.h"
#include "except/exception.h"

#ifdef __LINUX__

  #include <linux/futex.h>
  #include <sys/time.h>

  #include "time/time.h"

  #ifndef __NR_futex
    #define __NR_futex 202
  #endif

  #ifndef SYS_futex
    #define SYS_futex __NR_futex
  #endif

  #define __futex_op(addr1, op, val, rel, addr2, val3) \
	syscall(SYS_futex, addr1, op, val, rel, addr2, val3)

int futex_wait(int *uaddr, int val, int timeout)
{
	struct timespec tm = {};
	struct timespec *tmptr = NULL;
	int             flag = 0;

	if (likely(ATOMIC_GET(uaddr) != val)) {
		return -1;
	}

	if (timeout >= 0) {
		tm.tv_sec = timeout / 1000;
		tm.tv_nsec = (timeout % 1000) * 1000;
		tmptr = &tm;
	}

again:
	flag = __futex_op(uaddr, FUTEX_WAIT, val, tmptr, NULL, 0);

	if (unlikely(flag < 0)) {
		int code = errno;

		if (likely(code == EWOULDBLOCK)) {
			return -1;
		} else if (unlikely(code == ETIMEDOUT)) {
			return 0;
		} else if (unlikely(code == EINTR)) {
			goto again;
		} else {
			RAISE_SYS_ERROR(flag);
		}
	}

	return 1;
}

int futex_wake(int *uaddr, int n)
{
	int flag = 0;

	flag = __futex_op(uaddr, FUTEX_WAKE, n < 1 ? 32 : n, NULL, NULL, 0);
	RAISE_SYS_ERROR(flag);

	return flag;
}

#else
  #include <pthread.h>

/*
 * 离散使用条件变量的冲突
 * 只能通知到进程内部
 */
static struct
{
	int             waiter;
	pthread_cond_t  cond;
	pthread_mutex_t mutex;
} _futex_wait_ctl[] = {
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },	// 5
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },	// 10
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },	// 15
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
	{ 0, PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER },
};

int futex_wait(int *uaddr, int val, int timeout)
{
  #if 1
	assert(uaddr);
	int     flag = 0;
	int     of = 0;

	of = (((uintptr_t)uaddr) >> 3) % DIM(_futex_wait_ctl);

	if (likely(ATOMIC_GET(uaddr) != val)) {
		return -1;
	}

	pthread_mutex_lock(&_futex_wait_ctl[of].mutex);
	ATOMIC_INC(&_futex_wait_ctl[of].waiter);

	if (timeout >= 0) {
		struct timespec tm = {};
		TM_FillAbsolute(&tm, timeout);
		flag = pthread_cond_timedwait(&_futex_wait_ctl[of].cond,
				&_futex_wait_ctl[of].mutex, &tm);
	} else {
		flag = pthread_cond_wait(&_futex_wait_ctl[of].cond,
				&_futex_wait_ctl[of].mutex);
	}

	ATOMIC_DEC(&_futex_wait_ctl[of].waiter);
	pthread_mutex_unlock(&_futex_wait_ctl[of].mutex);

	if (unlikely(flag > 0)) {
		if (unlikely(flag != ETIMEDOUT)) {
			x_perror("pthread_cond_timedwait() error");
			abort();
		} else {
			return 0;
		}
	} else {
		return 1;
	}

  #else
	RAISE_SYS_ERROR_ERRNO(ENOTSUP);
	return -1;
  #endif/* if 1 */
}

int futex_wake(int *uaddr, int n)
{
  #if 1
	assert(uaddr);

	int     waiter;
	int     of = 0;

	of = (((uintptr_t)uaddr) >> 3) % DIM(_futex_wait_ctl);
	pthread_mutex_lock(&_futex_wait_ctl[of].mutex);
	pthread_cond_broadcast(&_futex_wait_ctl[of].cond);
	//	pthread_cond_signal(&_futex_wait_ctl[of].cond);
	waiter = _futex_wait_ctl[of].waiter;
	pthread_mutex_unlock(&_futex_wait_ctl[of].mutex);

	return waiter;

  #else
	RAISE_SYS_ERROR_ERRNO(ENOTSUP);
	return -1;
  #endif
}
#endif	/* ifdef __LINUX__ */

bool futex_cond_wait(int *uaddr, int until, int trys)
{
	int rmtrys = 0;

	assert(uaddr);

	while (1) {
		volatile int _old = 0;

		_old = *uaddr;

		if (likely(_old == until)) {
			break;
		} else if (unlikely((trys >= 0) && (++rmtrys >= trys))) {
			return false;
		}

		futex_wait(uaddr, _old, 1000);
	}

	return true;
}

