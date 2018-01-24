//
//  exception.h
//
//
//  Created by 周凯 on 15/8/28.
//
//

#ifndef __minilib__exception__
#define __minilib__exception__

#include "except.h"

__BEGIN_DECLS
/* -------              */
/** 系统调用错误*/
#define EXCEPT_SYS_CODE            (0xffffffff)
extern const ExceptT EXCEPT_SYS;
/** 表达式断言失败*/
#define EXCEPT_ASSERT_CODE       (0xfffffffe)
extern const ExceptT EXCEPT_ASSERT;
/** 对象没有被初始化*/
#define EXCEPT_OBJECT_CODE         (0xfffffffd)
extern const ExceptT EXCEPT_OBJECT;

/* -------              */
__END_DECLS
#endif	/* defined(__minilib__exception__) */

