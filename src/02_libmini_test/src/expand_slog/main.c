//
//  main.c
//  supex
//
//  Created by 周凯 on 15/12/19.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include <stdio.h>
#include "libmini.h"

static const SLogLevelT g_expandLevel[] = {
	{ 1, "T1", "TRACE1" },
	{ 2, "T2", "TRACE2" },
};

static SLogCfgT g_expandSlog = {
	.fd             = STDOUT_FILENO,
	.crtv           = 0,
	.lock           = NULLOBJ_AO_SPINLOCK,
	.clevel         = &g_expandLevel[0],
	.alevel         = &g_expandLevel[0],
	.alevels        = DIM(g_expandLevel),
};

#define T1      (&g_expandLevel[0])
#define T2      (&g_expandLevel[1])

#define TRACE(level, ...) \
	(SLogWrite)(&g_expandSlog, (level), __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)

int main(int argc, char **argv)
{
	/*
	 * 打开日至文件
	 */
	if (argc == 2) {
		(SLogOpen)(&g_expandSlog, argv[1], NULL);
	}

	TRACE(T1, "%s", "This level-1 of expand-level");

	TRACE(T2, "%s", "This level-2 of expand-level");

	return EXIT_SUCCESS;
}

