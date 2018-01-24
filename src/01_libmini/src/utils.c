//
//  utils.c
//  minilib
//
//  Created by 周凯 on 15/8/21.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#include <netdb.h>
#include <sched.h>
#include <sys/syscall.h>

#include <termios.h>

// #define _GNU_SOURCE
#ifdef _GNU_SOURCE
  #include <dlfcn.h>
#endif
#include <execinfo.h>
#include "utils.h"

#include "atomic/atomic.h"

/* -------------------------------------------                */

/*
 * 错误消息最大长度
 */
#ifndef ERROR_MESSAGE_MAX_SIZE
  #define       ERROR_MESSAGE_MAX_SIZE (512)
#endif

/* -------------------------------------------                */
/**字节序*/
const union _byteorder g_ByteOrder = { 0x01020304 };

/* -------------------------------------------                */

static __thread char _g_errMsg[ERROR_MESSAGE_MAX_SIZE] = { 0 };

/**程序名称*/
static struct
{
	int     magic;
	char    name[32];
} _g_ProgName = {
	0,
	{ 0 }
};

/* -------------------------------------------                */
/*一些对外的全局*/
long            g_PageSize = -1;
__thread long   g_ThreadID = -1;
long            g_ProcessID = -1;
long            g_CPUCores = -1;
const char      *g_ProgName = NULL;

/* -------------------------------------------                */

/* -----------------------                              */

char *x_strerror(int error)
{
	/*不能调用有日志相关函数*/
	char *ptr = &_g_errMsg[0];

	ptr[0] = '\0';

	if (unlikely(error < 0)) {
		snprintf(ptr, sizeof(_g_errMsg), "%s", GaiStrerror(error));
	} else {
#ifdef __LINUX__
  #if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE
		int rc = 0;
		rc = strerror_r(error, ptr, sizeof(_g_errMsg));

		if (unlikely(rc < 0)) {
			snprintf(ptr, sizeof(_g_errMsg),
				"Unkown error [%d], or call strerror_r() failed", error);
		}

  #else
		char *rc = NULL;
		rc = strerror_r(error, ptr, sizeof(_g_errMsg));

		if (unlikely(rc == NULL)) {
			snprintf(ptr, sizeof(_g_errMsg),
				"Unkown error [%d], or call strerror_r() failed", error);
		} else {
			snprintf(ptr, sizeof(_g_errMsg), "%s", rc);
		}
  #endif

#else
		int rc = 0;
		rc = strerror_r(error, ptr, sizeof(_g_errMsg));

		if (unlikely(rc < 0)) {
			snprintf(ptr, sizeof(_g_errMsg),
				"Unkown error [%d], or call strerror_r() failed", error);
		}
#endif			/* if defined(__LINUX__) */
	}

	return ptr;
}

/* ---------------                      */

/*
 * 获取套接字发生的错误
 */
bool IsGaiError(int error)
{
	switch (error)
	{
#ifdef EAI_ADDRFAMILY
		case NEG(EAI_ADDRFAMILY):
#endif
		case NEG(EAI_AGAIN):
		case NEG(EAI_BADFLAGS):
		case NEG(EAI_FAIL):
		case NEG(EAI_FAMILY):
		case NEG(EAI_MEMORY):
#ifdef EAI_NODATA
		case NEG(EAI_NODATA):
#endif
		case NEG(EAI_NONAME):
		case NEG(EAI_SERVICE):
		case NEG(EAI_SOCKTYPE):
		case NEG(EAI_SYSTEM):
			return true;

		default:
			return false;
	}
}

const char *GaiStrerror(int error)
{
	switch (error)
	{
#ifdef EAI_ADDRFAMILY
		case NEG(EAI_ADDRFAMILY):
			return "address family for host not supported";
#endif
		case NEG(EAI_AGAIN):
			return "temporary failure in name resolution";

		case NEG(EAI_BADFLAGS):
			return "invalid flags value";

		case NEG(EAI_FAIL):
			return "non-recoverable failure in name resolution";

		case NEG(EAI_FAMILY):
			return "address family not supported";

		case NEG(EAI_MEMORY):
			return "memory allocation failure";

#ifdef EAI_NODATA
		case NEG(EAI_NODATA):
			return "no address associated with host";
#endif
		case NEG(EAI_NONAME):
			return "host nor service provided, or not known";

		case NEG(EAI_SERVICE):
			return "service not supported for socket type";

		case NEG(EAI_SOCKTYPE):
			return "socket type not supported";

		case NEG(EAI_SYSTEM):
			return "system error";

		default:
			return "unknown getaddrinfo() error";
	}
}

/* ---------------                      */

void SetProgName(const char *name)
{
	return_if_fail(name && (name[0] != '\0'));

	if (likely(REFOBJ(&_g_ProgName))) {
		/*第一次调用,设置全局*/
		const char *base = NULL;
		base = strrchr(name, '/');
		base = base ? (base + 1) : name;
		snprintf(_g_ProgName.name, sizeof(_g_ProgName.name), "%s", base);
		ATOMIC_SET(&g_ProgName, &_g_ProgName.name[0]);
	}
}

