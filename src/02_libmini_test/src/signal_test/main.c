//
//  main.c
//  libmini
//
//  Created by 周凯 on 15/12/14.
//  Copyright © 2015年 zk. All rights reserved.
//

#include <stdio.h>
#include "libmini.h"

static jmp_buf          sig_jmpbuf[10] = {};
static volatile int     sig_jmpbuf_step = 0;
static volatile int     sig_jmp_step = 0;

static jmp_buf          main_jmpbuf = {};
static volatile int     sig_deal = 0;

static void creat_sigaltstack(SignalCB callback)
{
	assert(callback);

	SignalMaskBlockOne(SIGUSR1, NULL);

	//	SignalIntr(SIGUSR1, signal_on_stack);

	/* ------ */
	struct sigaction        siganew = {};
	struct sigaction        sigaold = {};
	int                     rc = 0;

	siganew.sa_flags = SA_ONSTACK | SA_RESTART;
	siganew.sa_handler = callback;
	sigemptyset(&siganew.sa_mask);

	rc = sigaction(SIGUSR1, &siganew, &sigaold);
	RAISE_SYS_ERROR(rc);

	/* ------ */
	stack_t snew = {};
	stack_t sold = {};

	snew.ss_flags = 0;
	snew.ss_size = ADJUST_SIZE(SIGSTKSZ + 8, 8);
	snew.ss_sp = malloc(snew.ss_size);
	AssertError(snew.ss_sp, ENOMEM);

	x_printf(W, "space of new stack : [%p - %p]", snew.ss_sp, snew.ss_sp + snew.ss_size - 1);

	rc = sigaltstack(&snew, &sold);
	RAISE_SYS_ERROR(rc);

	/* ------ */
	sigset_t sigset;
	sigfillset(&sigset);
	sigdelset(&sigset, SIGUSR1);

	sig_deal = 0;
	kill(getpid(), SIGUSR1);

	while (sig_deal == 0) {
		sigsuspend(&sigset);
	}

	SignalMaskUnblockOne(SIGUSR1);
}

static void (check_sigaltstack)(const char *func, const char *file, int line)
{
	stack_t stack = {};
	int     rc = 0;

	rc = sigaltstack(NULL, &stack);
	RAISE_SYS_ERROR(rc);

	if (stack.ss_flags & SS_ONSTACK) {
		SLogWriteByInfo(W, func, file, line, "[SS_ONSTACK] turn on, [SS_DISABLE] turn off");
	} else if (stack.ss_flags & SS_DISABLE) {
		SLogWriteByInfo(W, func, file, line, "[SS_ONSTACK] turn off, [SS_DISABLE] turn on");
	} else {
		SLogWriteByInfo(W, func, file, line, "[SS_ONSTACK] turn off, [SS_DISABLE] turn off");
	}

	if (stack.ss_size && stack.ss_sp) {
		SLogWriteByInfo(W, func, file, line, "space of stack : [%p - %p]",
			stack.ss_sp, stack.ss_sp + stack.ss_size - 1);
	}
}

#define check_sigaltstack() ((check_sigaltstack)(__FUNCTION__, __FILE__, __LINE__))

static void sigaltstack_turnoff(const char *func, const char *file, int line)
{
	(check_sigaltstack)(func, file, line);

	TRY
	{
		stack_t stack = {};
		int     rc = 0;

		rc = sigaltstack(NULL, &stack);
		RAISE_SYS_ERROR(rc);

		stack.ss_flags = SS_DISABLE;

		rc = sigaltstack(&stack, NULL);
		RAISE_SYS_ERROR(rc);
	}
	CATCH
	{}
	END;

	(check_sigaltstack)(func, file, line);
}

#define sigaltstack_turnoff() ((sigaltstack_turnoff)(__FUNCTION__, __FILE__, __LINE__))

static void signal_on_stack(int signo)
{
	char hook1 = '\1';

	x_printf(I, "address hook1 `%p'", &hook1);

	if (sigsetjmp(sig_jmpbuf[sig_jmpbuf_step++], 1)) {
		/*second,return back*/
		char hook2 = '\2';
		x_printf(I, "address hook2 `%p'", &hook2);

		//		creat_sigaltstack(signal_on_stack);
		//
		//		sigaltstack_turnoff();

		x_printf(W, "sub corotine `%d`", sig_jmp_step);

		if (sig_jmp_step >= sig_jmpbuf_step) {
			siglongjmp(main_jmpbuf, 1);
		} else {
			siglongjmp(sig_jmpbuf[sig_jmp_step++], 1);
		}
	} else {
		/*first,run those follow code*/
		sig_deal = 1;
		check_sigaltstack();
	}
}

static void signal_normal(int signo)
{
	char hook = '\1';

	x_printf(I, "address hook `%p'", &hook);

	sig_deal = 1;
}

int main(int args, char **argv)
{
	char hook1 = '\1';

	x_printf(I, "address hook1 `%p'", &hook1);

	int exit_here = 0;

	exit_here = sigsetjmp(main_jmpbuf, 1);

	if (exit_here) {
		check_sigaltstack();
		x_printf(D, "processe return form signal stack, and exit ...");
		return EXIT_SUCCESS;
	}

	creat_sigaltstack(signal_on_stack);

	//	sigaltstack_turnoff();

	sigset_t old;
	SignalMaskBlockOne(SIGUSR2, &old);

	//	SignalIntr(SIGUSR2, signal_normal);

	struct sigaction        siganew = {};
	struct sigaction        sigaold = {};
	int                     rc = 0;

	siganew.sa_flags = SA_ONSTACK | SA_RESTART;
	siganew.sa_handler = signal_normal;
	sigemptyset(&siganew.sa_mask);

	rc = sigaction(SIGUSR2, &siganew, &sigaold);
	RAISE_SYS_ERROR(rc);

	sigset_t sigset;
	sigfillset(&sigset);
	sigdelset(&sigset, SIGUSR2);
	//	sigwait(&sigset, NULL);
	sig_deal = 0;
	kill(getpid(), SIGUSR2);

	while (!sig_deal) {
		sigsuspend(&sigset);
	}

	rc = sigaction(SIGUSR2, &sigaold, NULL);
	RAISE_SYS_ERROR(rc);

	SignalMaskBlockAll(&old);

	//	creat_sigaltstack(signal_on_stack);
	//
	//	siglongjmp(sig_jmpbuf[sig_jmp_step++], 1);

	return EXIT_SUCCESS;
}

