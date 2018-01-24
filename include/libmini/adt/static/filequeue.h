//
//  filequeue.h
//
//
//  Created by 周凯 on 15/9/1.
//
//

#ifndef __libmini__filequeue__
#define __libmini__filequeue__

#include "slist.h"
#include "squeue.h"

__BEGIN_DECLS

/* ---------------                  */

/*
 * 创建文件内存队列
 */
typedef struct FileQueue *FileQueueT;

struct FileQueue
{
	int     magic;
	int     fd;
	char    *file;
	SQueueT data;
};

/**
 * 初始化
 * @param file 文件名
 * @param nodetotal 节点数量，如果小于等于0，则从已存在的文件中加载
 * @param nodesize 节点大小，如果小于等于0，则从已存在的文件中加载
 */
FileQueueT File_QueueCreate(const char *file, int nodetotal, int nodesize);

bool File_QueuePush(FileQueueT queue, char *buff, int size, int *effectsize);

bool File_QueuePriorityPush(FileQueueT queue, char *buff, int size, int *effectsize);

bool File_QueuePull(FileQueueT queue, char *buff, int size, int *effectsize);

void File_QueueDestroy(FileQueueT *queue, bool shmrm, NodeDestroyCB destroy);

__END_DECLS
#endif	/* defined(__libmini__filequeue__) */

