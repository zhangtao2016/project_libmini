//
//  http_string.c
//  supex
//
//  Created by 周凯 on 15/12/25.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include "tcp_api/http_api/http_string.h"

static const char *HTTP_RESP_INFO[] = {
	"HTTP/1.1 200 OK\r\n"
	"Server: supex/1.4.2\r\n"
	"Content-Type: text/html\r\n"
	"Connection: close\r\n"
	"\r\n",

	"HTTP/1.1 200 OK\r\n"
	"Server: supex/1.4.2\r\n"
	"Content-Type: text/html\r\n"
	"Content-Length: 40\r\n"
	"Connection: Keep-Alive\r\n"
	"\r\n"
	"<html>\r\n<head><title>OK</title></head>\r\n",

	"HTTP/1.1 400 Not Found\r\n"
	"Server: supex/1.4.2\r\n"
	"Content-Type: text/html\r\n"
	"Content-Length: 168\r\n"
	"Connection: close\r\n"
	"\r\n"
	"<html>\r\n<head><title>404 Not Found</title></head>\r\n"
	"<body bgcolor=\"white\">\r\n<center><h1>404 Not Found</h1></center>\r\n"
	"<hr><center>supex/1.4.2</center>\r\n</body>\r\n</html>\r\n",

	"HTTP/1.1 413 Request Entity Too Large\r\n"
	"Server: supex/1.4.2\r\n"
	"Content-Type: text/html\r\n"
	"Content-Length: 198\r\n"
	"Connection: close\r\n"
	"\r\n"
	"<html>\r\n<head><title>413 Request Entity Too Large</title></head>\r\n"
	"<body bgcolor=\"white\">\r\n<center><h1>413 Request Entity Too Large</h1></center>\r\n"
	"<hr><center>supex/1.4.2</center>\r\n</body>\r\n</html>\r\n",

	"HTTP/1.1 500 Internal Server Error\r\n"
	"Server: supex/1.4.2\r\n"
	"Content-Type: text/html\r\n"
	"Content-Length: 192\r\n"
	"Connection: close\r\n"
	"\r\n"
	"<html>\r\n<head><title>500 Internal Server Error</title></head>\r\n"
	"<body bgcolor=\"white\">\r\n<center><h1>500 Internal Server Error</h1></center>\r\n"
	"<hr><center>supex/1.4.2</center>\r\n</body>\r\n</html>\r\n",

	"HTTP/1.1 503 Service Unavailable\r\n"
	"Server: supex/1.4.2\r\n"
	"Content-Type: text/html\r\n"
	"Content-Length: 188\r\n"
	"Connection: close\r\n"
	"\r\n"
	"<html>\r\n<head><title>503 Service Unavailable</title></head>\r\n"
	"<body bgcolor=\"white\">\r\n<center><h1>503 Service Unavailable</h1></center>\r\n"
	"<hr><center>supex/1.4.2</center>\r\n</body>\r\n</html>\r\n"
};

int http_string_resp(struct cache *cache, int code, bool keepalive)
{
	ASSERTOBJ(cache);

	if (unlikely(cache->start != cache->end)) {
		/*使用中*/
		errno = EINPROGRESS;
		return -1;
	}

	int pos = 0;
	switch (code)
	{
		case 200:
			pos = keepalive ? 1 : 0;
			break;

		case 400:
			pos = 2;
			break;

		case 413:
			pos = 3;
			break;

		case 500:
			pos = 4;
			break;

		default:
			pos = 5;
			break;
	}

	pos = INRANGE(pos, 0, sizeof(HTTP_RESP_INFO) - 1);

	return (int)cache_append(cache, HTTP_RESP_INFO[pos], (int)strlen(HTTP_RESP_INFO[pos]));
}

