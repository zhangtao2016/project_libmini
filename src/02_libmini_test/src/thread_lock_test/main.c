#include <sys/shm.h>
#include <sched.h>
#include <pthread.h>
#include "libmini.h"

const static int        defchilds = 2;
const static int        maxchilds = 40;

const static int        defloops = 120;
const static int        maxloops = INT32_MAX;

struct testarg
{
	int                     magic;
	int                     counter;
	int                     loops;
	int                     ms;
#if defined(USE_MUTEX)
	pthread_mutex_t         locker;
#elif defined(USE_SPINMUTEX)
	pthread_spinlock_t      locker;
#elif defined(USE_FUTEX)
	AO_LockT                locker;
#else
	AO_SpinLockT            locker;
#endif
};

void *work(void *arg)
{
	int             i = 0;
	struct testarg  *test = (struct testarg *)arg;

	BindCPUCore(-1);

	assert(test);

	while (!ISOBJ(test)) {
		// usleep(1);
		sched_yield();
	}

	for (i = 0; i < test->loops; i++) {
#if defined(USE_MUTEX)
		pthread_mutex_lock(&test->locker);
#elif defined(USE_SPINMUTEX)
		pthread_spin_lock(&test->locker);
#elif defined(USE_FUTEX)
		AO_Lock(&test->locker);
#else
		AO_ThreadSpinLock(&test->locker);
  #if 0
		AO_ThreadSpinLock(&test->locker);
  #endif
#endif

#ifdef DEBUG
		x_printf(D, "%06ld counter : %d", GetThreadID(), test->counter++);
#else
		test->counter++;
#endif

		/*模拟一个慢速调用，测试找到一个慢速调用的平衡点*/
		if (test->ms > 0) {
			usleep(test->ms * 100);
		}

#if defined(USE_MUTEX)
		pthread_mutex_unlock(&test->locker);
#elif defined(USE_SPINMUTEX)
		pthread_spin_unlock(&test->locker);
#elif defined(USE_FUTEX)
		AO_Unlock(&test->locker);
#else
  #if 0
		AO_ThreadSpinUnlock(&test->locker);
  #endif
		AO_ThreadSpinUnlock(&test->locker);
#endif
	}

	return NULL;
}

int main(int argc, char **argv)
{
	char            prg[32] = {};
	int             threads = 0;
	int             loops = 0;
	int             i = 0;
	int             ms = 0;
	pthread_t       *tid = NULL;
	struct testarg  test = {};

	if (unlikely(argc != 4)) {
		x_printf(E, "Usage %s <#threads> <#loops> <#sleep>",
			GetProgName(prg, sizeof(prg)));

		return EXIT_FAILURE;
	}

	threads = atoi(argv[1]);
	loops = atoi(argv[2]);
	ms = atoi(argv[3]);

	threads = INRANGE(threads, defchilds, maxchilds);
	loops = INRANGE(loops, defloops, maxloops);

	NewArray0(threads, tid);

	AssertError(tid, ENOMEM);

	for (i = 0; i < threads; i++) {
		pthread_create(&tid[i], NULL, work, &test);
	}

	test.loops = loops;
	test.ms = ms;

#if defined(USE_MUTEX)
	pthread_mutexattr_t attr = {};
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&test.locker, &attr);
	pthread_mutexattr_destroy(&attr);
#elif defined(USE_SPINMUTEX)
	pthread_spin_init(&test.locker, PTHREAD_PROCESS_SHARED);
#elif defined(USE_FUTEX)
	AO_LockInit(&test.locker, false);
#else
	AO_SpinLockInit(&test.locker, false);
#endif
	usleep(10);

	REFOBJ(&test);

	for (i = 0; i < threads; i++) {
		pthread_join(tid[i], NULL);
	}

	assert(threads * loops == test.counter);

	return EXIT_SUCCESS;
}

