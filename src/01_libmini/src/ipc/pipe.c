//
//  pipe.c
//  minilib
//
//  Created by 周凯 on 15/8/24.
//  Copyright (c) 2015年 zk. All rights reserved.
//
#include "ipc/pipe.h"
#include "slog/slog.h"
#include "except/exception.h"

/**
 * 创建并打开命名管道
 */
int CreateFIFO(const char *path, int flag, int mode)
{
	int     fd = -1;
	int     creat = false;

	assert(path);

	if (flag & O_TRUNC) {
		unlink(path);
	}

	if ((flag & O_CREAT) && (mkfifo(path, mode) < 0)) {
		if (unlikely(errno != EEXIST)) {
			x_printf(S, "create fifo fail %s.", path);
			return -1;
		}
	} else {
		creat = true;
	}

	//        flag &= ~O_TRUNC;
	flag &= ~O_CREAT;
	flag &= ~O_EXCL;

	fd = open(path, flag, 0);

	if (unlikely(fd < 0)) {
		if (creat) {
			unlink(path);
		}

		x_printf(S, "open fifo fail %s.", path);

		return -1;
	}

	return fd;
}

