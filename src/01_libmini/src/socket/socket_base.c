//
//  socket_base.c
//
//
//  Created by 周凯 on 15/9/6.
//
//

#include "socket/socket_base.h"
#include "socket/socket_opt.h"
#include "slog/slog.h"
#include "except/exception.h"

struct addrinfo *SA_GetAddrInfo(const char *host, const char *serv,
	int flag, int family, int socktype)
{
	struct addrinfo hints = {};
	struct addrinfo *addrinfo = NULL;
	int             n = 0;

	assert(host && serv);

	bzero(&hints, sizeof(hints));

	hints.ai_flags = AI_CANONNAME | flag;
	hints.ai_family = family;
	hints.ai_socktype = socktype;
again:
	n = getaddrinfo(host, serv, &hints, &addrinfo);

	if (unlikely(n != 0)) {
		if (likely(n == EAI_AGAIN)) {
			goto again;
		}

		x_printf(E, "Can't get address information "
			"about socket by [%s : %s] : %s.",
			SWITCH_NULL_STR(host),
			SWITCH_NULL_STR(serv),
			GaiStrerror(NEG(n)));

		RAISE_SYS_ERROR_ERRNO(NEG(n));
	}

	return addrinfo;
}

void SA_FreeAddrInfo(struct addrinfo **addrinfo)
{
	assert(addrinfo);

	if (likely(*addrinfo)) {
		freeaddrinfo(*addrinfo);
	}
}

char *SA_ntop(const struct sockaddr *naddr, char *paddr, size_t plen)
{
	const char *ptr = NULL;

	assert(naddr && paddr);

	return_val_if_fail(plen, NULL);

	bzero(paddr, plen);

	switch (naddr->sa_family)
	{
		case AF_INET:
			ptr = inet_ntop(AF_INET, &((SA4)naddr)->sin_addr,
					paddr, (socklen_t)plen);
			break;

		case AF_INET6:
			ptr = inet_ntop(AF_INET6, &((SA6)naddr)->sin6_addr,
					paddr, (socklen_t)plen);
			break;

		case AF_UNIX:
			snprintf(paddr, plen, "%s", ((SAU)naddr)->sun_path);
			ptr = paddr;
			break;

		default:
			RAISE_SYS_ERROR_ERRNO(EAFNOSUPPORT);
			break;
	}

	if (unlikely(ptr == NULL)) {
		RAISE(EXCEPT_SYS);
	}

	return (char *)ptr;
}

socklen_t SA_pton(const char *paddr, struct sockaddr *naddr, socklen_t nlen)
{
	int             flag = -1;
	socklen_t       len = 0;

	assert(paddr && naddr);

	switch (naddr->sa_family)
	{
		case AF_INET:
			len = sizeof(struct sockaddr_in);

			if (unlikely(nlen < len)) {
				errno = ENOSPC;
			} else {
				flag = inet_pton(AF_INET, paddr, &((SA4)naddr)->sin_addr);
			}

			break;

		case AF_INET6:
			len = sizeof(struct sockaddr_in6);

			if (unlikely(nlen < len)) {
				errno = ENOSPC;
			} else {
				flag = inet_pton(AF_INET6, paddr, &((SA6)naddr)->sin6_addr);
			}

			break;

		case AF_UNIX:
			len = offsetof(struct sockaddr_un, sun_path);

			if (unlikely(nlen < len)) {
				errno = ENOSPC;
			} else {
				size_t buffsize = nlen - len;

				len = snprintf(((SAU)naddr)->sun_path, buffsize, "%s", paddr);

				if (unlikely(len >= buffsize)) {
					errno = ENOSPC;
				} else {
					flag = 1;
				}
			}

			break;

		default:
			errno = EAFNOSUPPORT;
			break;
	}

	if (unlikely(flag == 0)) {
		errno = EADDRNOTAVAIL;
	}

	if (unlikely(flag < 1)) {
		x_printf(E, "Converts IP[%s] to SA failed : %s.", paddr, x_strerror(errno));
		RAISE(EXCEPT_SYS);
	}

	return len;
}

int SA_GetAFLevel(const struct sockaddr *naddr)
{
	int family = 0;

	family = SA_GetFamily(naddr);

	switch (family)
	{
		case AF_INET:
			return IPPROTO_IP;

			break;

		case AF_INET6:
			return IPPROTO_IPV6;

			break;

		default:
			RAISE_SYS_ERROR_ERRNO(EAFNOSUPPORT);
			break;
	}
	return -1;
}

