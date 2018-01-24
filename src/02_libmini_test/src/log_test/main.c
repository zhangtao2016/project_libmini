#include "libmini.h"
#include <pthread.h>

typedef struct
{
	const char cmd[200];
} ArgT;

void *log_test(void *argv)
{
	BindCPUCore(-1);

	ArgT *a = (ArgT *)argv;

	while (1) {
		// usleep(20000);
		x_printf(I, "log test `%s` ...", a->cmd);
	}
}

int main(int argc, char **argv)
{
	//        pthread_t       tid[4];
	//        int             i = 0;
	//        ArgT            arg = { "this a message from user." };

	SLogOpen("log.out", SLOG_D_LEVEL);

	//        for (i = 0; i < DIM(tid); i++) {
	//                pthread_create(&tid[i], NULL, log_test, &arg);
	//        }
	//
	//        for (i = 0; i < DIM(tid); i++) {
	//                pthread_join(tid[i], NULL);
	//        }

	x_printf(D, "this a debug information ...");
	x_printf(I, "this a information ...");
	x_printf(W, "this a warning information ...");
	x_printf(E, "this a error information ...");
	x_printf(S, "this a system information ...");
	x_printf(F, "this a fatal information ...");

	return EXIT_SUCCESS;
}

