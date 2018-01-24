//
//  socket_base.h
//
//
//  Created by 周凯 on 15/9/6.
//
//

#ifndef __libmini__socket_base__
#define __libmini__socket_base__

#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include "../utils.h"

__BEGIN_DECLS

/*
 * 套接字相关结构体重定义
 */
#define SA4     struct sockaddr_in *
#define SA6     struct sockaddr_in6 *
#define SAU     struct sockaddr_un *
#define SA      struct sockaddr *
#define SAS     struct sockaddr_storage *

#if defined(HAVA_SOCKADDR_DL_STRUCT)
  #include <net/if_dl.h>
  #define SDL struct sockaddr_dl *
#endif

#ifndef IPADDR_MAXSIZE
  #define IPADDR_MAXSIZE INET6_ADDRSTRLEN
#endif

#ifndef IPADDR_MINSIZE
  #define IPADDR_MINSIZE INET_ADDRSTRLEN
#endif

/* -------------------------           */

/**
 * 获取套接字地址信息
 * @param host 主机信息 可以为域名或IP地址，可以为空，即通用IP地址
 * @param serv 服务信息 可以为数字端口号，比如“8080“，亦可以为字符串，比如“http“
 * @param flag 标志 为 0 主动发起连接端信息，
 *             或 AI_PASSIVE 被动发起连接端信息（需要绑定的套接字）
 * @param family 地址族 为 AF_UNSPEC 不指定，返回所有地址族的地址信息
 *            或为 AF_INET、AF_INET6 分别为IPV4、IPV6的地址信息
 * @param socktype 套接字类型 SOCK_STREAM、SOCK_DGRAM
 */
struct addrinfo *SA_GetAddrInfo(const char *host, const char *serv,
	int flag, int family, int socktype);

void SA_FreeAddrInfo(struct addrinfo **addrinfo);

/*
 * 套接字数字地址与表达式地址互相转换
 * SA_pton()需要填写naddr里的地址族sa_family字段
 */
char *SA_ntop(const struct sockaddr *naddr, char *paddr, size_t plen);

socklen_t SA_pton(const char *paddr, struct sockaddr *naddr, socklen_t nlen);

/*
 * 获取套接字数字地址的地址族：AF_INET、AF_INET6、AF_UNIX
 */
#define     SA_GetFamily(sa) \
	(assert(sa), ((const SA)(sa))->sa_family)

/*
 * 得到协议族级别
 */
int SA_GetAFLevel(const struct sockaddr *naddr);

int SF_GetAFLevel(int fd);

/*
 * 获取套接字数字地址的端口号和长度
 */
uint16_t SA_GetPort(const struct sockaddr *naddr);

socklen_t SA_GetLength(const struct sockaddr *naddr);

/* -------------------------           */

/*
 * 得到本端地址信息
 * 从连接的UDP套接字中只能获取端口号
 */
#define SF_GetLocalAddr(fd, naddr, nlen)	  \
	({ struct sockaddr *_ptr = (SA)(naddr);	  \
	   socklen_t _nlen = (socklen_t)(nlen);	  \
	   int _rc = -1;			  \
	   assert(_ptr && _nlen);		  \
	   _rc = getsockname((fd), _ptr, &_nlen); \
	   _rc; })

/*
 * 得到远端地址信息
 * 不能获取UDP套接字的任何信息
 */
#define SF_GetRemoteAddr(fd, naddr, nlen)	  \
	({ struct sockaddr *_ptr = (SA)(naddr);	  \
	   socklen_t _nlen = (socklen_t)(nlen);	  \
	   int _rc = -1;			  \
	   assert(_ptr && _nlen);		  \
	   _rc = getpeername((fd), _ptr, &_nlen); \
	   _rc; })

/*
 * 获取套接字描述符的类型：流、数据报
 */
#define     SF_IsStream(fd) \
	(SO_GetSocketType((fd)) == SOCK_STREAM ? true : false)

#define     SF_IsDatagram(fd) \
	(SO_GetSocketType((fd)) == SOCK_DGRAM ? true : false)

#define     SF_GetFamily(fd)				 \
	({ struct sockaddr_storage _sa = {};		 \
	   SF_GetLocalAddr((fd), (SA)&_sa, sizeof(_sa)); \
	   _sa.ss_family; })

/* -------------------------           */

/*
 * 窥视套接字缓冲区的数据
 * 返回值=0，则套接字已经被断开，
 * 返回值<0，且errno＝EAGAIN，则表示没有数据达到，否则套接字已发生错误，需要关闭重连。
 */
ssize_t SIO_ReadPeek(int fd, char *buff, size_t len);

#define SF_IsClosed(fd)							\
	({								\
		bool _ret = false;					\
		char _b = '\0';						\
		ssize_t _r = 0;						\
		_r = SIO_ReadPeek((fd), &_b, sizeof(_b));		\
		if (unlikely(_r == 0 || (_r < 0 && errno != EAGAIN))) {	\
			_ret = true;					\
		}							\
		_ret;							\
	})

/* -------------------------           */

/*
 * 其他辅助函数
 */
socklen_t SA_FillSocket(int family, const char *addr, uint16_t port,
	struct sockaddr *naddr, socklen_t nlen);

/* -------------------------           */

/*
 * 比较地址族和套接字地址
 */
bool SA_CompareAddr(struct sockaddr *a, struct sockaddr *b);

/* -------------------------           */

__END_DECLS
#endif	/* defined(__libmini__socket_base__) */

