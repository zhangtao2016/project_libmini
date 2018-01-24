//
//  pipe.h
//  minilib
//
//  Created by 周凯 on 15/8/24.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __minilib__pipe__
#define __minilib__pipe__

#include <fcntl.h>
#include <sys/stat.h>
#include "../utils.h"

__BEGIN_DECLS

/**
 * 创建并打开命名管道
 * @param flag 如果flag & O_TRUNC 为真则先unlink(path)
 * @param mode 访问权限
 * @return -1 failure; >=0 success
 */
int CreateFIFO(const char *path, int flag, int mode);

__END_DECLS
#endif	/* defined(__minilib__pipe__) */

