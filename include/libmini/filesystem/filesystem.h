//
//  filesystem.h
//  libmini
//
//  Created by 周凯 on 15/9/16.
//  Copyright © 2015年 zk. All rights reserved.
//

#ifndef __minilib_filesystem_h__
#define __minilib_filesystem_h__

#include "../utils.h"

__BEGIN_DECLS

/**
 * 判断路径是否是文件夹
 * @param create 如果为真，在不存在时尝试创建
 * @return false 不是文件夹，且设置errno为ENOTDIR
 */
bool FS_IsDirectory(const char *path, bool create);

/*获取文件大小*/
#define FS_GetSize(path)		    \
	({				    \
		int _rc = 0;		    \
		struct stat _stat = {};	    \
		_rc = stat((path), &_stat); \
		RAISE_SYS_ERROR(_rc);	    \
		_stat.st_size;		    \
	})

__END_DECLS
#endif	/* __minilib_filesystem_h__ */

