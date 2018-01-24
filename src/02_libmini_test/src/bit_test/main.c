#include "libmini.h"
#include <netinet/ip.h>

/*测试结构体位段定义*/
struct bit64
{
#if BYTE_ORDER == LITTLE_ENDIAN
	int64_t low     : 32,		/* low bytes */
		hight   : 32;		/* hight bytes */
#endif
#if BYTE_ORDER == BIG_ENDIAN
	int64_t low     : 32,		/* low bytes */
		hight   : 32;		/* hight bytes */
#endif
};

static inline void setbit64(struct bit64 *ptr, int32_t low, int32_t hight)
{
	assert(ptr);
	ptr->low = low;
	ptr->hight = hight;
}

#define setbit64low(ptr, val) \
	(*(&((ptr)->low)) = (val))

#define setbit64hight(ptr, val)	\
	(*(&((ptr)->hight)) = (val))

#define setbit64offset(ptr, val, offset) \
	(*(int32_t *)(((char *)(ptr)) + ((offset) / sizeof(char))) = (val))

int getsafamily(int fd)
{
	struct sockaddr_storage _sa = {};
	socklen_t               _nlen = sizeof(_sa);
	int                     _rc = -1;

	_rc = getsockname((fd), (SA)&_sa, &_nlen); \
	RAISE_SYS_ERROR(_rc);

	return ((SA)&_sa)->sa_family;
}

int main()
{
	struct bit64 value = {};

	*((int64_t *)&value) = 0xffffeeeeeeeeffff;

	value.hight = 0xeeeeffff;
	value.low = 0xffffeeee;

	setbit64(&value, 0x12345678, 0x87654321);

	setbit64offset(&value, 0x87654321, 0);
	setbit64offset(&value, 0x12345678, 32);

	x_printf(D, "0x%x 0x%x", value.low, value.hight);

	ATOMIC_SET((int64_t *)&value, 0x1234567890abcdef);

	x_printf(D, "0x%x 0x%x", value.low, value.hight);

	return EXIT_SUCCESS;
}

