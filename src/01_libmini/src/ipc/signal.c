//
//  signal.c
//
//
//  Created by 周凯 on 15/8/28.
//
//
#include <pthread.h>
#include <stdarg.h>
#include "ipc/signal.h"
#include "slog/slog.h"
#include "except/exception.h"

/**
 * 安装信号，尽可能重启慢速调用
 */
SignalCB Signal(int signo, SignalCB cb)
{
	struct sigaction        oact = {};
	struct sigaction        nact = {};
	int                     rc = 0;

	assert(signo != SIGKILL && signo != SIGSTOP && signo != SIGCONT);

	nact.sa_handler = cb;

	sigemptyset(&nact.sa_mask);

	if (signo == SIGALRM) {
#ifdef SA_INTERRUPT
		nact.sa_flags |= SA_INTERRUPT;
#endif
	} else {
#ifdef SA_RESTART
		nact.sa_flags |= SA_RESTART;
#endif
	}

	rc = sigaction(signo, &nact, &oact);
	RAISE_SYS_ERROR(rc);

	return (SignalCB)oact.sa_handler;
}

/**
 * 安装信号，不重启慢速调用
 */
SignalCB SignalIntr(int signo, SignalCB cb)
{
	struct sigaction        oact = {};
	struct sigaction        nact = {};
	int                     rc = 0;

	assert(signo != SIGKILL && signo != SIGSTOP && signo != SIGCONT);

	nact.sa_handler = cb;

	sigemptyset(&nact.sa_mask);

#ifdef SA_INTERRUPT
	nact.sa_flags |= SA_INTERRUPT;
#endif

	rc = sigaction(signo, &nact, &oact);
	RAISE_SYS_ERROR(rc);

	return (SignalCB)oact.sa_handler;
}

void SignalNormalCB(int signo)
{
	x_printf(D, "Interrupt by [%d : %s] signal.", signo, sys_siglist[signo]);
}

/**
 * 锁定线程的所有信号
 */
void SignalMaskBlockAll(sigset_t *sigset)
{
	int             rc = 0;
	sigset_t        block;

	sigfillset(&block);

	rc = pthread_sigmask(SIG_BLOCK, &block, sigset);
	RAISE_SYS_ERROR_ERRNO(rc);
}

/**
 * 解锁线程的所有信号
 */
void SignalMaskUnblockAll(sigset_t *sigset)
{
	int rc = 0;

	if (sigset) {
		rc = pthread_sigmask(SIG_SETMASK, sigset, NULL);
	} else {
		sigset_t block;
		sigfillset(&block);
		rc = pthread_sigmask(SIG_UNBLOCK, &block, NULL);
	}

	RAISE_SYS_ERROR_ERRNO(rc);
}

/**
 * 锁定线程的指定信号，如果sigset不为null则将旧信号掩码存到该值
 */
void SignalMaskBlockOne(int signo, sigset_t *sigset)
{
	int             rc = 0;
	sigset_t        block;

	sigemptyset(&block);
	sigaddset(&block, signo);

	rc = pthread_sigmask(SIG_BLOCK, &block, sigset);
	RAISE_SYS_ERROR_ERRNO(rc);
}

/**
 * 解锁线程的指定信号
 */
void SignalMaskUnblockOne(int signo)
{
	int             rc = 0;
	sigset_t        block;

	sigemptyset(&block);
	sigaddset(&block, signo);

	rc = pthread_sigmask(SIG_UNBLOCK, &block, NULL);
	RAISE_SYS_ERROR_ERRNO(rc);
}

