//
//  socket_opt.h
//
//
//  Created by 周凯 on 15/9/6.
//
//

#ifndef __libmini__socket_opt__
#define __libmini__socket_opt__

#include "socket_base.h"

__BEGIN_DECLS

/* -------------        */

/**
 * 获取整型套接字选项
 */
void SO_GetIntOption(int fd, int level, int name, int *val);

/**
 * 设置整型套接字选项
 */
void SO_SetIntOption(int fd, int level, int name, int val);

/**
 * 获取时间型套接字选项
 * 微秒单位
 */
void SO_GetTimeOption(int fd, int level, int name, long *usec);

/**
 * 设置时间型套接字选项
 * 微秒单位
 */
void SO_SetTimeOption(int fd, int level, int name, long usec);

/**
 * 关闭套接字属性
 * @param onoff     是否开启参数：TRUE，开启；FALSE，关闭
 * @param linger    延迟时间参数：
 *   0，发送RST分节给对端，本端无四路终止序列，
 *   即本端端口不会被TIME_WAIT状态保护，可能会被连续使用给
 *   下一个新连接，使下一个连接收到上个连接的旧分节（网络在
 *   途分节）。
 *   > 0（为延迟时间），进程close()套接字时被挂
 *   起（除非套接字被设置为非阻塞模式），直到套接字发送缓冲
 *   区的数据全被对端确认接受，或延迟时间到达（此时内核未将
 *   缓冲区数据没有发送完毕，则close()返回EWOULDBLOCK错
 *   误，且缓冲区数据被丢弃）
 *   单位 10ms ?
 */
void SO_SetCloseLinger(int fd, bool onoff, int linger);

/* -------------        */

/*
 * 设置套接字的接收低水平位套接字选项
 * 超过此值，select()被读被唤醒
 * 默认为1
 */
#define SO_SetRecvLowat(fd, val) \
	(SO_SetIntOption((fd), SOL_SOCKET, SO_RCVLOWAT, (val)))

/*
 * 获取套接字的接收低水平位套接字选项
 */
#define SO_GetRecvLowat(fd) \
	({ int _val = 0; SO_GetIntOption((fd), SOL_SOCKET, SO_RCVLOWAT, &_val); _val; })

/* -------------        */

/*
 * 设置套接字的发送低水平位套接字选项
 * 超过此值，select()被写被唤醒
 */
#define SO_SetSendLowat(fd, val) \
	(SO_SetIntOption((fd), SOL_SOCKET, SO_SNDLOWAT, (val)))

/*
 * 获取套接字的发送低水平位套接字选项
 */
#define SO_GetSendLowat(fd) \
	({ int _val = 0; SO_GetIntOption((fd), SOL_SOCKET, SO_SNDLOWAT, &_val); _val; })

/* -------------        */

/*
 * 设置套接字的接收缓冲套接字选项
 */
#define SO_SetRecvBuff(fd, val)	\
	(SO_SetIntOption((fd), SOL_SOCKET, SO_RCVBUF, (val)))

/*
 * 获取套接字的接收缓冲套接字选项
 */
#define SO_GetRecvBuff(fd) \
	({ int _val = 0; SO_GetIntOption((fd), SOL_SOCKET, SO_RCVBUF, &_val); _val; })

/* -------------        */

/*
 * 设置套接字的发送缓冲套接字选项
 */
#define SO_SetSendBuff(fd, val)	\
	(SO_SetIntOption((fd), SOL_SOCKET, SO_SNDBUF, (val)))

/*
 * 获取套接字的发送缓冲套接字选项
 */
#define SO_GetSendBuff(fd) \
	({ int _val = 0; SO_GetIntOption((fd), SOL_SOCKET, SO_SNDBUF, &_val); _val; })

/* -------------        */

/*
 * 开启Nagle算法
 */
#define SO_EnableNoDelay(fd) \
	(SO_SetIntOption((fd), SOL_SOCKET, TCP_NODELAY, 1))

/*
 * 关闭Nagle算法
 */
