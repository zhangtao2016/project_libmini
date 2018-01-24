//
//  except.c
//
//
//  Created by 周凯 on 15/8/28.
//
//
#include "except/except.h"
#include "except/exception.h"
#include "slog/slog.h"

__thread ExceptFrameT   *_g_ef_ = NULL;
__thread volatile int   g_errno = 0;

void ExceptRaise(const char *function, const char *file, int line, const ExceptT *e)
{
	ExceptFrameT *ptr = NULL;

	/*
	 * 从栈中弹出当前抛出的异常
	 */
	ptr = PopExceptFrame();

	if (unlikely((!ptr) || (!e))) {
		SLogWriteByInfo(F, function, file, line,
			"Uncaught exception [ 0x%08x : %s ].\n"
			"aborting ...",
			e->exceptNo,
			e->exceptNo == EXCEPT_SYS_CODE ? x_strerror(g_errno) :
			SWITCH_NULL_STR(e->exceptName));

		abort();
	}

	/*
	 * 打印异常跟踪
	 */
	if (likely(e == &EXCEPT_SYS)) {
		/*保存错误值*/
		if (g_errno == 0) {
			g_errno = errno;
		}

		SLogWriteByInfo(W, function, file, line,
			"%03d. %s : [%d : %s].",
			ptr->layer,
			SWITCH_NULL_STR(e->exceptName),
			g_errno,
			IsGaiError(g_errno) ? GaiStrerror(g_errno) : x_strerror(g_errno));
	} else {
		SLogWriteByInfo(W, function, file, line,
			"%03d. [ 0x%08x : %s].",
			ptr->layer,
			e->exceptNo,
			SWITCH_NULL_STR(e->exceptName));
	}

	/*为捕捉比较异常，再次抛出做准备*/
	ptr->except = e;

	/*
	 * 跳转到最近的TRY-CATCH-FINALLY模块的处理（或结束）处
	 */
	siglongjmp(ptr->env, _ET_RAISED);
}

