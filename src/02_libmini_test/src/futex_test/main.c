#include <sys/shm.h>
#include <pthread.h>
#include "libmini.h"

static const key_t shmkey = 0xf0000002;

enum
{
	FT_OFF,
	FT_ON,
};

struct FutexTest
{
	int     magic;
	int     stat;
};

struct Args
{
	struct FutexTest        *ft;
	int                     secop;
};

void *workwait(void *arg)
{
	struct Args *ptr = (struct Args *)arg;

	assert(ptr && ptr->ft);

	/*等待状态变化*/
	int     flag = 0;
	bool    modify = false;

again:
	flag = futex_wait(&ptr->ft->stat, FT_OFF, ptr->secop);

	switch (flag)
	{
		case 0:
			x_printf(D, "timed out ...");
			modify = false;
			break;

		case 1:
			x_printf(D, "wait standby ...");
			modify = true;
			break;

		default:
			x_printf(D, "already standby ...");
			modify = true;
			break;
	}

	if (modify) {
		bool flag = false;

		flag = ATOMIC_CASB(&ptr->ft->stat, FT_ON, FT_OFF);

		if (likely(flag)) {
			x_printf(D, "modified successful ...");
		} else {
			x_printf(D, "modified failure, continue wait ...");
			goto again;
		}
	}

	return NULL;
}

int main(int argc, char **argv)
{
	char                    prg[32] = {};
	bool                    iswaitop = false;
	bool                    iscreate = false;
	int                     secops = 0;
	int                     shmid = 0;
	struct FutexTest        *ftptr = NULL;
	struct Args             arg = {};

	/* ----------- parse arguments ----------- */

	if (unlikely(argc != 3)) {
		x_printf(E, "Usage %s <wait/wake> <#sleeps/waiters>", GetProgName(prg, sizeof(prg)));
		return EXIT_FAILURE;
	}

	if (strcasecmp(argv[1], "wait") == 0) {
		iswaitop = true;
	} else if (strcasecmp(argv[1], "wake") == 0) {
		iswaitop = false;
	} else {
		x_printf(E, "Usage %s <wait/wake> <#sleeps/waiters>", GetProgName(prg, sizeof(prg)));
		return EXIT_FAILURE;
	}

	secops = atoi(argv[2]);
	secops = secops <= 0 ? 1 : secops;

	/* ----------- create or link shared memory ----------- */
	shmid = shmget(shmkey, sizeof(*ftptr), IPC_CREAT | IPC_EXCL | SVIPC_MODE);

	if (shmid < 0) {
		int flag = 0;

		struct shmid_ds shmds = {};

		if (unlikely(errno != EEXIST)) {
			RAISE_SYS_ERROR(shmid);
		}

		shmid = shmget(shmkey, 0, SVIPC_MODE);
		RAISE_SYS_ERROR(shmid);

		flag = shmctl(shmid, IPC_STAT, &shmds);
		RAISE_SYS_ERROR(flag);

		AssertError(shmds.shm_segsz == sizeof(*ftptr), EINVAL);
	} else {
		iscreate = true;
	}

	ftptr = (struct FutexTest *)shmat(shmid, NULL, 0);

	if (unlikely((intptr_t)ftptr < 0)) {
		int code = errno;

		if (iscreate) {
			shmctl(shmid, IPC_RMID, NULL);
		}

		errno = code;
		RAISE(EXCEPT_SYS);
	}

	bool ret = false;

	if (iscreate) {
		ftptr->stat = FT_OFF;
		//                REFOBJ(ftptr);
		ret = futex_set_signal(&ftptr->magic, OBJMAGIC, 128);
		assert(ret);
	} else {
		ret = futex_cond_wait(&ftptr->magic, OBJMAGIC, -1);

		if (unlikely(!ret)) {
			x_printf(E, "Tried so many times.");
		}

		//                while (!ISOBJ(ftptr)) {
		//                        usleep(10);
		//                }
	}

	/* ----------- test start ----------- */

	arg.ft = ftptr;
	arg.secop = secops;

	if (iswaitop) {
		/*等待状态变化*/
#if 0
		int             i = 0;
		pthread_t       tid[2] = {};

		for (i = 0; i < DIM(tid); i++) {
			pthread_create(&tid[i], NULL, workwait, &arg);
		}

		for (i = 0; i < DIM(tid); i++) {
			pthread_join(tid[i], NULL);
		}

#else
		workwait(&arg);
#endif
	} else {
		/*唤醒等待的进程*/
		bool flag = false;
		flag = ATOMIC_CASB(&ftptr->stat, FT_OFF, FT_ON);

		if (likely(flag)) {
			x_printf(D, "it's necessary to wake somebody ...");
			futex_wake(&ftptr->stat, secops);
		} else {
			x_printf(D, "it's not necessary to wake somebody ...");
		}
	}

	return EXIT_SUCCESS;
}

