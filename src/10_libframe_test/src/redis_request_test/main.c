//
//  main.c
//  supex
//
//  Created by 周凯 on 16/01/13.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include "tcp_api/redis_api/redis_string.h"
#include "tcp_api/tcp_io.h"
#define TIME_OUT (-1)
static void redis_request_send(struct tcp_socket *socket);
static void print_redis_proto(struct cache *cache);

#define test(tcp, msg, ...)							   \
	do {									   \
		ssize_t bytes = 0;						   \
		cache_clean(&(tcp)->cache);					   \
		bytes = redis_stringf(&(tcp)->cache, (msg), ##__VA_ARGS__); \
		RAISE_SYS_ERROR(bytes);						   \
		redis_request_send((tcp));					   \
	} while (0)



int main(int argc, char const *argv[])
{
	if (unlikely(argc != 3)) {
		x_perror("usage %s <ip> <port>", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	struct tcp_socket *tcp = NULL;
	
	TRY
	{
		tcp = tcp_connect(argv[1], argv[2], -1);
		AssertRaise(tcp, EXCEPT_SYS);
		
		test(tcp, "Set age %d", 30);
		test(tcp, "Get %s", "age");
		
		test(tcp, "lpush %s %f %lf %2.2f %.*f %*.*lf", "list", 2.0f, 3.1f, 4.555, 2, 165.546f, 4, 5, 2016.01201801f);
		test(tcp, "lrange %s %d %d", "list", 0, -1);
		
		ssize_t bytes = 0;
		struct redis_string rreq = { .cache = &tcp->cache };
		
		redis_string_init(&rreq);
		
		bytes = redis_string_append(&rreq, "SET", -1);
		RAISE_SYS_ERROR(bytes);
		
		bytes = redis_string_append(&rreq, "binary", -1);
		RAISE_SYS_ERROR(bytes);
		
		/*二进制数据*/
		bytes = redis_string_append(&rreq, "\x01\x02\x03", 3);
		RAISE_SYS_ERROR(bytes);
		
		bytes = redis_string_complete(&rreq);
		RAISE_SYS_ERROR(bytes);
		
		redis_request_send(tcp);
		
		test(tcp, "Get binary");
		
		redis_string_init(&rreq);
		
		bytes = redis_string_append(&rreq, "SET", -1);
		RAISE_SYS_ERROR(bytes);
		
		bytes = redis_string_append(&rreq, "binary", -1);
		RAISE_SYS_ERROR(bytes);
		
		bytes = redis_string_appendf(&rreq, "%s", "\\x01\\x02\\x03");
		RAISE_SYS_ERROR(bytes);
		
		bytes = redis_string_complete(&rreq);
		RAISE_SYS_ERROR(bytes);
		
		redis_request_send(tcp);
		
		test(tcp, "Get binary");
		
	}
	CATCH
	{
		
	}
	END;
#if 0
	//	struct redis_request req = { };
	struct cache    cache = {};
	bool            bflag = false;
	bflag = cache_init(&cache);
	assert(bflag);

	//	req.cache = &cache;

	ssize_t bytes = 0;
	cache_clean(&cache);

	bytes = redis_stringf(&cache, " GET %s ", "zhoukai");
	RAISE_SYS_ERROR(bytes);
	printf("---------------------------------------------------\n");
	printf("%.*s", cache.end - cache.start, &cache.buff[cache.start]);

	cache_clean(&cache);
	bytes = redis_stringf(&cache, " GET %s  %d  ", "zhoukai", 30);
	RAISE_SYS_ERROR(bytes);
	printf("---------------------------------------------------\n");
	printf("%.*s", cache.end - cache.start, &cache.buff[cache.start]);

	cache_clean(&cache);
	bytes = redis_stringf(&cache, " GET %s  %d  %.2f", "zhoukai", 30, 164.5f);
	RAISE_SYS_ERROR(bytes);
	printf("---------------------------------------------------\n");
	printf("%.*s", cache.end - cache.start, &cache.buff[cache.start]);

	cache_clean(&cache);
	bytes = redis_stringf(&cache, " GET %s  %d  %.2f %.*lf", "zhoukai", 30, 164.5f, 2, 2015.0111f);
	RAISE_SYS_ERROR(bytes);
	printf("---------------------------------------------------\n");
	printf("%.*s", cache.end - cache.start, &cache.buff[cache.start]);

	cache_clean(&cache);
	bytes = redis_stringf(&cache, " GET %s  %d  %2f %*lf", "zhoukai", 30, 164.5f, 2, 2015.0111f);
	RAISE_SYS_ERROR(bytes);
	printf("---------------------------------------------------\n");
	printf("%.*s", cache.end - cache.start, &cache.buff[cache.start]);

	cache_clean(&cache);
	bytes = redis_stringf(&cache, " GET %s  %d  %2.2f %*.*lf", "zhoukai", 30, 164.5f, 2, 4, 2015.0111f);
	RAISE_SYS_ERROR(bytes);
	printf("---------------------------------------------------\n");
	printf("%.*s", cache.end - cache.start, &cache.buff[cache.start]);

	
	printf("---------------------------------------------------\n");
	printf("%.*s", cache.end - cache.start, &cache.buff[cache.start]);
	printf("---------------------------------------------------\n");
#endif
	/* code */
	return 0;
}

static void redis_request_send(struct tcp_socket *socket)
{
	ssize_t bytes = 0;
	socket->timeout = TIME_OUT;
	/*发送*/
	printf("SEND : \n");
	print_redis_proto(&socket->cache);
	bytes = tcp_send(socket, NULL, NULL);
	RAISE_SYS_ERROR(bytes);
	/*接收，仅阻塞10毫秒*/
	socket->timeout = 10;
	socket->miou = 1024;
	bytes = tcp_recv(socket, NULL, NULL, NULL);
	if (unlikely(bytes < 0 && errno != ETIMEDOUT)) {
		RAISE_SYS_ERROR(bytes);
	}
	printf("RECV : \n");
	print_redis_proto(&socket->cache);
}

static void print_redis_proto(struct cache *cache)
{
	printf("---------------------------------------------------\n");
	unsigned len = cache_data_length(cache);
	if (likely(len)) {
		unsigned i = 0;
		for (i = 0; i < len; i++) {
			unsigned char ch = cache_data_address(cache)[i];
			if (likely(isprint(ch))) {
				printf("%c", ch);
			} else {
				if (ch == '\r') {
					printf("\\r");
				} else if (ch == '\n') {
					printf("\\n");
				} else if (ch == '\t') {
					printf("\\t");
				} else {
					printf("\\x%02x", ch);
				}
			}
		}
	} else {
		printf("NO DATA IN THIS CACHE");
	}
	printf("\n---------------------------------------------------\n");
}


