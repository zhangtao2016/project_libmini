/*
 * Copyright (c) 2001-2011 Marc Alexander Lehmann <schmorp@schmorp.de>
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 *
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License ("GPL") version 2 or any later version,
 * in which case the provisions of the GPL are applicable instead of
 * the above. If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the BSD license, indicate your decision
 * by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL. If you do not delete the
 * provisions above, a recipient may use your version of this file under
 * either the BSD or the GPL.
 *
 * This library is modelled strictly after Ralf S. Engelschalls article at
 * http://www.gnu.org/software/pth/rse-pmt.ps. So most of the credit must
 * go to Ralf S. Engelschall <rse@engelschall.com>.
 */

#include "coro.h"

#include <stddef.h>
#include <string.h>

#if defined __GNUC__
  #define likely(x)     __builtin_expect(!!(x), 1)
  #define unlikely(x)   __builtin_expect(!!(x), 0)
  #define THREAD_VAR __thread
#else
  #define likely(x)     (!!(x))
  #define unlikely(x)   (!!(x))
  #define THREAD_VAR
#endif

/*****************************************************************************/
/* ucontext/setjmp/asm backends                                              */
/*****************************************************************************/
#if CORO_UCONTEXT || CORO_SJLJ || CORO_LOSER || CORO_LINUX || CORO_IRIX || CORO_ASM

  #if CORO_UCONTEXT
    #include <stddef.h>
  #endif

  #if !defined(STACK_ADJUST_PTR)
    #if __sgi
/* IRIX is decidedly NON-unix */
      #define STACK_ADJUST_PTR(sp, ss)  ((char *)(sp) + (ss) - 8)
      #define STACK_ADJUST_SIZE(sp, ss) ((ss) - 8)
    #elif (__i386__ && CORO_LINUX) || (_M_IX86 && CORO_LOSER)
      #define STACK_ADJUST_PTR(sp, ss)  ((char *)(sp) + (ss))
      #define STACK_ADJUST_SIZE(sp, ss) (ss)
    #elif (__amd64__ && CORO_LINUX) || ((_M_AMD64 || _M_IA64) && CORO_LOSER)
      #define STACK_ADJUST_PTR(sp, ss)  ((char *)(sp) + (ss) - 8)
      #define STACK_ADJUST_SIZE(sp, ss) (ss)
    #else
      #define STACK_ADJUST_PTR(sp, ss)  (sp)
      #define STACK_ADJUST_SIZE(sp, ss) (ss)
    #endif
  #endif

  #include <stdlib.h>

  #if CORO_SJLJ
    #include <stdio.h>
    #include <signal.h>
    #include <unistd.h>
  #endif

static THREAD_VAR coro_func     coro_init_func;
static THREAD_VAR void          *coro_init_arg;
static THREAD_VAR coro_context  *new_coro, *create_coro;

  #if CORO_UCONTEXT
    #include <unistd.h>
    #include <stdint.h>

struct coro_init_args
{
        coro_func       call;
        void            *usr;
        coro_context    *newctx;
        coro_context    *oldctx;
};

static void
coro_init(uint32_t hight, uint32_t low)
{
        struct coro_init_args   *args = NULL;
        uint64_t                bits = hight;

        bits <<= 32;
        bits |= low;
        args = (void *)(uintptr_t)bits;

        if (unlikely(!args)) {
                abort();
        }

        coro_func       func = args->call;
        void            *arg = args->usr;

        coro_transfer(args->newctx, args->oldctx);

        func((void *)arg);

        /* 必须手动切回*/
        /* the new coro returned. bad. just abort() for now */
        abort();
}

  #else

static void
coro_init(void)
{
        volatile coro_func      func = coro_init_func;
        volatile void           *arg = coro_init_arg;

        coro_transfer(new_coro, create_coro);

    #if __GCC_HAVE_DWARF2_CFI_ASM && __amd64
        asm (".cfi_undefined rip");
    #endif

        func((void *)arg);

        /* the new coro returned. bad. just abort() for now */
        abort();
}
  #endif /* if CORO_UCONTEXT */

  #if CORO_SJLJ

    #include <pthread.h>

static THREAD_VAR volatile int trampoline_done;

