//
//  main.c
//  supex
//
//  Created by 周凯 on 15/12/23.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include <stdio.h>
#include "libmini.h"

#define KEY_LEN 6

static DHashT   g_Hash = NULL;
static int      g_insertTotal = 0;
static int      g_getTotal = 0;
static int      g_delTotal = 0;

static void *insert_test(DHashT arg);

static void *get_test(DHashT arg);

static void *del_test(DHashT arg);

static int compareKey(const char *data, int size, void *usr, int usrsize);

static void destroyKey(const char *data, int size);

static void destroyValue(const char *data, int size);

int main(int argc, char **argv)
{
	Rand();

	if (unlikely(argc != 3)) {
		x_printf(E, "usage %s <size> <total>", argv[0]);
		return EXIT_FAILURE;
	}

	g_delTotal = g_getTotal = g_insertTotal = atoi(argv[2]);
	assert(g_insertTotal > 0);
	g_Hash = DHashCreate(atoi(argv[1]), HashTime33, compareKey);
	assert(g_Hash);
	g_Hash->destroyk = destroyKey;
	g_Hash->destroyv = destroyValue;

#if 0
	pthread_t       ptid[4];
	int             i = 0;

	for (i = 0; i < DIM(ptid); i++) {
		pthread_create(&ptid[i], NULL, insert_test, g_Hash);
	}

	for (i = 0; i < DIM(ptid); i++) {
		pthread_join(ptid[i], NULL);
	}

#else
	insert_test(g_Hash);
	get_test(g_Hash);
	del_test(g_Hash);
#endif

	char    key[] = "ABC";
	char    *value = NULL;
	NewArray0(10, value);
	assert(value);

	memcpy(value, "123456", 6);

	DHashPut(g_Hash, key, -1, (char *)&value, sizeof(value));
	char            *fvalue = NULL;
	unsigned        fsize = sizeof(fvalue);
	DHashGet(g_Hash, key, -1, (char *)&fvalue, &fsize);
	DHashDel(g_Hash, key, -1);

	DHashDestroy(&g_Hash);

	return EXIT_SUCCESS;
}

static void *insert_test(DHashT arg)
{
	char    key[KEY_LEN] = {};
	char    *value = NULL;

	while (ATOMIC_F_SUB(&g_insertTotal, 1) > 0) {
		//
		int     len = (int)RandInt(1, DIM(key) - 1);
		int     i = 0;

		for (i = 0; i < len; i++) {
			key[i] = (char)RandInt('0', '9');
		}

		key[i] = '\0';

		len = (int)RandInt(1, 32);
		NewArray0(len, value);
		assert(value);

		for (i = 0; i < len - 1; i++) {
			value[i] = (char)RandInt('a', 'z');
		}

		value[i] = '\0';

		bool flag = false;
		flag = DHashPut(arg, key, -1, (char *)&value, sizeof(value));
		printf("INSERT [%s] = [%s] : %s\n", key, value,
			likely(flag) ? LOG_I_COLOR "SUCCESS" LOG_COLOR_NULL :
			LOG_E_COLOR "FAILURE" LOG_COLOR_NULL);

		if (unlikely(!flag)) {
			free(value);
		}
	}

	return NULL;
}

static void *get_test(DHashT arg)
{
	char            key[KEY_LEN] = {};
	char            *value = NULL;
	unsigned        size = 0;

	while (ATOMIC_F_SUB(&g_getTotal, 1) > 0) {
		int     len = (int)RandInt(1, DIM(key) - 1);
		int     i = 0;

		for (i = 0; i < len; i++) {
			key[i] = (char)RandInt('0', '9');
		}

		key[i] = '\0';

		bool flag = false;
		size = sizeof(value);
		flag = DHashGet(arg, key, -1, (char *)&value, &size);

		if (unlikely(flag)) {
			assert(size == sizeof(intptr_t));
			printf("GET    [%s] = [%s] : %s\n", key, value, LOG_I_COLOR "SUCCESS" LOG_COLOR_NULL);
		} else {
			printf("GET    [%s] : %s\n", key, LOG_E_COLOR "FAILURE" LOG_COLOR_NULL);
		}
	}

	return NULL;
}

static void *del_test(DHashT arg)
{
	char key[KEY_LEN] = {};

	while (ATOMIC_F_SUB(&g_delTotal, 1) > 0) {
		int     len = (int)RandInt(1, DIM(key) - 1);
		int     i = 0;

		for (i = 0; i < len; i++) {
			key[i] = (char)RandInt('0', '9');
		}

		key[i] = '\0';

		bool flag = false;
		flag = DHashDel(arg, key, -1);
		printf("DEL    [%s] : %s\n", key,
			unlikely(flag) ? LOG_I_COLOR "SUCCESS" LOG_COLOR_NULL :
			LOG_E_COLOR "FAILURE" LOG_COLOR_NULL);
	}

	return NULL;
}

static int compareKey(const char *data, int size, void *usr, int usrsize)
{
	if ((size == usrsize) &&
		(memcmp(data, usr, size) == 0)) {
		return 0;
	}

	return -1;
}

static void destroyKey(const char *data, int size)
{
	//	printf("[%.*s]", size, data);
	memset((void *)data, 0, size);
}

static void destroyValue(const char *data, int size)
{
	//	if (size > 0) {
	//		printf(" = [%.*s]\n", size, data);
	//	} else {
	//		printf("\n");
	//	}
	free(*(char **)data);
	memset((void *)data, 0, size);
}

