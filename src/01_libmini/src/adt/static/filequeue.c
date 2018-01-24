//
//  filequeue.c
//
//
//  Created by 周凯 on 15/9/1.
//
//
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "adt/static/filequeue.h"

#include "slog/slog.h"
#include "except/exception.h"
#include "ipc/ipc_base.h"
#include "ipc/futex.h"
#include "filesystem/filelock.h"
#include "filesystem/filesystem.h"
#include "string/string.h"

/* -----------------                            */

#ifndef FILE_QUEUE_TRYS
  #define FILE_QUEUE_TRYS 1000
#endif

#ifndef FILE_QUEUE_PSIZE
  #define FILE_QUEUE_PSIZE 4096
#endif
/* -----------------                            */

FileQueueT File_QueueCreate(const char *file, int nodetotal, int nodesize)
{
	volatile int            fd = -1;
	volatile bool           iscreate = true;
	void *volatile          mptr = NULL;
	char *volatile          filepath = NULL;
	FileQueueT volatile     queue = NULL;

	TRY
	{
		SQueueT data = NULL;
		long    totalsize = 0;
		long    filesize = 0;

		New(queue);
		AssertError(queue, ENOMEM);

		fd = open(file, O_CREAT | O_EXCL | O_RDWR, FILE_MODE);

		if (fd < 0) {
			iscreate = false;
			AssertRaise(errno == EEXIST, EXCEPT_SYS);

			fd = open(file, O_RDWR, 0);
			RAISE_SYS_ERROR(fd);
		}

		/*当节点大小和节点数量同时有效时，计算文件大小和映射大小，否则使用文件本身的大小*/
		if ((nodetotal > 0) && (nodesize > 0)) {
			totalsize = SQueueCalculateSize(nodetotal, nodesize);
			filesize = ADJUST_SIZE(totalsize, GetPageSize());
		} else if (!iscreate) {
			/*不是本次创建，则获取文件大小*/
			filesize = FS_GetSize(file);
			totalsize = filesize;
		} else {
			/*没有指定节点大小和节点数量，又是新创建，则抛出错误*/
			RAISE_SYS_ERROR_ERRNO(ENOENT);
		}

		if (iscreate) {
			int flag = 0;
			flag = ftruncate(fd, filesize);
			RAISE_SYS_ERROR(flag);
		} else if ((nodetotal > 0) && (nodesize > 0)) {
			long    fsize = 0;
			int     trys = FILE_QUEUE_TRYS;

			while (1) {
				fsize = FS_GetSize(file);

				if (likely(fsize == filesize)) {
					break;
				} else {
					usleep(1);
				}

				if (unlikely(--trys < 0)) {
					x_printf(E, "File_QueueCreate() by %s failed : "
						"expected the size of "
						"file is `%ld`, but get `%ld`.",
						file, totalsize, fsize);
					RAISE_SYS_ERROR_ERRNO(EINVAL);
				}
			}
		}

		mptr = mmap(NULL, totalsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		AssertRaise(mptr != MAP_FAILED, EXCEPT_SYS);

		filepath = x_strdup(file);
		AssertError(filepath, ENOMEM);

		if (iscreate) {
			data = SQueueInit((char *)mptr, totalsize, nodesize, true);
			ASSERTOBJ(data);
		} else {
			bool flags = false;
			data = (SQueueT)mptr;
			/*等待其他进程初始化*/
			flags = futex_cond_wait(&data->magic, OBJMAGIC, FILE_QUEUE_TRYS);
			AssertError(flags, ENOENT);
			flags = SQueueCheck((char *)mptr, totalsize, nodesize, false);
			AssertError(flags, EINVAL);
		}

		/*
		 * 锁住指定字节，防止被其他进程意外销毁文件锁
		 */
		int     offset = 0;
		int     flag = 0;
		offset = ATOMIC_F_ADD(&data->refs, 1);
		flag = FL_ReadTryLock(fd, SEEK_SET, offset, 1);
		AssertError(flag, EBUSY);

		queue->file = filepath;
		queue->data = data;
		queue->fd = fd;

		REFOBJ(queue);
	}
	CATCH
	{
		if (iscreate && (fd > -1)) {
			unlink(file);
		}

		close(fd);

		Free(queue);
		Free(filepath);
	}
	END;

	return queue;
}

bool File_QueuePush(FileQueueT queue, char *buff, int size, int *effectsize)
{
	ASSERTOBJ(queue);
	return SQueuePush(queue->data, buff, size, effectsize);
}

bool File_QueuePriorityPush(FileQueueT queue, char *buff, int size, int *effectsize)
{
	ASSERTOBJ(queue);
	return SQueuePriorityPush(queue->data, buff, size, effectsize);
}

bool File_QueuePull(FileQueueT queue, char *buff, int size, int *effectsize)
{
	ASSERTOBJ(queue);
	return SQueuePull(queue->data, buff, size, effectsize);
}

void File_QueueDestroy(FileQueueT *queue, bool shmrm, NodeDestroyCB destroy)
{
	int     flag = 0;
	SQueueT data = NULL;

	assert(queue);

	return_if_fail(UNREFOBJ(*queue));

	data = (*queue)->data;

	if (shmrm) {
		char *volatile buff = NULL;
		TRY
		{
			/*试图锁住整个文件，然后销毁数据*/
			int refs = 0;
			refs = ATOMIC_GET(&data->refs);

			if (unlikely(refs > 1)) {
				if (likely(FL_WriteTryLock((*queue)->fd, SEEK_SET, 0, 0))) {
					refs = 1;
				}
			}

			if (unlikely(refs > 1)) {
				ATOMIC_DEC(&data->refs);
			} else {
				flag = unlink((*queue)->file);
				RAISE_SYS_ERROR(flag);

				if (destroy) {
					int     size = 0;
					int     effect = 0;

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

	Free((*queue)->file);
	close((*queue)->fd);
	Free(*queue);

	flag = munmap(data, data->totalsize);
	RAISE_SYS_ERROR(flag);
}

