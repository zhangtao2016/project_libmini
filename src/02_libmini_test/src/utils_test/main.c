#include "libmini.h"

void test_likely(bool x)
{
	if (likely(x)) {
		x_printf(I, "TEST likely() : true.");
	} else {
		x_printf(I, "TEST likely() : false.");
	}
}

void test_unlikely(bool x)
{
	if (unlikely(x)) {
		x_printf(I, "TEST unlikely() : true.");
	} else {
		x_printf(I, "TEST unlikely() : false.");
	}
}

void who_caller()
{
	char    myname[32] = { 0 };
	char    caller[32] = { 0 };
	char    callercaller[32] = { 0 };

	GetCallerName(myname, sizeof(myname), -1);

	x_printf(I, "my name : `%s`", myname);

	GetCallerName(myname, sizeof(myname), 0);

	x_printf(I, "my name : `%s`", myname);

	GetCallerName(caller, sizeof(caller), 1);

	x_printf(I, "`%s` call `%s`", caller, myname);

	GetCallerName(callercaller, sizeof(callercaller), 2);

	x_printf(I, "`%s` call `%s`", callercaller, caller);
}

static unsigned short stopwatch[] = {
	S16 _ _ _ _ _ X X X X X _ _ X X X X,
	S16 _ _ _ X X X X X X X X X _ X X X,
	S16 _ _ X X X _ _ _ _ _ X X X _ X X,
	S16 _ X X X _ _ _ _ _ _ _ X X X _ X,
	S16 _ X X _ _ _ _ _ _ _ _ _ X X _ _,
	S16 X X _ _ _ _ _ _ _ _ _ _ _ X X _,
	S16 X X _ _ _ _ _ _ _ _ _ _ _ X X _,
	S16 X X _ X X X X X _ _ _ _ _ X X _,
	S16 X X _ _ _ _ _ X _ _ _ _ _ X X _,
	S16 X X _ _ _ _ _ X _ _ _ _ _ X X _,
	S16 _ X X _ _ _ _ X _ _ _ _ X X _ _,
	S16 _ X X X _ _ _ X _ _ _ X X X _ _,
	S16 _ _ X X X _ _ _ _ _ X X X _ _ _,
	S16 _ _ _ X X X X X X X X X _ _ _ _,
	S16 _ _ _ _ _ X X X X X _ _ _ _ _ _,
};

int main(int argc, char **argv)
{
	int     i = 0;
	int     cnt = DIM(stopwatch);

	for (i = 0; i < cnt; i++) {
		char    *ptr = binary2bitrstr((char *)&stopwatch[i], sizeof(stopwatch[0]));
		char    *tmp = ptr;
		//		printf("0x%04x:", stopwatch[i]);

		while (*tmp) {
			printf("%c ", *tmp == '1' ? 'X' : '_');
			tmp++;
		}

		printf("\n");
		free(ptr);
	}

	// return 0;

	char    buff[32] = {};
	char    tm[TM_STRING_SIZE] = {};

	time_t  sec = 0;
	int     usec = 0;
	struct
	{
		/* data */
		int magic;
	} obj = {};

	ASSERTOBJ(&obj);

	TM_GetTimeStamp(&sec, &usec);

	TM_FormatTime(tm, sizeof(tm), sec, TM_FORMAT_DATETIME);

	x_printf(D, "start at %s : %d.", tm, usec);

	x_printf(I, "program : %s.", GetProgName(buff, sizeof(buff)));
	SLogOpen("log.out", NULL);

	x_perror("TEST x_strerror() :%s.", x_strerror(EINTR));

	test_likely(true);
	test_likely(false);
	test_unlikely(true);
	test_unlikely(false);

	who_caller();

	SLogSetLevel(SLOG_F_LEVEL);

	/*以下日志不会被打印*/
	x_printf(E, "Must not appear this message.");

	char *volatile ptr = NULL;

	TRY
	{
		New(ptr);
		AssertError(ptr, ENOMEM);

		RAISE(EXCEPT_ASSERT);
	}
	CATCH
	{
		if (ptr) {
			/* code */
			Free(ptr);
		} else {
			x_printf(W, "please use `volatile` key word.");
		}
	}
	FINALLY
	{}
	END;

	return EXIT_SUCCESS;
}

