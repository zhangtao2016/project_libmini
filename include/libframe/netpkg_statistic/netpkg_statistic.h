//
//  netpkg_statistic.h
//  supex
//
//  Created by 周凯 on 16/1/5.
//  Copyright © 2016年 zhoukai. All rights reserved.
//

#ifndef netpkg_statistic_h
#define netpkg_statistic_h

#include "../utils.h"

__BEGIN_DECLS

/**
 * 网络包统计初始化
 * @param hosts统计的主机数量，小于1时使用默认值
 */
bool netpkg_stat_init(int hosts);

/**
 * 增加新的统计连接描述符
 * @param sockfd 关联的连接描述符
 */
void netpkg_stat_new(int sockfd);

/**
 * 为描述符关联的网络包增加一次计数
 * @param sockfd 关联的连接描述符
 */
void netpkg_stat_increase(int sockfd);

/**
 * 移除统计
 */
void netpkg_stat_remove(int sockfd);

/**
 * 打印统计信息到文件描述符，或文件中
 * @param desfd输出描述符，如果小于0，被忽略；如果file参数不为null，则输出到文件
 * @param file输出文件，在desfd参数忽略的情况下有效
 */
void netpkg_stat_print(int desfd, const char *file);

__END_DECLS

#endif /* netpkg_statistic_h */
