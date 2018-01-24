//
//  signal.h
//
//
//  Created by 周凯 on 15/8/28.
//
//

#ifndef __minilib__signal__
#define __minilib__signal__

#include <signal.h>
#include "../utils.h"

__BEGIN_DECLS

/* ------------------------                   */

/**
 * 普通信号回调
 */
typedef void (*SignalCB)(int signo);

/**
 * 锁定线程的所有信号，如果sigset不为null则将旧信号掩码存到该值
 */
void SignalMaskBlockAll(sigset_t *sigset);

/**
 * 解锁线程的所有信号，如果sigset不为null则将信号掩码设置为该值
 */
void SignalMaskUnblockAll(sigset_t *sigset);

/**
 * 锁定线程的指定信号，如果sigset不为null则将旧信号掩码存到该值
 */
void SignalMaskBlockOne(int signo, sigset_t *sigset);

/**
 * 解锁线程的指定信号
 */
void SignalMaskUnblockOne(int signo);

/**
 * 安装信号，尽可能重启慢速调用
 */
SignalCB Signal(int signo, SignalCB cb);

/**
 * 安装信号，尽可能不重启慢速调用
 */
SignalCB SignalIntr(int signo, SignalCB cb);

/**
 * 无任何动作信号处理函数
 */
void SignalNormalCB(int signo);

/* ------------------------                   */

__END_DECLS
#endif	/* defined(__minilib__signal__) */

