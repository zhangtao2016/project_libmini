//
//  filesystem.c
//  libmini
//
//  Created by 周凯 on 15/9/16.
//  Copyright © 2015年 zk. All rights reserved.
//
#include <fcntl.h>
#include <sys/stat.h>
#include "filesystem/filesystem.h"
#include "slog/slog.h"
#include "except/exception.h"

bool FS_IsDirectory(const char *path, bool create)
{
	struct stat     st = {};
	int             flag = 0;

	assert(path);

	flag = stat(path, &st);

	if (unlikely(flag < 0)) {
		int code = errno;

		if (unlikely(code != ENOENT)) {
			return false;
		}

		if (create) {
			flag = mkdir(path, 0777);

			if (unlikely((flag < 0) && (code != EEXIST))) {
				x_perror("mkdir() by `%s` : %s", path, x_strerror(errno));
				return false;
			}
		}
	} else if (unlikely(!S_ISDIR(st.st_mode))) {
		errno = ENOTDIR;
		return false;
	}

	return true;
}

