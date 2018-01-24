//
//  memqueue.c
//  libmini
//
//  Created by 周凯 on 15/9/14.
//  Copyright © 2015年 zk. All rights reserved.
//

#include "adt/static/memqueue.h"
#include "slog/slog.h"
#include "except/exception.h"
#include "ipc/ipc_base.h"

MemQueueT MEM_QueueCreate(const char *name, int nodetotal, int nodesize)
{
	long                    totalsize = 0;
	char *volatile          buff = NULL;
	volatile MemQueueT      queue = NULL;
	SQueueT                 data = NULL;

	totalsize = SQueueCalculateSize(nodetotal, nodesize);

	TRY
	{
		New(queue);

		return_val_if_fail(queue, NULL);

		NewArray(totalsize, buff);
		AssertError(buff, ENOMEM);

		data = SQueueInit(buff, totalsize, nodesize, false);
		ASSERTOBJ(data);

		queue->data = data;
		REFOBJ(queue);
	}
	CATCH
	{
		Free(buff);
		Free(queue);
	}
	END;

	return queue;
}

bool MEM_QueuePush(MemQueueT queue, char *buff, int size, int *effectsize)
{
	ASSERTOBJ(queue);
	return SQueuePush(queue->data, buff, size, effectsize);
}

bool MEM_QueuePriorityPush(MemQueueT queue, char *buff, int size, int *effectsize)
{
	ASSERTOBJ(queue);
	return SQueuePriorityPush(queue->data, buff, size, effectsize);
}

bool MEM_QueuePull(MemQueueT queue, char *buff, int size, int *effectsize)
{
	ASSERTOBJ(queue);
	return SQueuePull(queue->data, buff, size, effectsize);
}

void MEM_QueueDestroy(MemQueueT *queue, bool shmrm, NodeDestroyCB destroy)
{
	assert(queue);

	return_if_fail(UNREFOBJ(*queue));

	if (shmrm) {
		char *volatile buff = NULL;
		TRY
		{
			if (destroy) {
				int     size = 0;
				int     effect = 0;

				size = (*queue)->data->datasize;
				NewArray(size, buff);
				AssertError(buff, ENOMEM);

				while (SQueuePull((*queue)->data, buff, size, &effect)) {
					TRY
					{
						destroy(buff, effect);
					}
					CATCH
					{}
					END;
				}
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

		UNREFOBJ((*queue)->data);

		Free((*queue)->data);
	}

	Free(*queue);
}