char *GetProgName(char *buff, size_t size)
{
	/*不能调用有日志相关函数*/
	void *stackptr[1] = {};

	return_val_if_fail(buff && size > 0, NULL);

	/*
	 * 如果已经设置过,则返回,否则尝试使用系统函数去获取,
	 * 但这只能在静态库和二进制执行文件中调用有效,
	 * 在动态库中只能获取到动态库的名称
	 */
	if (likely(ISOBJ(&_g_ProgName))) {
		snprintf(buff, size, "%s", _g_ProgName.name);
		return buff;
	}

	backtrace(stackptr, DIM(stackptr));

#ifdef _GNU_SOURCE
	Dl_info info = {};
	int     flag = 0;

	flag = dladdr(stackptr[0], &info);

	if (likely(flag > 0)) {
		const char      *ptr1 = NULL;
		const char      *ptr2 = NULL;
		ptr1 = SWITCH_NULL_STR(info.dli_fname);
		ptr2 = strrchr(ptr1, '/');
		ptr2 = ptr2 ? (ptr2 + 1) : ptr1;
		snprintf(buff, size, "%s", ptr2);
	} else {
		snprintf(buff, size, "unkown");
	}

#else
	char **info = NULL;

	info = backtrace_symbols(stackptr, DIM(stackptr));

	if (likely(info)) {
		const char      *ptr = NULL;
		const char      *next = NULL;
  #ifdef __LINUX__
		bzero(buff, size);

		ptr = strrchr(info[0], '/');

		if (likely(ptr)) {
			next = strrchr(++ptr, '(');

			if (likely(next)) {
				snprintf(buff, size, "%.*s", (int)(next - ptr), ptr);
			}
		}

		if (unlikely(buff[0] == '\0')) {
			snprintf(buff, size, "unkown");
		}

  #else
		ptr = strchr(info[0], ' ');
		ptr = ptr ? (ptr + 1) : info[0];

		while (*ptr == ' ') {
			ptr++;
		}

		next = strchr(ptr, ' ');

		if (likely(next)) {
			snprintf(buff, size, "%.*s", (int)(next - ptr), ptr);
		} else {
			snprintf(buff, size, "%s", ptr);
		}
  #endif	/* ifdef __LINUX__ */
		Free(info);
	} else {
		snprintf(buff, size, "unkown");
	}
#endif	/* ifdef _GNU_SOURCE */

	SetProgName(buff);

	return buff;
}

char *GetCallerName(char *buff, size_t size, int layer)
{
	/*禁止添加日志相关文件*/
	int     expect = layer;
	void    *stackptr[8] = {};

	return_val_if_fail(buff && size > 0, NULL);

	expect = expect > 0 ? (expect + 1) : 1;
	/*至多返回8层*/
	layer = INRANGE(expect, 2, DIM(stackptr));

	layer = backtrace(stackptr, DIM(stackptr));

	expect = INRANGE(expect, 1, layer);

#ifdef _GNU_SOURCE
	Dl_info info = {};
	int     flag = 0;
again:
	flag = dladdr(stackptr[expect], &info);

	if (likely(flag > 0)) {
		const char *ptr = info.dli_sname;

		if (unlikely((ptr == NULL) && ((++expect) < layer))) {
			goto again;
		}

		snprintf(buff, size, "%s", SWITCH_NULL_STR(ptr));
	} else {
		snprintf(buff, size, "unkown");
	}

#else
	char **info = NULL;

	info = backtrace_symbols(stackptr, layer);

	if (likely(info)) {
		char *ptr = NULL;
		ptr = strrchr(info[expect], '+');

		if (likely(ptr && (*(--ptr) == ' ') && (ptr > &info[expect][1]))) {
			*ptr = '\0';
			ptr = strrchr(info[expect], ' ');

			if (likely(ptr)) {
				snprintf(buff, size, "%s", ptr + 1);
			}
		}

		Free(info);
	}

	if (unlikely(buff[0] == '\0')) {
		snprintf(buff, size, "unkown");
	}
#endif	/* ifdef _GNU_SOURCE */

	return buff;
}

#ifndef __LINUX__

typedef int cpu_set_t;

  #define CPU_ZERO(X)           ((void)0)
  #define CPU_SET(X, Y)         ((void)0)
  #define CPU_ISSET(X, Y)       (0)

static inline int sched_setaffinity(int pid, size_t size, cpu_set_t *mask)
{
	return 0;
}

static inline int sched_getaffinity(int pid, size_t size, cpu_set_t *mask)
{
	return 0;
}
#endif

/*
 * @param which CPU_ID (0~ Max - 1)
 */