/* trampoline signal handler */
static void
trampoline(int sig)
{
        if (coro_setjmp(new_coro->env)) {
                coro_init();         /* start it */
        } else {
                trampoline_done = 1;
        }
}
  #endif

  #if CORO_ASM

    #if _WIN32 || __CYGWIN__
      #define CORO_WIN_TIB 1
    #endif

asm (
        "\t.text\n"
    #if _WIN32 || __CYGWIN__
        "\t.globl _coro_transfer\n"
        "_coro_transfer:\n"
    #else
        "\t.globl coro_transfer\n"
        "coro_transfer:\n"
    #endif
        /* windows, of course, gives a shit on the amd64 ABI and uses different registers */
        /* http://blogs.msdn.com/freik/archive/2005/03/17/398200.aspx */
    #if __amd64
      #if _WIN32 || __CYGWIN__
        #define NUM_SAVED 29
        "\tsubq $168, %rsp\t"         /* one dummy qword to improve alignment */
        "\tmovaps %xmm6, (%rsp)\n"
        "\tmovaps %xmm7, 16(%rsp)\n"
        "\tmovaps %xmm8, 32(%rsp)\n"
        "\tmovaps %xmm9, 48(%rsp)\n"
        "\tmovaps %xmm10, 64(%rsp)\n"
        "\tmovaps %xmm11, 80(%rsp)\n"
        "\tmovaps %xmm12, 96(%rsp)\n"
        "\tmovaps %xmm13, 112(%rsp)\n"
        "\tmovaps %xmm14, 128(%rsp)\n"
        "\tmovaps %xmm15, 144(%rsp)\n"
        "\tpushq %rsi\n"
        "\tpushq %rdi\n"
        "\tpushq %rbp\n"
        "\tpushq %rbx\n"
        "\tpushq %r12\n"
        "\tpushq %r13\n"
        "\tpushq %r14\n"
        "\tpushq %r15\n"
        #if CORO_WIN_TIB
        "\tpushq %fs:0x0\n"
        "\tpushq %fs:0x8\n"
        "\tpushq %fs:0xc\n"
        #endif
        "\tmovq %rsp, (%rcx)\n"
        "\tmovq (%rdx), %rsp\n"
        #if CORO_WIN_TIB
        "\tpopq %fs:0xc\n"
        "\tpopq %fs:0x8\n"
        "\tpopq %fs:0x0\n"
        #endif
        "\tpopq %r15\n"
        "\tpopq %r14\n"
        "\tpopq %r13\n"
        "\tpopq %r12\n"
        "\tpopq %rbx\n"
        "\tpopq %rbp\n"
        "\tpopq %rdi\n"
        "\tpopq %rsi\n"
        "\tmovaps (%rsp), %xmm6\n"
        "\tmovaps 16(%rsp), %xmm7\n"
        "\tmovaps 32(%rsp), %xmm8\n"
        "\tmovaps 48(%rsp), %xmm9\n"
        "\tmovaps 64(%rsp), %xmm10\n"
        "\tmovaps 80(%rsp), %xmm11\n"
        "\tmovaps 96(%rsp), %xmm12\n"
        "\tmovaps 112(%rsp), %xmm13\n"
        "\tmovaps 128(%rsp), %xmm14\n"
        "\tmovaps 144(%rsp), %xmm15\n"
        "\taddq $168, %rsp\n"
      #else
        #define NUM_SAVED 6
        "\tpushq %rbp\n"
        "\tpushq %rbx\n"
        "\tpushq %r12\n"
        "\tpushq %r13\n"
        "\tpushq %r14\n"
        "\tpushq %r15\n"
        "\tmovq %rsp, (%rdi)\n"
        "\tmovq (%rsi), %rsp\n"
        "\tpopq %r15\n"
        "\tpopq %r14\n"
        "\tpopq %r13\n"
        "\tpopq %r12\n"
        "\tpopq %rbx\n"
        "\tpopq %rbp\n"
      #endif /* if _WIN32 || __CYGWIN__ */
        "\tpopq %rcx\n"
        "\tjmpq *%rcx\n"

    #elif __i386
      #define NUM_SAVED 4
        "\tpushl %ebp\n"
        "\tpushl %ebx\n"
        "\tpushl %esi\n"
        "\tpushl %edi\n"
      #if CORO_WIN_TIB
        #undef NUM_SAVED
        #define NUM_SAVED 7
        "\tpushl %fs:0\n"
        "\tpushl %fs:4\n"
        "\tpushl %fs:8\n"
      #endif
        "\tmovl %esp, (%eax)\n"
        "\tmovl (%edx), %esp\n"
      #if CORO_WIN_TIB
        "\tpopl %fs:8\n"
        "\tpopl %fs:4\n"
        "\tpopl %fs:0\n"
      #endif
        "\tpopl %edi\n"
        "\tpopl %esi\n"
        "\tpopl %ebx\n"
        "\tpopl %ebp\n"
        "\tpopl %ecx\n"
        "\tjmpl *%ecx\n"

    #else
      #error unsupported architecture
    #endif      /* if __amd64 */
        );
  #endif        /* if CORO_ASM */