int SF_GetAFLevel(int fd)
{
	int family = 0;

	family = SF_GetFamily(fd);

	switch (family)
	{
		case AF_INET:
			return IPPROTO_IP;

			break;

		case AF_INET6:
			return IPPROTO_IPV6;

			break;

		default:
			RAISE_SYS_ERROR_ERRNO(EAFNOSUPPORT);
			break;
	}
	return -1;
}

uint16_t SA_GetPort(const struct sockaddr *naddr)
{
	uint16_t port = 0;

	assert(naddr);
	switch (naddr->sa_family)
	{
		case AF_INET:
			port = ((SA4)naddr)->sin_port;
			break;

		case AF_INET6:
			port = ((SA6)naddr)->sin6_port;
			break;

		default:
			RAISE_SYS_ERROR_ERRNO(EAFNOSUPPORT);
			break;
	}

	return ntohs(port);
}

socklen_t SA_GetLength(const struct sockaddr *naddr)
{
	socklen_t len = 0;

	assert(naddr);

	switch (naddr->sa_family)
	{
		case AF_INET:
			len = sizeof(struct sockaddr_in);
			break;

		case AF_INET6:
			len = sizeof(struct sockaddr_in6);
			break;

		case AF_UNIX:
			len = (socklen_t)strlen(((SAU)naddr)->sun_path);
			break;

		default:
			RAISE_SYS_ERROR_ERRNO(EAFNOSUPPORT);
			break;
	}

	return len;
}

socklen_t SA_FillSocket(int family, const char *addr, uint16_t port,
	struct sockaddr *naddr, socklen_t nlen)
{
	socklen_t len = 0;

	assert(naddr);

	bzero(naddr, nlen);

	switch (family)
	{
		case AF_INET:
		{
			struct sockaddr_in *ptr = (SA4)naddr;

			if (nlen < sizeof(*ptr)) {
				errno = ENOSPC;
			} else {
				ptr->sin_family = AF_INET;
				ptr->sin_port = htons(port);
				ptr->sin_addr.s_addr = htonl(INADDR_ANY);
				len = sizeof(*ptr);
			}
		}
		break;

		case AF_INET6:
		{
			struct sockaddr_in6 *ptr = (SA6)naddr;

			if (nlen < sizeof(*ptr)) {
				errno = ENOSPC;
			} else {
				ptr->sin6_family = AF_INET6;
				ptr->sin6_port = htons(port);
				ptr->sin6_addr = in6addr_any;
				len = sizeof(*ptr);
			}
		}
		break;

		default:

			errno = EAFNOSUPPORT;
			break;
	}

	if ((len > 0) && addr) {
		len = SA_pton(addr, naddr, nlen);
	}

	if (unlikely(len == 0)) {
		RAISE(EXCEPT_SYS);
	}

	return len;
}

bool SA_CompareAddr(struct sockaddr *a, struct sockaddr *b)
{
	assert(a && b);

	if (unlikely(a->sa_family != b->sa_family)) {
		return false;
	}

	switch (a->sa_family)
	{
		case AF_INET:
		{
			struct sockaddr_in      *addr1 = (SA4)a;
			struct sockaddr_in      *addr2 = (SA4)b;

			if (addr1->sin_addr.s_addr == addr2->sin_addr.s_addr) {
				return true;
			}
		}
		break;

		case AF_INET6:
		{
			struct sockaddr_in6     *addr1 = (SA6)a;
			struct sockaddr_in6     *addr2 = (SA6)a;

			if (bcmp(&addr2->sin6_addr, &addr1->sin6_addr, sizeof(addr2->sin6_addr)) == 0) {
				return true;
			}
		}
		break;

		default:
			RAISE_SYS_ERROR_ERRNO(EAFNOSUPPORT);
			break;
	}

	return true;
}

ssize_t SIO_ReadPeek(int fd, char *buff, size_t len)
{
	ssize_t         bytes = -1;
	struct msghdr   hdr;
	struct iovec    vec;

	assert(buff);

	bzero(&hdr, sizeof(hdr));
	hdr.msg_iov = &vec;
	hdr.msg_iovlen = 1;
	vec.iov_base = buff;
	vec.iov_len = len;

#ifdef SIG_RESTART_SYSCALL
again:
	bytes = recvmsg(fd, &hdr, MSG_PEEK | MSG_DONTWAIT);

	if (unlikely((bytes < 0) && (errno == EINTR))) {
		goto again;
	}

#else
	bytes = recvmsg(fd, &hdr, MSG_PEEK | MSG_DONTWAIT);
#endif

	if (bytes != (ssize_t)len) {
		x_printf(W, "Attempt to read data failed : %s.",
			bytes < 0 ? x_strerror(errno) :
			"Connection closed by foreign host "
			"or try to read zero bytes");
	}

	return bytes;
}

