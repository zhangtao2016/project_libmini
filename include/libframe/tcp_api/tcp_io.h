//
//  tcp_io.h
//  supex
//
//  Created by 周凯 on 15/12/24.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#ifndef tcp_io_h
#define tcp_io_h

#include "tcp_base.h"

__BEGIN_DECLS

/* --------------------------					*\
 * 如果回调函数失败返回，则视为出错，如果当前errno为0， *
 * 则函数自动设置为ETIMEOUT或EPROTO
\* --------------------------					*/
/*空闲（在描述符没有可读数据或发送空间时）回调，返回false失败，中断io；true成功，继续io*/
typedef bool (*tcp_idle_cb)(struct tcp_socket *tcp, void *usr);
/*接收分析数据回调，返回0时继续接收，<0时出错，>0时停止接收数据*/
typedef int (*tcp_parse_cb)(struct tcp_socket *tcp, void *usr);
/* --------------------------					*/

/**
 * 从连接类型的套接字读入数据存入自身的缓存中
 */
ssize_t tcp_read(struct tcp_socket *tcp);

/**
 * 将自身缓存的数据写出连接类型的套接字中
 * @return =0，缓冲中没有数据
 */
ssize_t tcp_write(struct tcp_socket *tcp);

/* --------------------------					*/

/**
 *发送所有的数据或接收特定的数据
 *为接收或发送完整的数据都将返回-1，此时已发生或接收的数据调用者自己从cache中查看
 *@param parse在接收到数据时调用
 *@param idle在描述符资源不可用时调用
 *@return =0缓冲没有数据或对端断开；<0 失败，设置errno；>0 本次发送接收的长度
 */
ssize_t tcp_send(struct tcp_socket *tcp, tcp_idle_cb idle, void *usr);

ssize_t tcp_recv(struct tcp_socket *tcp, tcp_parse_cb parse, tcp_idle_cb idle, void *usr);

/* --------------------------					*/
__END_DECLS
#endif	/* tcp_io_h */