void
coro_create(coro_context *ctx, coro_func coro, void *arg, void *sptr, size_t ssize)
{
        coro_context nctx;

  #if CORO_SJLJ
        int                     flag;
        stack_t                 ostk, nstk;
        struct sigaction        osa, nsa;
        sigset_t                nsig, osig;
  #endif

        if (!coro) {
                return;
        }

        coro_init_func = coro;
        coro_init_arg = arg;

        new_coro = ctx;
        create_coro = &nctx;

  #if CORO_SJLJ
        /* we use SIGUSR2. first block it, then fiddle with it. */

        sigemptyset(&nsig);
        sigaddset(&nsig, SIGUSR2);
    #if 0
        sigprocmask(SIG_BLOCK, &nsig, &osig);
    #else
        pthread_sigmask(SIG_BLOCK, &nsig, &osig);
    #endif

        nsa.sa_handler = trampoline;
        sigemptyset(&nsa.sa_mask);
        nsa.sa_flags = SA_ONSTACK | SA_RESTART;

        flag = sigaction(SIGUSR2, &nsa, &osa);

        if (unlikely(flag != 0)) {
                perror("sigaction");
                abort();
        }

        /* set the new stack */
        /* yes, some platforms (IRIX) get this wrong. */
        nstk.ss_sp = STACK_ADJUST_PTR(sptr, ssize);
        nstk.ss_size = STACK_ADJUST_SIZE(sptr, ssize);
        nstk.ss_flags = 0;
        /* 从现在设置开始触发的信号将使用设置的空间作为其栈空间，并运行中断函数*/
        flag = sigaltstack(&nstk, &ostk);

        if (unlikely(flag != 0)) {
                perror("sigaltstack");
                abort();
        }

        trampoline_done = 0;
    #if 0
        kill(getpid(), SIGUSR2);
    #else
        pthread_kill(pthread_self(), SIGUSR2);
    #endif
        sigfillset(&nsig); sigdelset(&nsig, SIGUSR2);

        while (!trampoline_done) {
                sigsuspend(&nsig);
        }

        /*
         * 运行下面一些列函数主要是将一次信号触发使用的信号栈服务唯一的协同
         */
        /*关闭信号在设置栈上运行的能力*/
        sigaltstack(0, &nstk);
        nstk.ss_flags = SS_DISABLE;

        flag = sigaltstack(&nstk, 0);

        if (unlikely(flag != 0)) {
                perror("sigaltstack");
        }

        sigaltstack(0, &nstk);

        /*查看没有成功设置SS_DISABLE*/
        if (unlikely(~nstk.ss_flags & SS_DISABLE)) {
                abort();
        }

        /*如果以前信号栈功能不是关闭的则还原*/
        if (~ostk.ss_flags & SS_DISABLE) {
                sigaltstack(&ostk, 0);
        }

        sigaction(SIGUSR2, &osa, 0);
    #if 0
        sigprocmask(SIG_SETMASK, &osig, 0);
    #else
        pthread_sigmask(SIG_SETMASK, &osig, 0);
    #endif

  #elif CORO_LOSER
        coro_setjmp(ctx->env);
    #if __CYGWIN__ && __i386
        ctx->env[8] = (long)coro_init;
        ctx->env[7] = (long)((char *)sptr + ssize) - sizeof(long);
    #elif __CYGWIN__ && __x86_64
        ctx->env[7] = (long)coro_init;
        ctx->env[6] = (long)((char *)sptr + ssize) - sizeof(long);
    #elif defined __MINGW32__
        ctx->env[5] = (long)coro_init;
        ctx->env[4] = (long)((char *)sptr + ssize) - sizeof(long);
    #elif defined _M_IX86
        ((_JUMP_BUFFER *)&ctx->env)->Eip = (long)coro_init;
        ((_JUMP_BUFFER *)&ctx->env)->Esp = (long)STACK_ADJUST_PTR(sptr, ssize) - sizeof(long);
    #elif defined _M_AMD64
        ((_JUMP_BUFFER *)&ctx->env)->Rip = (__int64)coro_init;
        ((_JUMP_BUFFER *)&ctx->env)->Rsp = (__int64)STACK_ADJUST_PTR(sptr, ssize) - sizeof(__int64);
    #elif defined _M_IA64
        ((_JUMP_BUFFER *)&ctx->env)->StIIP = (__int64)coro_init;
        ((_JUMP_BUFFER *)&ctx->env)->IntSp = (__int64)STACK_ADJUST_PTR(sptr, ssize) - sizeof(__int64);
    #else
      #error "microsoft libc or architecture not supported"
    #endif

  #elif CORO_LINUX
        coro_setjmp(ctx->env);
    #if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && defined(JB_PC) && defined(JB_SP)
        ctx->env[0].__jmpbuf[JB_PC] = (long)coro_init;
        ctx->env[0].__jmpbuf[JB_SP] = (long)STACK_ADJUST_PTR(sptr, ssize) - sizeof(long);
    #elif __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && defined(__mc68000__)
        ctx->env[0].__jmpbuf[0].__aregs[0] = (long int)coro_init;
        ctx->env[0].__jmpbuf[0].__sp = (int *)((char *)sptr + ssize) - sizeof(long);
    #elif defined(__GNU_LIBRARY__) && defined(__i386__)
        ctx->env[0].__jmpbuf[0].__pc = (char *)coro_init;
        ctx->env[0].__jmpbuf[0].__sp = (void *)((char *)sptr + ssize) - sizeof(long);
    #elif defined(__GNU_LIBRARY__) && defined(__amd64__)
        ctx->env[0].__jmpbuf[JB_PC] = (long)coro_init;
        ctx->env[0].__jmpbuf[0].__sp = (void *)((char *)sptr + ssize) - sizeof(long);
    #else
      #error "linux libc or architecture not supported"
    #endif

  #elif CORO_IRIX
        coro_setjmp(ctx->env, 0);
        ctx->env[JB_PC] = (__uint64_t)coro_init;
        ctx->env[JB_SP] = (__uint64_t)STACK_ADJUST_PTR(sptr, ssize) - sizeof(long);

  #elif CORO_ASM
        ctx->sp = (void **)(ssize + (char *)sptr);
        *--ctx->sp = (void *)abort;         /* needed for alignment only */
        *--ctx->sp = (void *)coro_init;

    #if CORO_WIN_TIB
        *--ctx->sp = 0;                                 /* ExceptionList */
        *--ctx->sp = (char *)sptr + ssize;              /* StackBase */
        *--ctx->sp = sptr;                              /* StackLimit */
    #endif
        ctx->sp -= NUM_SAVED;
        memset(ctx->sp, 0, sizeof(*ctx->sp) * NUM_SAVED);

  #elif CORO_UCONTEXT
        getcontext(&(ctx->uc));
    #if 0
        ctx->uc.uc_link = 0;
        ctx->uc.uc_stack.ss_sp = sptr;
        ctx->uc.uc_stack.ss_size = (size_t)ssize;
        ctx->uc.uc_stack.ss_flags = 0;

        makecontext(&(ctx->uc), (void (*)())coro_init, 0);
    #else
        struct coro_init_args args = {
                coro, arg, ctx, &nctx,
        };

        ctx->uc.uc_link = 0;
        ctx->uc.uc_stack.ss_sp = sptr;
        ctx->uc.uc_stack.ss_size = (size_t)ssize;
        ctx->uc.uc_stack.ss_flags = 0;

        makecontext(&(ctx->uc),
                (void (*)())coro_init,
                2,
                (uint32_t)(((uintptr_t)&args) >> 32),
                (uint32_t)(uintptr_t)&args);
        coro_transfer(&nctx, ctx);
        return;
    #endif      /* if 0 */
  #endif        /* if CORO_SJLJ */
        /*再次切换，将参数传递到新创建的协同，并再次切换回来*/
        coro_transfer(create_coro, new_coro);
}

