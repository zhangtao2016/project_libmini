//
//  filelock.h
//  libmini
//
//  Created by 周凯 on 15/10/9.
//  Copyright © 2015年 zk. All rights reserved.
//

#ifndef __minilib_filelock_h__
#define __minilib_filelock_h__

#include "../utils.h"
__BEGIN_DECLS

bool FL_LockBase(int fd, int cmd, short type, short whence, off_t start, off_t len);

pid_t FL_LockTest(int fd, short type, short whence, off_t start, off_t len);

/* -----------------------------------            */

/*
 * 阻塞式写锁
 */
#define FL_WriteLock(fd, whence, start, len) \
	(FL_LockBase((fd), F_SETLKW, F_WRLCK, (whence), (start), (len)))

/*
 * 阻塞式读锁
 */
#define FL_ReadLock(fd, whence, start, len) \
	(FL_LockBase((fd), F_SETLKW, F_RDLCK, (whence), (start), (len)))

/*
 * 非阻塞式写锁
 */
#define FL_WriteTryLock(fd, whence, start, len)	\
	(FL_LockBase((fd), F_SETLK, F_WRLCK, (whence), (start), (len)))

/*
 * 非阻塞式读锁
 */
#define FL_ReadTryLock(fd, whence, start, len) \
	(FL_LockBase((fd), F_SETLK, F_RDLCK, (whence), (start), (len)))

/*
 * 解锁
 */
#define FL_Unlock(fd, whence, start, len) \
	(FL_LockBase((fd), F_SETLK, F_UNLCK, (whence), (start), (len)))

/*
 * 测试锁是否可写
 * 返回 = 0 表示可写
 * 返回 > 0 表示有进程号为该值的进程持有该锁
 * 返回 = -1 表示出错
 */
#define FL_WriteIsReady(fd, whence, start, len)	\
	(FL_LockTest((fd), F_WRLCK, (whence), (start), (len)))

/*
 * 测试锁是否可读
 * 返回 = 0 表示可读
 * 返回 > 0 表示有进程号为该值的进程持有该锁
 * 返回 = -1 表示出错
 */
#define FL_ReadIsReady(fd, whence, start, len) \
	(FL_LockTest((fd), F_RDLCK, (whence), (start), (len)))

__END_DECLS
#endif	/* __minilib_filelock_h__ */

