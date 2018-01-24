//
//  exception.c
//
//
//  Created by 周凯 on 15/8/28.
//
//

#include "except/exception.h"

const ExceptT EXCEPT_SYS = {
	"System call error.",
	EXCEPT_SYS_CODE
};

const ExceptT EXCEPT_ASSERT = {
	"Assertion failed, Invalid arguments or conditions.",
	EXCEPT_ASSERT_CODE
};

const ExceptT EXCEPT_OBJECT = {
	"Object not been initialized.",
	EXCEPT_OBJECT_CODE
};