/*****************************************************************************/
/* pthread backend                                                           */
/*****************************************************************************/
#elif CORO_PTHREAD

/* this mutex will be locked by the running coroutine */
pthread_mutex_t coro_mutex = PTHREAD_MUTEX_INITIALIZER;

struct coro_init_args
{
        coro_func       func;
        void            *arg;
        coro_context    *self, *main;
};

static pthread_t null_tid;

/* I'd so love to cast pthread_mutex_unlock to void (*)(void *)... */
static void
mutex_unlock_wrapper(void *arg)
{
        pthread_mutex_unlock((pthread_mutex_t *)arg);
}

static void *
coro_init(void *args_)
{
        struct coro_init_args   *args = (struct coro_init_args *)args_;
        coro_func               func = args->func;
        void                    *arg = args->arg;

        pthread_mutex_lock(&coro_mutex);

        /* we try to be good citizens and use deferred cancellation and cleanup handlers */
        pthread_cleanup_push(mutex_unlock_wrapper, &coro_mutex);
        coro_transfer(args->self, args->main);
        func(arg);
        pthread_cleanup_pop(1);

        return 0;
}

void
coro_transfer(coro_context *prev, coro_context *next)
{
        pthread_cond_signal(&next->cv);
        pthread_cond_wait(&prev->cv, &coro_mutex);
  #if __FreeBSD__
        /* freebsd is of course broken and needs manual testcancel calls... yay... */
        pthread_testcancel();
  #endif
}

