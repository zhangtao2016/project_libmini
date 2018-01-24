#include "libmini.h"

#ifdef __APPLE__
  #define _XOPEN_SOURCE
#endif

#include <ucontext.h>

struct scheudler;
struct coroutine;
enum corstat;

typedef void (*corapi)(struct coroutine *, void *);

enum corstat
{
	cor_start,
	cor_suspend,
	cor_end,
};

struct scheudler
{
	ucontext_t      ctx;
	char            stack[4096];
};

struct coroutine
{
	ucontext_t              ctx;
	corapi                  task;
	void                    *usr;
	enum corstat            stat;
	char                    stack[4096];
	size_t                  size;
	struct scheudler        *sche;
};

void corproto(struct coroutine *c)
{
	assert(c);

	x_printf(D, "coroutine start ...");

	if (c->task) {
		c->task(c, c->usr);
	}

	c->stat = cor_end;

	x_printf(D, "coroutine over ...");
}

void corstart(struct scheudler *s, struct coroutine *c, corapi task, void *usr)
{
	int flag = 0;

	assert(c);
	assert(s);

	flag = getcontext(&c->ctx);

	RAISE_SYS_ERROR(flag);

	c->ctx.uc_link = &s->ctx;
	c->ctx.uc_stack.ss_sp = &c->stack[0];
	c->ctx.uc_stack.ss_size = sizeof(c->stack);
	c->stat = cor_start;
	c->task = task;
	c->usr = usr;
	c->sche = s;
}

void corresume(struct coroutine *c)
{
	int flag = 0;

	assert(c);
	assert(c->sche);

	switch (c->stat)
	{
		case cor_start:
			makecontext(&c->ctx, (void(*))corproto, 1, c);
			// 在此切出，并运行corproto函数
			flag = swapcontext(&c->sche->ctx, &c->ctx);
			RAISE_SYS_ERROR(flag);
			break;

		case cor_suspend:
			// 在此切出
			flag = swapcontext(&c->sche->ctx, &c->ctx);
			RAISE_SYS_ERROR(flag);
			// 在此处切回
			break;

		default:
			break;
	}
}

bool corisactive(struct coroutine *c)
{
	assert(c);

	return !(c->stat != cor_start && c->stat != cor_suspend);
}

void coryield(struct coroutine *c)
{
	int flag = 0;

	assert(c);

	c->stat = cor_suspend;

	x_printf(D, "coroutine suspend ...");

	// 切出成功时在此跳出
	flag = swapcontext(&c->ctx, &c->sche->ctx);
	// 切出失败时在此继续
	RAISE_SYS_ERROR(flag);

	// 切回时，在此运行
}

void work(struct coroutine *c, void *usr)
{
	int     loops = (int)(intptr_t)usr;
	int     i = 0;

	for (i = 0; i < loops; i++) {
		x_printf(I, "coroutine %d ...", i);
		coryield(c);
		x_printf(I, "coroutine continue ...");
	}

	RAISE(EXCEPT_ASSERT);
}

int main()
{
	struct scheudler s = {};

	struct coroutine c = {};

	TRY
	{
		x_printf(D, "Test start ...");

		corstart(&s, &c, work, (void *)(intptr_t)10);

		while (corisactive(&c)) {
			corresume(&c);
		}
	}
	CATCH
	{
		TRY
		{
			x_printf(D, "Sub Test start ...");

			corstart(&s, &c, work, (void *)(intptr_t)2);

			while (corisactive(&c)) {
				corresume(&c);
			}
		}
		CATCH
		{}
		FINALLY
		{
			x_printf(D, "Sub Test over ...");
		}
		END;
	}
	FINALLY
	{
		x_printf(D, "Test over ...");
	}
	END;

	return EXIT_SUCCESS;
}

