#include <sys/shm.h>
#include <sched.h>
#include <pthread.h>
#include "libmini.h"

const static int        defchilds = 2;
const static int        maxchilds = 40;
const static int        minms = 1;
const static int        defloops = 120;
const static key_t      key = 0xf0000001;
static int              shmid = 0;

struct counter
{
	int                     magic;
	int                     counter;
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

void exitaction()
{
	if (shmctl(shmid, IPC_RMID, NULL) < 0) {
		x_perror("shmctl() by IPC_RMID failure : %s", x_strerror(errno));
	} else {
		x_printf(D, "delete shared memory ...");
	}
}

void childwork(struct counter *ptr, int loops, int ms)
{
	x_printf(D, "child %06d Start ...", getpid());

	while (!ISOBJ(ptr)) {
		usleep(100);
	}

	x_printf(D, "child %06d Work ...", getpid());

	int i = 0;

	for (i = 0; i < loops; i++) {
#if defined(USE_MUTEX)
		pthread_mutex_lock(&ptr->locker);
#elif defined(USE_SPINMUTEX)
		pthread_spin_lock(&ptr->locker);
#elif defined(USE_FUTEX)
		AO_Lock(&ptr->locker);
#else
		AO_SpinLock(&ptr->locker);
#endif

#ifdef DEBUG
		x_printf(D, "counter : %d", ptr->counter++);
#else
		ptr->counter++;
#endif

		/*模拟一个慢速调用，测试找到一个慢速调用的平衡点*/
		if (ms > 0) {
			usleep(ms * 100);
		}

#if defined(USE_MUTEX)
		pthread_mutex_unlock(&ptr->locker);
#elif defined(USE_SPINMUTEX)
		pthread_spin_unlock(&ptr->locker);
#elif defined(USE_FUTEX)
		AO_Unlock(&ptr->locker);
#else
		AO_SpinUnlock(&ptr->locker);
#endif
		// usleep(10);
	}

	x_printf(D, "child %06d Over ...", getpid());
}

int main(int argc, char **argv)
{
	int             childs = 0;
	int             i = 0;
	int             ms = 0;
	int             loops = 0;
	char            name[32] = {};
	pid_t           *childspid = NULL;
	struct counter  *ptr = NULL;

	if (unlikely(argc != 4)) {
		x_perror("Usage %s <#childs> <#loop> <#sleep>", GetProgName(name, sizeof(name)));
		return EXIT_FAILURE;
	}

	childs = atoi(argv[1]);

	if (childs > 0) {
		childs = INRANGE(childs, defchilds, maxchilds);
		NewArray0(childs, childspid);
	}

	loops = atoi(argv[2]);
	loops = loops < 1 ? defloops : loops;

	ms = atoi(argv[3]);

	// ms = ms < 1 ? minms : ms;

	/*映射内存*/
	shmid = shmget(key, ADJUST_SIZE(sizeof(struct counter), sizeof(long)), IPC_CREAT | IPC_EXCL | SVIPC_MODE);

	if (unlikely(shmid < 0)) {
		x_perror("shmget() for 0x%08x failure : %s", key, x_strerror(errno));
		return EXIT_FAILURE;
	}

	ptr = shmat(shmid, NULL, 0);

	if (unlikely((intptr_t)ptr < 0)) {
		x_perror("shmat() for 0x%08x failure : %s", key, x_strerror(errno));
		return EXIT_FAILURE;
	}

	if (childs > 0) {
		for (i = 0; i < childs; i++) {
			ForkFrontEndStart(&childspid[i]);

			BindCPUCore((i + 1) % GetCPUCores());

			childwork(ptr, loops, ms);

			ForkFrontEndTerm();
		}

		atexit(exitaction);
		sleep(2);
	} else {
		atexit(exitaction);
	}

#if defined(USE_MUTEX)
	pthread_mutexattr_t attr = {};
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&ptr->locker, &attr);
	pthread_mutexattr_destroy(&attr);
#elif defined(USE_SPINMUTEX)
	pthread_spin_init(&ptr->locker, PTHREAD_PROCESS_SHARED);
#elif defined(USE_FUTEX)
	AO_LockInit(&ptr->locker, true);
#else
	AO_SpinLockInit(&ptr->locker, true);
#endif

	REFOBJ(ptr);

	if (childs < 1) {
		childwork(ptr, loops, ms);
	}

	x_printf(D, "Wait all of children ...");

	ForkWaitAllProcess();

	x_printf(D, "Result : %d", ptr->counter);

	return EXIT_SUCCESS;
}