void
coro_create(coro_context *ctx, coro_func coro, void *arg, void *sptr, size_t ssize)
{
        static coro_context     nctx;
        static int              once;

        if (!once) {
                once = 1;

                pthread_mutex_lock(&coro_mutex);
                pthread_cond_init(&nctx.cv, 0);
                null_tid = pthread_self();
        }

        pthread_cond_init(&ctx->cv, 0);

        if (coro) {
                pthread_attr_t          attr;
                struct coro_init_args   args;

                args.func = coro;
                args.arg = arg;
                args.self = ctx;
                args.main = &nctx;

                pthread_attr_init(&attr);
  #if __UCLIBC__
                /* exists, but is borked */
                /*pthread_attr_setstacksize (&attr, (size_t)ssize);*/
  #elif __CYGWIN__
                /* POSIX, not here */
                pthread_attr_setstacksize(&attr, (size_t)ssize);
  #else
                pthread_attr_setstack(&attr, sptr, (size_t)ssize);
  #endif
                pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);
                pthread_create(&ctx->id, &attr, coro_init, &args);

                coro_transfer(args.main, args.self);
        } else {
                ctx->id = null_tid;
        }
}

void
coro_destroy(coro_context *ctx)
{
        if (!pthread_equal(ctx->id, null_tid)) {
                pthread_cancel(ctx->id);
                pthread_mutex_unlock(&coro_mutex);
                pthread_join(ctx->id, 0);
                pthread_mutex_lock(&coro_mutex);
        }

        pthread_cond_destroy(&ctx->cv);
}

/*****************************************************************************/
/* fiber backend                                                             */
/*****************************************************************************/
#elif CORO_FIBER

  #define WIN32_LEAN_AND_MEAN
  #if _WIN32_WINNT < 0x0400
    #undef _WIN32_WINNT
    #define _WIN32_WINNT 0x0400
  #endif
  #include <windows.h>

VOID CALLBACK
coro_init(PVOID arg)
{
        coro_context *ctx = (coro_context *)arg;

        ctx->coro(ctx->arg);
}

void
coro_transfer(coro_context *prev, coro_context *next)
{
        if (!prev->fiber) {
                prev->fiber = GetCurrentFiber();

                if ((prev->fiber == 0) || (prev->fiber == (void *)0x1e00)) {
                        prev->fiber = ConvertThreadToFiber(0);
                }
        }

        SwitchToFiber(next->fiber);
}

void
coro_create(coro_context *ctx, coro_func coro, void *arg, void *sptr, size_t ssize)
{
        ctx->fiber = 0;
        ctx->coro = coro;
        ctx->arg = arg;

        if (!coro) {
                return;
        }

        ctx->fiber = CreateFiber(ssize, coro_init, ctx);
}

