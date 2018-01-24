//
//  main.c
//  supex
//
//  Created by 周凯 on 15/12/28.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include <stdio.h>
#include "cache/cache.h"

int main()
{
	struct cache    cache = {};
	bool            flag = false;

	TRY
	{
		flag = cache_init(&cache);
		assert(flag);
		RAISE(EXCEPT_SYS);
		
		
	}
	CATCH
	{
		
	}
	FINALLY
	{
		cache_finally(&cache);
	}
	END;
	
	flag = cache_init(&cache);
	assert(flag);

	
	
	
	Rand();
	ssize_t bytes = 0;
	ssize_t pushbytes = 0;
	ssize_t pullbytes = 0;
	char    buff[2048];

	while (1) {
		int pbytes = (int)RandInt(1, 128);
		bytes = cache_append(&cache, buff, pbytes);

		if (likely(bytes > 0)) {
			printf("expect push `"
				LOG_I_COLOR "%d"LOG_COLOR_NULL
				"` bytes, and push `"
				LOG_I_COLOR "%d"LOG_COLOR_NULL "` bytes \n",
				pbytes, bytes);
			pushbytes += bytes;
		} else {
			printf("expect push `"
				LOG_I_COLOR "%d"LOG_COLOR_NULL
				"` bytes, but push "
				LOG_E_COLOR "FAILURE"LOG_COLOR_NULL ".\n", pbytes);
			bytes = cache_incrmem(&cache, pbytes);
			printf("***** increment `"LOG_I_COLOR "%d"LOG_COLOR_NULL "` bytes: ", bytes);

			if (likely(bytes > 0)) {
				printf(LOG_I_COLOR "SUCCESS"LOG_COLOR_NULL " *****\n");
			} else {
				printf(LOG_E_COLOR "FAILURE"LOG_COLOR_NULL " *****\n");
			}

			break;
		}
	}

	printf("-----------------------------\n");

	while (1) {
		unsigned pbytes = (unsigned)RandInt(1, 64);
		bytes = cache_get(&cache, buff, pbytes);

		if (likely(bytes > 0)) {
			printf("expect pull `"
				LOG_I_COLOR "%d"LOG_COLOR_NULL
				"` bytes, and pull `"
				LOG_I_COLOR "%d"LOG_COLOR_NULL "` bytes \n",
				pbytes, bytes);
			pullbytes += bytes;
		} else {
			printf("***** pull `"LOG_I_COLOR "%d"LOG_COLOR_NULL "` bytes: ", pbytes);
			printf(LOG_E_COLOR "%s"LOG_COLOR_NULL " *****\n", x_strerror(errno));
			break;
		}
	}
	
	assert(pushbytes == pullbytes);
	/**/
	printf("-----------------------------\n");
	
	cache_clean(&cache);
	
	bytes = cache_appendf(&cache, "%s", "kai");
	assert(bytes == sizeof("kai") - 1);
	
	bytes = cache_get(&cache, buff, 2);
	AssertRaise(bytes == 2, EXCEPT_SYS);
	
	bytes = cache_insert(&cache, buff, 2);
	AssertRaise(bytes == 2, EXCEPT_SYS);
	
	cache_movemem(&cache);

	bytes = cache_insertf(&cache, "%s", "zhou");
	assert(bytes == sizeof("zhou") - 1);

	bytes = cache_get(&cache, buff, sizeof(buff));
	assert(bytes == sizeof("zhoukai") - 1);

	return EXIT_SUCCESS;
}