#define SO_DisableNoDelay(fd) \
	({ int _val = 0; SO_GetIntOption((fd), SOL_SOCKET, TCP_NODELAY, &_val); _val; })

/* -------------        */

/*
 * 得到套接字上的错误值
 */
#define SO_GetSocketError(fd) \
	({ int _val = 0; SO_GetIntOption((fd), SOL_SOCKET, SO_ERROR, &_val); _val; })

/* -------------        */

/*
 * 重用绑定
 */
#define SO_ReuseAddress(fd) \
	(SO_SetIntOption((fd), SOL_SOCKET, SO_REUSEADDR, 1))

/*
 * 开启广播
 */
#define SO_EnableBroadCast(fd) \
	(SO_SetIntOption((fd), SOL_SOCKET, SO_BROADCAST, 1))

/*
 * 关闭广播
 */
#define SO_DisableBroadCast(fd)	\
	(SO_SetIntOption((fd), SOL_SOCKET, SO_BROADCAST, 0))

/* -------------        */

/*
 * 通过套接字描述符获取套接字地址族类型
 */
#define SO_GetSocketType(fd) \
	({ int _val = 0; SO_GetIntOption((fd), SOL_SOCKET, SO_TYPE, &_val); _val; })

/* -------------        */

#if defined(IP_RECVDSTADDR)

/*
 * 开启接收目的地址
 */
  #define SO_EnableRecvDstAddr(fd) \
	(SO_SetIntOption((fd), IPPROTO_IP, IP_RECVDSTADDR, 1))

/*
 * 关闭接收目的地址
 */
  #define SO_DisableRecvDstAddr(fd) \
	(SO_SetIntOption((fd), IPPROTO_IP, IP_RECVDSTADDR, 0))

#else

  #define SO_EnableRecvDstAddr(fd) \
	((void)0)

  #define SO_DisableRecvDstAddr(fd) \
	((void)0)
#endif

/* -------------        */

#if defined(IP_RECVIF)

/*
 * 开启接收接收接口
 */
  #define SO_EnableRecvIF(fd) \
	(SO_SetIntOption((fd), IPPROTO_IP, IP_RECVIF, 1))

/*
 * 关闭接收接收接口
 */
  #define SO_DisableRecvIF(fd) \
	(SO_SetIntOption((fd), IPPROTO_IP, IP_RECVIF, 0))
#else

  #define SO_EnableRecvIF(fd) \
	((void)0)

  #define SO_DisableRecvIF(fd) \
	((void)0)
#endif
/* -------------        */

/*设置发送超时 毫秒单位*/
#define SO_SetSndTimeout(fd, x)	\
	SO_SetTimeOption((fd), SOL_SOCKET, SO_SNDTIMEO, (x) * 1000)

/*设置接收超时 毫秒单位*/
#define SO_SetRcvTimeout(fd, x)	\
	SO_SetTimeOption((fd), SOL_SOCKET, SO_RCVTIMEO, (x) * 1000)

/*获取发送超时 毫秒单位*/
#define SO_GetSndTimeout(fd) \
	({ long _val = 0; SO_GetTimeOption((fd), SOL_SOCKET, SO_SNDTIMEO, &_val); _val / 1000; })

/*获取接收超时 毫秒单位*/
#define SO_GetRcvTimeout(fd) \
	({ long _val = 0; SO_GetTimeOption((fd), SOL_SOCKET, SO_RCVTIMEO, &_val); _val / 1000; })

/* -------------        */

/*
 * @param idle 在套接字空闲时起，首次探测的时间，单位：秒，如果小于1，则默认为2个小时
 * @param intervel 首次探测后，每次探测的间隔时间，单位：秒，如果小于1，则默认为75秒
 * @param trys 尝试探测的次数，如果小于1，则默认为9次
 * 仅linux支持修改首次探测时间参数
 */
void SO_EnableKeepAlive(int fd, int idle, int intervel, int trys);

/* -------------        */
__END_DECLS
#endif	/* defined(__libmini__socket_opt__) */

