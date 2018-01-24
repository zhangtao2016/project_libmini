//
//  ipc_base.c
//  minilib
//
//  Created by 周凯 on 15/8/25.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#include "ipc/ipc_base.h"
#include "slog/slog.h"
#include "except/exception.h"

char *PosixIPCName(char *buff, size_t size, const char *ipcname)
{
	const char      *preName = NULL;
	const char      *slash = NULL;
	int             bytes = 0;

	assert(ipcname && ipcname[0] != '\0');
	assert(buff);

	return_val_if_fail(size > 0, NULL);

	if ((preName = getenv(PX_IPC_ENV_PRENAME)) == NULL) {
#ifdef POSIX_IPC_PRENAME
		pPreName = POSIX_IPC_PRENAME;
#else
		preName = PX_IPC_PRENAME;
#endif
	}

	slash = preName[strlen(preName) - 1] == '/' ? "" : "/";

	bytes = snprintf(buff, size, "%s%s%s", preName, slash, ipcname);

	if (unlikely(bytes >= size)) {
		x_printf(E, "not enough space to construct a name of posix-ipc.");
		return NULL;
	}

	return buff;
}

