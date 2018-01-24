//
//  except.h
//
//
//  Created by 周凯 on 15/8/28.
//
//

#ifndef __minilib__except__
#define __minilib__except__

#include <setjmp.h>
#include "../utils.h"

__BEGIN_DECLS

/* -------------------                          */

/*
 * 断言表达式，抛出一个指定的异常
 */
#define AssertRaise(e, except) \
	(likely((e)) ? 1 : (RAISE((except)), 0))

/*
 * 断言表达式，抛出一个系统调用错误异常
 */
#define AssertError(e, error) \
	(likely((e)) ? 1 : (errno = (error), RAISE(EXCEPT_SYS), 0))

/*
 * 抛出指定错误值的系统调用异常
 * 不为0则为错误
 */
#define RAISE_SYS_ERROR_ERRNO(x) \
	(unlikely((errno = (x)) != 0) ? RAISE(EXCEPT_SYS) : 0)

/*
 * 判断值（小于0）抛出系统调用异常
 */
#define RAISE_SYS_ERROR(x) \
	(unlikely((x) < 0) ? RAISE(EXCEPT_SYS) : 0)

/* -------------------                          */

/**
 * 异常结构
 */
typedef struct _Except
{
	const char      *exceptName;
	unsigned        exceptNo;
} ExceptT;

#define NULLOBJ_EXCEPT { NULL, 0 }

/**
 * 异常栈
 * 内部使用
 */
typedef struct _ExceptFrame
{
	struct _ExceptFrame     *prev;		/*上一个异常栈帧*/
	const ExceptT           *except;	/*当前异常信息*/
	const char              *file;		/*当前异常出现文件*/
	const char              *func;		/*当前异常出现函数*/
	int                     line;		/*当前异常出现文件行*/
	int                     layer;		/*当前栈层*/
	sigjmp_buf              env;		/*程序运行栈*/
} ExceptFrameT;

/**
 * 异常处理流程
 * 内部使用
 */
typedef enum _ExceptType
{
	/** 初始化状态，必须是0*/
	_ET_ENTERED = 0,
	/** 抛出异常状态*/
	_ET_RAISED = 1,
	/** 已捕捉异常状态*/
	_ET_HANDLED = 2,
	/** 进入清理状态*/
	_ET_FINAILIZED = 3
} ExceptTypeT;

/* ------------------------                   */

/**
 * 抛出异常
 * 内部使用
 */
void ExceptRaise(const char *function, const char *file, int line, const ExceptT *e);

/*异常栈保存的系统错误，用来在异常处理模块中获取正确的系统错误值，共有*/
extern __thread volatile int g_errno;
/* 异常栈，私有 */
extern __thread ExceptFrameT *_g_ef_;

/* ------------------------                   */

/* 主动抛出一个异常 */
#define RAISE(e) \
	ExceptRaise(__FUNCTION__, __FILE__, __LINE__, &(e))

/*
 * 再次抛出捕捉到的异常
 * 仅在捕获异常的情况下有效
 */
#define RERAISE				   \
	STMT_BEGIN			   \
	if (likely(_et_ == _ET_HANDLED)) { \
		_et_ = _ET_RAISED;	   \
	}				   \
	STMT_END

/* ------------------------                   */

/*
 * !!! 从异常处理中返回值 !!!
 * !!! 不能嵌套使用 !!!
 */
#define ReturnValue(x) \
	STMT_BEGIN EXCEPT_POP(); return (x); STMT_END

#define ReturnVoid() \
	STMT_BEGIN EXCEPT_POP(); return; STMT_END

/*
 * 获取捕捉到的异常（发生异常时可用）
 */
#define GetExcept()     (_ef_.except)

/*
 * 重新设置捕获到的异常（发生异常时可用）
 * 设置的异常必须为全局静态常量
 */
#define SetExcept(e)    (_ef_.except = &(e))

/*
 * 当发生系统调用错误时，用来在异常处理模块中获取正确的系统错误值
 */
#define GetErrno()      (_error_)

/* ------------------------                   */

/*
 * 流程如下:
 *      1.setjmp_and_compare_return_value <----------------\
 *      |  |                                               |
 *      |  \-> 1.1.deal_your_business ------------------\  |
 *      |  |                                            |  |
 *      |  |  /-- 1.2.raise_exception(if necessary) <---/  |
 *      |  |  |                                            |
 *      |  |  \-> 1.3.longjmp_and_return(only raise) ------/
 *      |  |
 *      |  \----------------------------------\
 *      |                                     |
 *      \-> 2.catch_and_deal_exception(list)  |
 *      |                                     |
 *      |<------------------------------------/
 *      |
 *      \-> 3.finally_and_clean
 *          |
 *          \-> 3.1.longjmp_previous_setjmp(if not catch or reraise) --->
 */
/* -------------------                          */

/* 压入一个异常 */
#define PushExceptFrame(ptr)				  \
	({						  \
		(ptr)->prev = _g_ef_;			  \
		if (likely(_g_ef_)) {			  \
			(ptr)->layer = _g_ef_->layer + 1; \
		}					  \
		_g_ef_ = (ptr);				  \
	})

/* 弹出一个异常 */
#define PopExceptFrame()		    \
	({				    \
		ExceptFrameT *ptr = NULL;   \
		ptr = _g_ef_;		    \
		if (likely(ptr)) {	    \
			_g_ef_ = ptr->prev; \
		}			    \
		ptr;			    \
	})

/*清理保存的错误值*/
#define CleanExceptFrame() \
	({ _error_ = g_errno; g_errno = 0; _error_; })

/*
 * 弹出异常栈
 * 内部使用
 */
#define EXCEPT_POP()			   \
	STMT_BEGIN			   \
	if (likely(_et_ == _ET_ENTERED)) { \
		PopExceptFrame();	   \
	}				   \
	STMT_END

/*
 * 安装
 * 在内联函数中不能使用TRY-CATCH-FINALLY模块
 */
#define TRY					    \
	STMT_BEGIN				    \
	ExceptFrameT _ef_ = {};			    \
	volatile int            _error_ = 0;	    \
	volatile ExceptTypeT    _et_ = _ET_ENTERED; \
	PushExceptFrame(&_ef_);			    \
	_et_ = sigsetjmp(_ef_.env, 1);		    \
	if (likely(_et_ == _ET_ENTERED)) {
/*
 * 捕捉指定异常
 * 在异常处理模块抛出异常，则不会执行finally模块
 * 可以通过再次抛出异常，继续finally模块的执行
 */
#define EXCEPT(e)				    \
	EXCEPT_POP();				    \
	} else if (unlikely(_ef_.except == &(e))) { \
		CleanExceptFrame();		    \
		_et_ = _ET_HANDLED;

/*
 * 捕捉所有异常
 * 在异常处理模块抛出异常，则不会执行finally模块
 */
#define CATCH			    \
	EXCEPT_POP();		    \
	} else {		    \
		CleanExceptFrame(); \
		_et_ = _ET_HANDLED;

/*
 * 清理
 */
#define FINALLY					   \
	EXCEPT_POP();				   \
	}					   \
	{					   \
		if (likely(_et_ == _ET_ENTERED)) { \
			_et_ = _ET_FINAILIZED;	   \
		}

/*
 * 结束
 */
#define END						  \
	EXCEPT_POP();					  \
	}						  \
	if (unlikely(_et_ == _ET_RAISED)) {		  \
		ExceptRaise(__FUNCTION__,		  \
			__FILE__, __LINE__, _ef_.except); \
	}						  \
	STMT_END

	__END_DECLS
#endif	/* defined(__minilib__except__) */

