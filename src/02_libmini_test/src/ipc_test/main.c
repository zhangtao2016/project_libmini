#include <mqueue.h>
#include "libmini.h"

int main(int argc, char const *argv[])
{
	char path[MAX_PATH_SIZE] = {};

	mqd_t           mqid;
	struct mq_attr  mqattr = {};

	if (!PosixIPCName(path, sizeof(path), "test.posix.mq")) {
		exit(EXIT_FAILURE);
	}

	mqattr.mq_maxmsg = 2000;
	mqattr.mq_msgsize = 512;

	mqid = mq_open(path, O_CREAT | O_EXCL | O_RDWR, PSIPC_MODE, NULL);

	if ((int)mqid < 0) {
		x_printf(E, "mq_open failure : %s.", x_strerror(errno));
		exit(EXIT_FAILURE);
	}

	x_printf(D, "create success");
	return 0;
}

