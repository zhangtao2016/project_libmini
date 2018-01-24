//
//  io_base.h
//  minilib
//
//  Created by 周凯 on 15/8/24.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __minilib__io_base__
#define __minilib__io_base__

#include "../utils.h"

__BEGIN_DECLS

/* -----------------------                      */

/* 描述符的相关操作 */

/**
 * 写测试
 * @return -1，出错；0，就绪
 */
int FD_CheckWrite(int fd, long ms);

/**
 * 读测试
 * @return -1，出错；0，就绪
 */
int FD_CheckRead(int fd, long ms);

/*＊关闭所有描述符*/
int FD_CloseAll();

/*标志为开关*/
#define FD_GetFlag(fd)				  \
	({					  \
		int _rc = 0;			  \
		_rc = fcntl((fd), F_GETFL, NULL); \
		RAISE_SYS_ERROR(_rc);		  \
		_rc;				  \
	})

#define FD_SetFlag(fd, flag)		    \
	STMT_BEGIN			    \
	int _rc = 0;			    \
	_rc = fcntl((fd), F_SETFL, (flag)); \
	RAISE_SYS_ERROR(_rc);		    \
	STMT_END

#define FD_Enable(fd, flag)		   \
	STMT_BEGIN			   \
	int _rc = 0;			   \
	_rc = fcntl((fd), F_GETFL, NULL);  \
	RAISE_SYS_ERROR(_rc);		   \
	_rc |= (flag);			   \
	_rc = fcntl((fd), F_SETFL, (_rc)); \
	RAISE_SYS_ERROR(_rc);		   \
	STMT_END

/* -----------------------                      */

/*
 * [xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx...] -> buff
 * |<-----------size--------->|
 *    ^nlstart
 *                      ^nlend('\n')
 */

struct LineBuff
{
	int     magic;
	int     nlstart;		/**<本次/上次找到的行起始下标*/
	int     nlend;			/**<本次/上次找到的行终止下标*/
	int     size;			/**<当前使用掉的空间大小*/
	char    buff[MAX_LINE_SIZE];	/**<数据缓冲*/
};

#define NULLOBJ_LINEBUFF { OBJMAGIC, -1, -1, 0, {} }

void LineBuffInit(struct LineBuff *buff);

ssize_t ReadLine(int fd, struct LineBuff *buff);

char *GetLine(struct LineBuff *buff);

/* -----------------------                      */

__END_DECLS
#endif	/* defined(__minilib__io_base__) */

