//
//  fork.h
//
//
//  Created by 周凯 on 15/8/28.
//
//

#ifndef __minilib__fork__
#define __minilib__fork__

#include <sys/stat.h>
#include <sys/wait.h>
#include "../utils.h"
#include "../io/io_base.h"
#include "../except/exception.h"

__BEGIN_DECLS

/*
 * 在忽略SIGCHLD信号的情况下回收子进程，将导致获取子进程结束状态异常。
 */
/* --------------------------------        */
/*fork()之后需要在子进程初始化一些全局变量，以保证日志信息的和原子锁的正确性*/

void InitProcess();

/* --------------------------------        */

/*
 * 创建进程
 */
#define     ForkFrontEndStart(pid)     \
	STMT_BEGIN		       \
	pid_t _pid = -1;	       \
	_pid = fork();		       \
	RAISE_SYS_ERROR(_pid);	       \
	SET_POINTER((pid), _pid);      \
	switch (_pid)		       \
	{			       \
		case 0:		       \
			InitProcess(); \
			umask(0)

/*
 * 结束进程
 */
#define     ForkFrontEndTerm() \
	exit(EXIT_SUCCESS);    \
	break;		       \
	default:	       \
		break;	       \
	}		       \
	STMT_END

/* --------------------------------        */

/*
 * 回收进程
 */
#define     ForkWaitProcess(pid, wait)					      \
	do {								      \
		int     _pStat = -1;					      \
		pid_t   _pid;						      \
		_pid = waitpid((pid), &_pStat, (wait) ? 0 : WNOHANG);	      \
		if (unlikely(_pid < 0)) {				      \
			if (likely(errno == EINTR)) {			      \
				continue;				      \
			} else {					      \
				RAISE(EXCEPT_SYS);			      \
			}						      \
		}							      \
		if (likely((_pid == pid) && WIFEXITED(_pStat) &&	      \
			(WEXITSTATUS(_pStat) == EXIT_SUCCESS))) {	      \
			x_printf(D, "Child process [%d] normal exit.", _pid); \
		} else {						      \
			x_printf(D, "Child process [%d] exit by %d.", _pid,   \
				WEXITSTATUS(_pStat));			      \
		}							      \
		break;							      \
	} while (1)

/*
 * 回收所有进程
 */
#define     ForkWaitAllProcess()			 \
	do {						 \
		volatile bool _cntwait = true;		 \
		TRY					 \
		{					 \
			ForkWaitProcess(0, true);	 \
		}					 \
		EXCEPT(EXCEPT_SYS)			 \
		{					 \
			if (unlikely(errno != ECHILD)) { \
				RERAISE;		 \
			}				 \
			_cntwait = false;		 \
		}					 \
		END;					 \
		if (!_cntwait) {			 \
			break;				 \
		}					 \
	} while (1)

/* --------------------------------        */

/*
 * ForkFrontEndStart()中调用，创建后台进程
 */
#define     ForkBackEndStart()				  \
	STMT_BEGIN					  \
	pid_t _subPid = -1;				  \
	int     _fd0 = -1;				  \
	int     _fd1 = -1;				  \
	int     _fd2 = -1;				  \
	signal(SIGHUP, SIG_IGN);			  \
	setsid();					  \
	_subPid = fork();				  \
	RAISE_SYS_ERROR(_subPid);			  \
	switch (_subPid)				  \
	{						  \
		case 0:					  \
			InitProcess();			  \
			FD_CloseAll();			  \
			_fd0 = open("/dev/null", O_RDWR); \
			_fd1 = dup2(0, 1);		  \
			_fd2 = dup2(0, 2)

/*
 * 结束后台进程
 */
#define     ForkBackEndTerm()	    \
	exit(EXIT_SUCCESS);	    \
	break;			    \
	default:		    \
		exit(EXIT_SUCCESS); \
		break;		    \
	}			    \
	STMT_END

/* --------------------------------        */
	__END_DECLS
#endif	/* defined(____fork__) */

