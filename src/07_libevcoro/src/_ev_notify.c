//
//  _ev_notify.c
//  libmini
//
//  Created by 周凯 on 15/12/5.
//  Copyright © 2015年 zk. All rights reserved.
//
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include "libevcoro/_ev_notify.h"
#include "libcoro/coro.h"

#ifdef __LINUX__

  #include <linux/futex.h>
  #include <sys/time.h>

  #include "../time/time.h"

  #ifndef __NR_futex
    #define __NR_futex 202
  #endif

  #ifndef SYS_futex
    #define SYS_futex __NR_futex
  #endif

  #define __futex_op(addr1, op, val, rel, addr2, val3) \
	syscall(SYS_futex, addr1, op, val, rel, addr2, val3)

static int _evcoro_wait(int *uaddr, int val, int timeout)
{
	struct timespec tm = {};
	struct timespec *tmptr = NULL;
	int             flag = 0;

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
			fprintf(stderr, "evcoro_wait() error : %s", strerror(errno));
			abort();
		}
	}

	return 1;
}

int _ev_notify_wake(int *uaddr, int n)
{
	int flag = 0;

	flag = __futex_op(uaddr, FUTEX_WAKE, n < 1 ? 32 : n, NULL, NULL, 0);

	if (unlikely(flag < 0)) {
		fprintf(stderr, "evcoro_wait() error : %s", strerror(errno));
		abort();
	}

	return flag;
}

#else
  #include <pthread.h>

  #undef  DIM
  #define DIM(x) ((int)(sizeof((x)) / sizeof((x)[0])))

/*
 * 离散使用条件变量的冲突
 * 只能通知到进程内部
 */
static struct
{
	int             waiter;
	pthread_cond_t  cond;
	pthread_mutex_t mutex;
} _evcoro_wait_ctl[] = {
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

static int _evcoro_wait(int *uaddr, int val, int timeout)
{
	assert(uaddr);
	int     flag = 0;
	int     of = 0;

	of = (((uintptr_t)uaddr) >> 3) % DIM(_evcoro_wait_ctl);

	if (likely(*uaddr != val)) {
		errno = EWOULDBLOCK;
		return -1;
	}

	pthread_mutex_lock(&_evcoro_wait_ctl[of].mutex);
	_evcoro_wait_ctl[of].waiter++;

	if (timeout >= 0) {
		struct timespec ts = {};
		struct timeval  tv = {};

		gettimeofday(&tv, NULL);

		tv.tv_sec += timeout / 1000;
		tv.tv_usec += (timeout % 1000) * 1000;

		if (tv.tv_usec > 1000000) {
			tv.tv_sec += 1;
			tv.tv_usec -= 1000000;
		}

		ts.tv_sec = tv.tv_sec;
		ts.tv_nsec = (long)(tv.tv_usec * 1000);

		flag = pthread_cond_timedwait(&_evcoro_wait_ctl[of].cond,
				&_evcoro_wait_ctl[of].mutex, &ts);
	} else {
		flag = pthread_cond_wait(&_evcoro_wait_ctl[of].cond,
				&_evcoro_wait_ctl[of].mutex);
	}

	_evcoro_wait_ctl[of].waiter--;
	pthread_mutex_unlock(&_evcoro_wait_ctl[of].mutex);

	if (unlikely(flag > 0)) {
		if (unlikely(flag != ETIMEDOUT)) {
			fprintf(stderr, "pthread_cond_timedwait() error : %s", strerror(flag));
			abort();
		} else {
			return 0;
		}
	} else {
		return 1;
	}
}

int _ev_notify_wake(int *uaddr, int n)
{
	assert(uaddr);

	int     waiter;
	int     of = 0;

	of = (((uintptr_t)uaddr) >> 3) % DIM(_evcoro_wait_ctl);
	pthread_mutex_lock(&_evcoro_wait_ctl[of].mutex);
	pthread_cond_broadcast(&_evcoro_wait_ctl[of].cond);
	waiter = _evcoro_wait_ctl[of].waiter;
	pthread_mutex_unlock(&_evcoro_wait_ctl[of].mutex);

	return waiter;
}
#endif	/* ifdef __LINUX__ */

