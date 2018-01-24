//
//  filelock.c
//  libmini
//
//  Created by 周凯 on 15/10/9.
//  Copyright © 2015年 zk. All rights reserved.
//

#include "filesystem/filelock.h"
#include "slog/slog.h"
#include "except/exception.h"

/*
 * 文件锁基础控制
 */
bool FL_LockBase(int fd, int cmd, short type, short whence, off_t start, off_t len)
{
	struct flock    lock = {};
	int             flag = -1;

	assert(fd > -1);

	lock.l_len = len;
	lock.l_start = start;
	lock.l_type = type;
	lock.l_whence = whence;

	flag = fcntl(fd, cmd, &lock);

	if (unlikely(flag < 0)) {
		int code = errno;

		if (unlikely((code != EACCES) && (code != EAGAIN))) {
			RAISE(EXCEPT_SYS);
		}
	}

	return flag == 0 ? true : false;
}

/*
 * 文件锁测试
 */
pid_t FL_LockTest(int fd, short type, short whence, off_t start, off_t len)
{
	struct flock    lock = {};
	int             flag = -1;

	assert(fd > -1);

	lock.l_len = len;
	lock.l_start = start;
	lock.l_whence = whence;
	lock.l_type = type;

	flag = fcntl(fd, F_GETLK, &lock);
	RAISE_SYS_ERROR(flag);

	return lock.l_type == F_UNLCK ? 0 : (lock.l_pid);
}

