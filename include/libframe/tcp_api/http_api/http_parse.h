//
//  http_parse.h
//  supex
//
//  Created by 周凯 on 15/12/25.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#ifndef http_parse_h
#define http_parse_h

#include "../../../http-parser/http_parser.h"
#include "../../utils.h"

__BEGIN_DECLS
#ifndef HTTP_MAX_HEADERS
#define HTTP_MAX_HEADERS 10
#endif
struct http_presult
{
	/*public:only read*/
	bool                    over;		/**< http部分的数据是否完成解析*/
	bool					upgrade;	/**< http是否进行了协议转换*/
	enum http_errno         err;		/**< 分析错误码*/
	enum http_method        method;		/**< http方法*/
	unsigned short          http_major;	/**< http主版本号*/
	unsigned short          http_minor;	/**< http次版本号*/
	bool                    keep_alive;	/**< 是否保持连接*/
	int                     status_code;	/**< 响应码*/
	unsigned				url_offset;	/**< url偏移*/
	unsigned				url_len;	/**< url长度*/
	unsigned				path_offset;	/**< 路径偏移*/
	unsigned          		path_len;	/**< 路径长度*/
	unsigned                uri_offset;	/**< uri偏移*/
	unsigned          		uri_len;	/**< uri长度*/
	unsigned				body_offset;	/**< body偏移*/
	unsigned				body_size;	/**< body长度*/
	unsigned				dosize;		/**< 已分析的长度*/
	char *const             *data;		/**< 被解析数据起始地址指针*/
	unsigned const          *size;		/**< 当前数据的总长度地址*/
	unsigned				step;		/**< 当前试图解析的次数*/
#if HTTP_MAX_HEADERS > 0
	enum
	{
		HEADER_NONE = 0,
		HEADER_FIELD,
		HEADER_VALUE,
	}                       header_fvalue_stat;	/**< 头部域值分析状态*/
	struct
	{
		struct
		{
			unsigned      	offset;	/**< 域值字符串偏移*/
			unsigned		lenght;	/**< 域值字符串长度*/
		} field, /**< 域*/ value;	/**< 值*/
	}                       header_fvalue[HTTP_MAX_HEADERS];
	int                     header_fvalues;
#endif	/* if MAX_HEADERS > 0 */
};

struct http_parse_t
{
	http_parser             hp;	/**< HTTP数据分析器*/
	struct http_presult      hs;	/**< HTTP数据分析器状态*/
};

/**
 * @param info解析句柄
 * @param buff待解析的字符串地址指针
 * @param size待解析的字符串长度指针
 */
void http_parse_init(struct http_parse_t *info, char *const *buff, unsigned const *size);

/**
 * 解析响应http字符串
 * @return false 还未结束，需要后续数据；ture 已经结束，根据内部状态判断成功与否
 */
bool http_parse_response(struct http_parse_t *info);

/**
 * 解析请求http字符串
 * @return false 还未结束，需要后续数据；ture 已经结束，根据内部状态判断成功与否
 */
bool http_parse_request(struct http_parse_t *info);

/**
 * 解析请求或响应http字符串
 * @return false 还未结束，需要后续数据；ture 已经结束，根据内部状态判断成功与否
 */
bool http_parse_both(struct http_parse_t *info);

__END_DECLS
#endif	/* http_parse_h */