bool BindCPUCore(long which)
{
	long            ncpus = 0;
	static AO_T     curcore = 0;
	cpu_set_t       mask;
	int             i = 0;
	int             rc = 0;

	ncpus = GetCPUCores();

	return_val_if_fail(ncpus > 1, false);

	if (likely(which < 0)) {
		which = ATOMIC_F_ADD(&curcore, 1) % (ncpus - 1) + 1;
	} else {
		which = INRANGE(which, 1, ncpus - 1);
	}

	CPU_ZERO(&mask);
	CPU_SET(which, &mask);
	rc = sched_setaffinity(0, sizeof(mask), &mask);

	if (unlikely(rc < 0)) {
		x_printf(S, "set affinity error : %s.\n", x_strerror(errno));
		return false;
	}

	CPU_ZERO(&mask);
	rc = sched_getaffinity(0, sizeof(mask), &mask);

	if (unlikely(rc < 0)) {
		x_printf(S, "get affinity error : %s.\n", x_strerror(errno));
		return false;
	}

	for (i = 0; i < ncpus; i++) {
		// 判断线程与哪个CPU有亲和力
		if (CPU_ISSET(i, &mask)) {
			if (isatty(fileno(stdout)) > 0) {
				fprintf(stdout,
					LOG_I_COLOR
					"This thread ["
					LOG_S_COLOR
					"%06ld"
					LOG_I_COLOR
					"] is running on processor"
					" ["
					LOG_S_COLOR
					"%02d"
					LOG_I_COLOR
					"]."
					PALETTE_NULL
					"\n",
					GetThreadID(), i);
			} else {
				fprintf(stdout,
					"This thread [%ld] "
					"is running on processor"
					" [%02d].\n",
					GetThreadID(), i);
			}
		}
	}

	return true;
}

long GetProcessID()
{
	long gpid = -1;

	/*不能调用有日志相关函数*/
	if (likely((gpid = ATOMIC_GET(&g_ProcessID)) != -1)) {
		return gpid;
	}

	pid_t pid = -1;
	pid = getpid();

	if (unlikely(pid < 0)) {
		fprintf(stderr,
			"Get process's id failure : %s",
			x_strerror(errno));
		abort();
	}

	ATOMIC_SET(&g_ProcessID, (long)pid);

	return (long)pid;
}

long GetThreadID()
{
	long gtid = -1;

	/*不能调用有日志相关函数*/
	if (likely((gtid = ATOMIC_GET(&g_ThreadID)) != -1)) {
		return gtid;
	}

#ifdef __APPLE__
	gtid = syscall(SYS_thread_selfid);
#else
	gtid = syscall(SYS_gettid);
#endif

	if (unlikely(gtid < 0)) {
		x_printf(F, "Get thread's id failure : %s", x_strerror(errno));
		abort();
	}

	ATOMIC_SET(&g_ThreadID, gtid);

	return gtid;
}

long GetCPUCores()
{
	long gcpus = -1;

	/*不能调用有日志相关函数*/
	if (likely((gcpus = ATOMIC_GET(&g_CPUCores)) != -1)) {
		return gcpus;
	}

	gcpus = sysconf(_SC_NPROCESSORS_CONF);

	if (unlikely(gcpus < 0)) {
		x_printf(F, "Get number of CPU failure : %s", x_strerror(errno));
		abort();
	}

	ATOMIC_SET(&g_CPUCores, gcpus);

	return gcpus;
}

/* ---------------                      */

int Getch(int *r)
{
	int             c = 0;
	struct termios  newv = {};
	struct termios  oldv = {};
	int             rc = -1;

	rc = isatty(fileno(stdout));

	if (unlikely(rc < 1)) {
		x_printf(E, "isatty() : %s", x_strerror(errno));
		return -1;
	}

	rc = tcgetattr(fileno(stdin), &newv);

	if (unlikely(rc < 0)) {
		x_printf(E, "tcgetattr() : %s", x_strerror(errno));
		return -1;
	}

	bcopy(&newv, &oldv, sizeof(oldv));

	newv.c_lflag &= ~ICANON;			/*非标准模式处理终端*/
	newv.c_lflag &= ~ECHO;				/*关闭回显*/
	newv.c_cc[VMIN] = 1;				/*只缓冲一个字符*/
	newv.c_cc[VTIME] = 0;				/*不超时*/
	newv.c_lflag &= ~ISIG;				/*忽略所以信号控制字符*/

	rc = tcsetattr(fileno(stdin), TCSANOW, &newv);

	if (unlikely(rc < 0)) {
		x_printf(E, "%s", x_strerror(errno));
		return -1;
	}

	c = fgetc(stdin);

	if (unlikely(c == EOF)) {
		if (unlikely(ferror(stdin) != 0)) {
			x_printf(W, "getchar() : %s.", x_strerror(errno));
		} else {
			x_printf(W, "the stream is at end-of-file.");
		}
	}

	rc = tcsetattr(fileno(stdin), TCSANOW, &oldv);

	if (unlikely(rc < 0)) {
		x_printf(E, "tcsetattr() : %s", x_strerror(errno));
		return -1;
	}

	SET_POINTER(r, c);

	return c;
}

long GetPageSize()
{
	long ps = -1;

	/*不能调用有日志相关函数*/
	if (likely((ps = ATOMIC_GET(&g_PageSize)) != -1)) {
		return ps;
	}

	ps = sysconf(_SC_PAGE_SIZE);

	if (unlikely(ps < 0)) {
		x_printf(F, "Get size of page failure : %s", x_strerror(errno));
		abort();
	}

	ATOMIC_SET(&g_PageSize, ps);

	return ps;
}

