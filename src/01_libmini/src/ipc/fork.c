//
//  fork.c
//
//
//  Created by 周凯 on 15/8/28.
//
//

#include "ipc/fork.h"
#include "atomic/atomic.h"

void InitProcess()
{
	ATOMIC_SET(&g_ThreadID, -1);
	ATOMIC_SET(&g_ProcessID, -1);
#if 1
	ATOMIC_SET(&_g_ef_, NULL);
	ATOMIC_SET(&g_errno, 0);
#endif
}

