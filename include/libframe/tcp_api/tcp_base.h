//
//  tcp_base.h
//  supex
//
//  Created by 周凯 on 15/12/26.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#ifndef tcp_base_h
#define tcp_base_h

#include "../utils.h"
#include "../cache/cache.h"

__BEGIN_DECLS

/*最大io单元*/
#ifndef TCP_MIOU
  #define TCP_MIOU 1024
#endif

#if TCP_MIOU < 1
  #error "ERROR PARAMRTER `TCP_MIOU`"
#endif

/* ------------------				*/

/*
 * 默认为非阻塞的状态创建
 */
struct tcp_socket
{
	/*private: only read*/
	int             magic;
	enum
	{
		TCP_TYPE_LISTEN,
		TCP_TYPE_CONN
	}               type;
	int             fd;			/**<描述符*/
	uint16_t        fport;			/**<远端ip端口*/
	uint16_t        lport;			/**<本端ip端口*/
	char            faddr[IPADDR_MAXSIZE];	/**<远端ip地址*/
	char            laddr[IPADDR_MAXSIZE];	/**<本端ip地址*/
	/*public: write and read*/
	struct cache    cache;			/**<io缓冲*/
	ssize_t         miou;			/**<最大io单元*/
	long            timeout;		/**<输入超时，<0时阻塞，=0时仅操作单次，>0时毫秒单位超时，输出流失时间*/
	void            *usr;			/**<用户层数据*/
};

/* ------------------				*/

/**
 * 连接远端
 * @param ipaddr 主机地址，可以为点分ip地址，也可以为域名地址字符串
 * @param port 主机端口，可以是数字端口，也可以为服务命名字符串
 * @param timeout 连接超时时间，> 0时有效
 */
struct tcp_socket       *tcp_connect(const char *ipaddr, const char *port, int timeout);

/**
 * 侦听本地
 * @param ipaddr 主机地址，只能为点分ip地址
 * @param port 主机端口，只能为数字端口
 */
struct tcp_socket       *tcp_listen(const char *ipaddr, const char *port);

/**
 * 通过本地绑定套接字接受客户端链接请求，返回客户端的tcp_socket结构
 * @param server 侦听tcp，当server->timeout =0 时，尝试接受一次，>0 时超时接受，<0 时阻塞接受
 */
struct tcp_socket       *tcp_accept(struct tcp_socket *server);

/**
 * 断开链接或解除绑定
 */
static inline void tcp_destroy(struct tcp_socket *tcp)
{
	return_if_fail(UNREFOBJ(tcp));
	cache_finally(&tcp->cache);
	close(tcp->fd);
	Free(tcp);
}

/* ------------------				*/
/**以fd初始化tcp对象*/
bool tcp_init(struct tcp_socket *tcp, int fd, bool isclient);
/**释放tcp对象内部占用的数据内存，但不关闭描述符*/
void tcp_finally(struct tcp_socket *tcp);
/* ------------------				*/
__END_DECLS
#endif	/* tcp_base_h */