void
coro_destroy(coro_context *ctx)
{
        DeleteFiber(ctx->fiber);
}

#else
  #error unsupported backend
#endif /* if CORO_UCONTEXT || CORO_SJLJ || CORO_LOSER || CORO_LINUX || CORO_IRIX || CORO_ASM */

/*****************************************************************************/
/* stack management                                                          */
/*****************************************************************************/
#if CORO_STACKALLOC

  #include <stdlib.h>

  #ifndef _WIN32
    #include <unistd.h>
  #endif

  #if CORO_USE_VALGRIND
    #include <valgrind/valgrind.h>
  #endif

  #if _POSIX_MAPPED_FILES
    #include <sys/mman.h>
    #define CORO_MMAP           1
    #ifndef MAP_ANONYMOUS
      #ifdef MAP_ANON
        #define MAP_ANONYMOUS   MAP_ANON
      #else
        #undef CORO_MMAP
      #endif
    #endif
    #include <limits.h>
  #else
    #undef CORO_MMAP
  #endif

  #if _POSIX_MEMORY_PROTECTION
    #ifndef CORO_GUARDPAGES
      #define CORO_GUARDPAGES 4
    #endif
  #else
    #undef CORO_GUARDPAGES
  #endif

  #if !CORO_MMAP
    #undef CORO_GUARDPAGES
  #endif

  #if !__i386 && !__x86_64 && !__powerpc && !__m68k && !__alpha && !__mips && !__sparc64
    #undef CORO_GUARDPAGES
  #endif

  #ifndef CORO_GUARDPAGES
    #define CORO_GUARDPAGES 0
  #endif

  #if !PAGESIZE
    #if !CORO_MMAP
      #define PAGESIZE 4096
    #else
static inline
size_t coro_pagesize(void)
{
        static THREAD_VAR size_t pagesize;

        if (!pagesize) {
                pagesize = sysconf(_SC_PAGESIZE);
        }

        return pagesize;
}

      #define PAGESIZE coro_pagesize()
    #endif
  #endif

int
coro_stack_alloc(struct coro_stack *stack, unsigned int size)
{
        if (!size) {
  #ifdef MINSIGSTKSZ
                size = MINSIGSTKSZ;
  #else
                size = 256 * 1024;
  #endif
        }

        stack->sptr = 0;
        stack->ssze = ((size_t)size * sizeof(void *) + PAGESIZE - 1) / PAGESIZE * PAGESIZE;

  #if CORO_FIBER
        stack->sptr = (void *)stack;
        return 1;

  #else
        size_t  ssze = stack->ssze + CORO_GUARDPAGES * PAGESIZE;
        void    *base;

    #if CORO_MMAP
        /* mmap supposedly does allocate-on-write for us */
        base = mmap(0, ssze, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (unlikely(base == (void *)-1)) {
                /* some systems don't let us have executable heap */
                /* we assume they won't need executable stack in that case */
                base = mmap(0, ssze, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

                if (unlikely(base == (void *)-1)) {
                        return 0;
                }
        }

      #if CORO_GUARDPAGES
        mprotect(base, CORO_GUARDPAGES * PAGESIZE, PROT_NONE);
      #endif

        base = (void *)((char *)base + CORO_GUARDPAGES * PAGESIZE);
    #else
        base = malloc(ssze);

        if (unlikely(!base)) {
                return 0;
        }
    #endif /* if CORO_MMAP */

    #if CORO_USE_VALGRIND
        stack->valgrind_id = VALGRIND_STACK_REGISTER((char *)base, ((char *)base) + ssze - CORO_GUARDPAGES * PAGESIZE);
    #endif
        stack->sptr = base;
        return 1;
  #endif /* if CORO_FIBER */
}

void
coro_stack_free(struct coro_stack *stack)
{
  #if CORO_FIBER
        /* nop */
  #else
    #if CORO_USE_VALGRIND
        VALGRIND_STACK_DEREGISTER(stack->valgrind_id);
    #endif

    #if CORO_MMAP
        if (stack->sptr) {
                munmap((void *)((char *)stack->sptr - CORO_GUARDPAGES * PAGESIZE),
                        stack->ssze + CORO_GUARDPAGES * PAGESIZE);
        }

    #else
        free(stack->sptr);
    #endif
  #endif
}
#endif /* if CORO_STACKALLOC */