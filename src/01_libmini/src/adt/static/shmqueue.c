#include <sys/shm.h>
#include <sched.h>
#include "adt/static/shmqueue.h"
#include "slog/slog.h"
#include "except/exception.h"
#include "ipc/ipc_base.h"
#include "ipc/futex.h"
#include "string/string.h"
/* -----------------                            */

#ifndef SHM_QUEUE_TRYS
  #define SHM_QUEUE_TRYS 1000
#endif
/* -----------------                            */

/* -----------------                            */

ShmQueueT SHM_QueueCreate(key_t shmkey, int nodetotal, int nodesize)
{
	ShmQueueT volatile      queue = NULL;
	void *volatile          aptr = NULL;
	int volatile            shmid = -1;
	bool volatile           iscreate = true;

	TRY
	{
		New(queue);
		AssertError(queue, ENOMEM);

		int     ipcflag = SVIPC_MODE;
		long    totalsize = 0;

		if ((nodetotal > 0) && (nodesize > 0)) {
			ipcflag |= IPC_CREAT | IPC_EXCL;
			totalsize = SQueueCalculateSize(nodetotal, nodesize);
		} else {
			iscreate = false;
		}

		shmid = shmget(shmkey, totalsize, ipcflag);

		if (shmid < 0) {
			iscreate = false;
			/*totalsize为0则一定错误*/
			AssertError(totalsize != 0, ENOENT);
			/*totalsize不为0，则错误不能不是EEXIST*/
			AssertRaise(errno == EEXIST, EXCEPT_SYS);
		}

		/*获取id或长度，并检查长度*/
		if ((shmid < 0) || (totalsize == 0)) {
			int             flag = 0;
			struct shmid_ds shmds = {};

			shmid = shmget(shmkey, 0, SVIPC_MODE);
			RAISE_SYS_ERROR(shmid);

			flag = shmctl(shmid, IPC_STAT, &shmds);
			RAISE_SYS_ERROR(shmid);

			if (unlikely((totalsize > 0) && (shmds.shm_segsz != totalsize))) {
				x_printf(E, "SHM_QueueCreate() by `0x%08x` failed : "
					"expected the length of "
					"shared memory is `%ld`, but get `%zu`.",
					shmkey, totalsize, shmds.shm_segsz);
				RAISE_SYS_ERROR_ERRNO(EINVAL);
			} else {
				totalsize = shmds.shm_segsz;
			}
		}

		aptr = shmat(shmid, NULL, 0);
		RAISE_SYS_ERROR((intptr_t)aptr);

		SQueueT data = NULL;

		if (iscreate) {
			data = SQueueInit((char *)aptr, totalsize, nodesize, true);
			ASSERTOBJ(data);
		} else {
			bool flag = false;

			data = (SQueueT)aptr;
			/*等待其他进程初始化*/
			flag = futex_cond_wait(&data->magic, OBJMAGIC, SHM_QUEUE_TRYS);
			AssertError(flag, ENOENT);

			flag = SQueueCheck((char *)aptr, totalsize, nodesize, false);
			AssertError(flag, EINVAL);
		}

		/*增加计数器*/
		ATOMIC_INC(&data->refs);
		queue->data = data;
		queue->shmid = shmid;
		queue->shmkey = shmkey;
		REFOBJ(queue);
	}
	CATCH
	{
		if (aptr) {
			shmdt(aptr);
		}

		if (iscreate) {
			shmctl(shmid, IPC_RMID, NULL);
		}

		Free(queue);
	}
	END;

	return queue;
}

ShmQueueT SHM_QueueCreateByPath(const char *path, int nodetotal, int nodesize)
{
	volatile ShmQueueT      queue = NULL;
	char *volatile          filepath = NULL;
	volatile bool           iscreate = true;
	volatile int            fd = -1;
	key_t                   key = 0;

	assert(path);

	TRY
	{
		fd = open(path, O_RDONLY | O_CREAT | O_EXCL, FILE_MODE);

		if (unlikely(fd < 0)) {
			iscreate = false;

			if (unlikely(errno != EEXIST)) {
				RAISE(EXCEPT_SYS);
			}

			fd = open(path, O_RDONLY, 0);
			RAISE_SYS_ERROR(fd);
		}

		key = ftok(path, 1);
		RAISE_SYS_ERROR((int)key);

		filepath = x_strdup(path);
		AssertError(filepath, ENOMEM);

		queue = SHM_QueueCreate(key, nodetotal, nodesize);

		assert(queue);
		queue->path = filepath;
	}
	CATCH
	{
		if (iscreate && (fd > -1)) {
			unlink(path);
		}

		Free(filepath);
	}
	FINALLY
	{
		if (likely(fd > -1)) {
			close(fd);
		}
	}
	END;

	return queue;
}

bool SHM_QueuePush(ShmQueueT queue, char *buff, int size, int *effectsize)
{
	ASSERTOBJ(queue);
	return SQueuePush(queue->data, buff, size, effectsize);
}

bool SHM_QueuePriorityPush(ShmQueueT queue, char *buff, int size, int *effectsize)
{
	ASSERTOBJ(queue);
	return SQueuePriorityPush(queue->data, buff, size, effectsize);
}

bool SHM_QueuePull(ShmQueueT queue, char *buff, int size, int *effectsize)
{
	ASSERTOBJ(queue);
	return SQueuePull(queue->data, buff, size, effectsize);
}

void SHM_QueueDestroy(ShmQueueT *queue, bool shmrm, NodeDestroyCB destroy)
{
	int     flag = -1;
	SQueueT data = NULL;

	assert(queue);
	return_if_fail(UNREFOBJ(*queue));

	data = (*queue)->data;

	if (shmrm) {
		char *volatile buff = NULL;
		TRY
		{
			int refs = 0;
			refs = ATOMIC_GET(&data->refs);

			if (unlikely(refs > 1)) {
				struct shmid_ds shmds = {};

				flag = shmctl((*queue)->shmid, IPC_STAT, &shmds);
				RAISE_SYS_ERROR(flag);

				refs = shmds.shm_nattch;
			}

			if (unlikely(refs > 1)) {
				ATOMIC_DEC(&data->refs);
			} else {
				/*在未detach的情况下，将变成进程私有到共享内存*/
				flag = shmctl((*queue)->shmid, IPC_RMID, NULL);
				RAISE_SYS_ERROR(flag);

				if (destroy) {
					int     effect = 0;
					int     size = 0;

					size = data->datasize;
					NewArray(size, buff);
					AssertError(buff, ENOMEM);

					while (SQueuePull(data, buff, size, &effect)) {
						TRY
						{
							destroy(buff, effect);
						}
						CATCH
						{}
						END;
					}
				}

				UNREFOBJ(data);
			}
		}
		CATCH
		{
			REFOBJ((*queue));
			RERAISE;
		}
		FINALLY
		{
			Free(buff);
		}
		END;
	}

	Free(*queue);

	flag = shmdt(data);
	RAISE_SYS_ERROR(flag);
}

