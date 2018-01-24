//
//  main.c
//  libmini
//
//  Created by 周凯 on 15/12/16.
//  Copyright © 2015年 zk. All rights reserved.
//

#include <stdio.h>
#include "libmini.h"

#ifdef __APPLE__
  #define _XOPEN_SOURCE
#endif

#include <ucontext.h>

static ucontext_t       sctx[10] = {};
static volatile int     sctx_step = 0;
static volatile int     switch_step = 0;

static ucontext_t mctx = {};

void work(ucontext_t *cctx, ucontext_t *ctx)
{
	char hook1 = '\1';

	x_printf(I, "address hook1 `%p'", &hook1);

	swapcontext(ctx, cctx);

	char hook2 = '\1';
	x_printf(I, "address hook2 `%p'", &hook2);
}

void creat_ctx(ucontext_t *cctx, ucontext_t *ctx)
{
	int flag = 0;

	flag = getcontext(ctx);
	RAISE_SYS_ERROR(flag);

	ctx->uc_link = &mctx;
	ctx->uc_stack.ss_size = ADJUST_SIZE(SIGSTKSZ + 8, 8);
	ctx->uc_stack.ss_sp = malloc(ctx->uc_stack.ss_size);
	AssertError(ctx->uc_stack.ss_sp, ENOMEM);

	x_printf(W, "space of new stack : [%p - %p]", ctx->uc_stack.ss_sp, (char *)ctx->uc_stack.ss_sp + ctx->uc_stack.ss_size);

	makecontext(ctx, (void (*)(void))work, 2, cctx, ctx);

	swapcontext(cctx, ctx);
}

int main(int argc, char **argv)
{
	creat_ctx(&mctx, &sctx[sctx_step]);

	swapcontext(&mctx, &sctx[sctx_step++]);

	x_printf(I, "exit from main context ...");

	return EXIT_SUCCESS;
}

