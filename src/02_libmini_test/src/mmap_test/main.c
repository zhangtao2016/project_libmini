#include <semaphore.h>
#include <sys/mman.h>
#include <sched.h>
#include "libmini.h"

#ifndef MMAP_QUEUE_TRYS
  #define MMAP_QUEUE_TRYS 1000
#endif

static char g_SemName[] = "mysemaphorename";

struct counter
{
	/* data */
	int     counter;
	int     magic;
};

int main(int argc, char **argv)
{
	int             fd = -1;
	int             i = 0;
	int             loop = 0;
	bool            iscreate = false;
	sem_t           *mutex = NULL;
	struct counter  *ptr = NULL;
	char            semname[MAX_PATH_SIZE] = {};

	if (unlikely(argc != 3)) {
		x_printf(E, "Usage %s <path> <#loop>", argv[0]);
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_CREAT | O_EXCL | O_RDWR, FILE_MODE);

	if (likely(fd < 0)) {
		if (unlikely(errno != EEXIST)) {
			x_printf(F, "open %s failure : %s", argv[0], x_strerror(errno));
			return EXIT_FAILURE;
		}

		fd = open(argv[1], O_RDWR, FILE_MODE);

		if (unlikely(fd < 0)) {
			x_printf(F, "open %s failure : %s", argv[0], x_strerror(errno));
			return EXIT_FAILURE;
		}
	} else {
		iscreate = true;
	}

	if (iscreate) {
		ftruncate(fd, sizeof(struct counter));
	}

	ptr = (struct counter *)mmap(NULL, sizeof(struct counter), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (unlikely(ptr == MAP_FAILED)) {
		x_printf(F, "mmap failure : %s", x_strerror(errno));
		return EXIT_FAILURE;
	}

	if (iscreate) {
		REFOBJ(ptr);
	} else {
		int trys = MMAP_QUEUE_TRYS;

		while (!ISOBJ(ptr)) {
			if (unlikely(--trys < 0)) {
				x_printf(E, "Too many attempts");
				return EXIT_FAILURE;
			}

			sched_yield();
		}
	}

	iscreate = false;
	mutex = sem_open(PosixIPCName(semname, sizeof(semname), g_SemName), O_CREAT | O_EXCL, FILE_MODE, 1);

	if (unlikely(mutex == SEM_FAILED)) {
		if (unlikely(errno != EEXIST)) {
			x_printf(F, "sem_open %s failure : %s", semname, x_strerror(errno));
			return EXIT_FAILURE;
		}

		mutex = sem_open(semname, 0, 0, 0);

		if (unlikely(mutex == SEM_FAILED)) {
			x_printf(F, "sem_open %s failure : %s", semname, x_strerror(errno));
			return EXIT_FAILURE;
		}
	} else {
		//                sem_unlink(semname);
		iscreate = true;
	}

	loop = atoi(argv[2]);

	if (loop > 0) {
		for (i = 0; i < loop; i++) {
			sem_wait(mutex);
			printf("P : %06d, counter : %06d.\n", (int)getpid(), ++ptr->counter);
			sem_post(mutex);
		}
	} else {
		sem_wait(mutex);
		printf("P : %06d, counter : %06d.\n", (int)getpid(), ptr->counter);
		sem_post(mutex);
	}

	sem_close(mutex);

	if (iscreate) {
		sem_unlink(semname);
	}

	return EXIT_SUCCESS;
}

