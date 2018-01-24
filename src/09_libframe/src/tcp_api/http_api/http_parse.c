//
//  http_parse.c
//  supex
//
//  Created by 周凯 on 15/12/25.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#include "tcp_api/http_api/http_parse.h"

static int message_begin_cb(http_parser *hp)
{
	struct http_presult      *stat = hp->data;
	char *const             *data = NULL;
	unsigned const            *size = NULL;

	assert(stat && stat->data && stat->size && *stat->size);
	data = stat->data;
	size = stat->size;
	// -->On request,life only enter once.
	memset(stat, 0, sizeof(*stat));
	stat->data = data;
	stat->size = size;
	stat->over = false;
	stat->upgrade = false;
	return 0;
}

int status_complete_cb(http_parser *hp, const char *at, size_t len)
{
	struct http_presult *stat = hp->data;

	stat->status_code = hp->status_code;
	return 0;
}

static int headers_complete_cb(http_parser *hp)
{
	struct http_presult *p_stat = hp->data;

	p_stat->upgrade = !!hp->upgrade;
	p_stat->method = hp->method;
	p_stat->http_major = hp->http_major;
	p_stat->http_minor = hp->http_minor;
	p_stat->keep_alive = http_should_keep_alive(hp) == 0 ? false : true;

	p_stat->path_offset = p_stat->url_offset;
	const char *p_mark = x_strstr((*p_stat->data + p_stat->url_offset), "?", p_stat->url_len);

	if (p_mark) {
		p_stat->path_len = (unsigned)(p_mark - (*p_stat->data + p_stat->path_offset));
		p_stat->uri_offset = (unsigned)(p_mark + 1 - *p_stat->data);
		p_stat->uri_len = p_stat->url_len - p_stat->path_len - 1;
	} else {
		p_stat->path_len = p_stat->url_len;
		p_stat->uri_offset = 0;
		p_stat->uri_len = 0;
	}

	/*不包含body数据*/
	if ((hp->content_length == ULLONG_MAX) || (hp->content_length == 0)) {
		p_stat->over = true;
	}

	return 0;
}

static int message_complete_cb(http_parser *hp)
{
	struct http_presult *p_stat = hp->data;

	p_stat->over = true;
	return 0;
}

#if HTTP_MAX_HEADERS > 0

static int header_field_cb(http_parser *hp, const char *at, size_t length)
{
	struct http_presult *stat = hp->data;

	return_val_if_fail(stat->header_fvalues < DIM(stat->header_fvalue), 0);

	if (likely(stat->header_fvalue_stat == HEADER_VALUE)) {
		stat->header_fvalues++;
	}

	if (stat->header_fvalue[stat->header_fvalues].field.offset == 0) {
		stat->header_fvalue_stat = HEADER_FIELD;
		stat->header_fvalue[stat->header_fvalues].field.offset = (unsigned)(at - *stat->data);
	}
	stat->header_fvalue[stat->header_fvalues].field.lenght += (unsigned)length;
	
	return 0;
}

static int header_value_cb(http_parser *hp, const char *at, size_t length)
{
	struct http_presult *stat = hp->data;

	return_val_if_fail(stat->header_fvalues < DIM(stat->header_fvalue), 0);

	if (stat->header_fvalue[stat->header_fvalues].value.offset == 0) {
		stat->header_fvalue_stat = HEADER_VALUE;
		stat->header_fvalue[stat->header_fvalues].value.offset = (unsigned)(at - *stat->data);
	}
	stat->header_fvalue[stat->header_fvalues].value.lenght += (unsigned)length;
	
	return 0;
}
#endif	/* if MAX_HEADERS > 0 */

static int url_cb(http_parser *hp, const char *at, size_t length)
{
	struct http_presult *p_stat = hp->data;

	if (p_stat->url_offset == 0) {
		p_stat->url_offset = (unsigned)(at - *p_stat->data);
	}

	p_stat->url_len += (unsigned)length;
	return 0;
}

static int body_cb(http_parser *hp, const char *at, size_t length)
{
	struct http_presult *p_stat = hp->data;

	if (p_stat->body_offset == 0) {
		p_stat->body_offset = (unsigned)(at - *p_stat->data);
	}

	p_stat->body_size += (unsigned)length;
	return 0;
}

const static http_parser_settings _parse_request_settings = {
	.on_message_begin       = message_begin_cb
	, .on_status            = 0
	, .on_headers_complete  = headers_complete_cb
	, .on_message_complete  = message_complete_cb
#if HTTP_MAX_HEADERS > 0
	, .on_header_field      = header_field_cb
	, .on_header_value      = header_value_cb
#else
	, .on_header_field      = NULL
	, .on_header_value      = NULL
#endif
	, .on_url               = url_cb
	, .on_body              = body_cb
};

const static http_parser_settings _parse_response_settings = {
	.on_message_begin       = message_begin_cb
	, .on_status            = status_complete_cb
	, .on_headers_complete  = headers_complete_cb
	, .on_message_complete  = message_complete_cb
#if HTTP_MAX_HEADERS > 0
	, .on_header_field      = header_field_cb
	, .on_header_value      = header_value_cb
#else
	, .on_header_field      = NULL
	, .on_header_value      = NULL
#endif
	, .on_url               = url_cb
	, .on_body              = body_cb
};

static bool _http_parse_handle(http_parser *hp, const http_parser_settings *settings)
{
	struct http_presult *stat = NULL;

	assert(hp && settings);

	stat = hp->data;
	assert(stat && stat->data && *stat->data && stat->size);

	unsigned todo = *stat->size - stat->dosize;
	assert(todo >= 0);

	/*
	 * 传入分析器，分析器动作集合，分析的数据及数据的长度
	 * 返回分析的数据长度
	 */
	unsigned done = (unsigned)http_parser_execute(hp, settings, (*stat->data + stat->dosize), todo);
	/*增加分析步进*/
	stat->step++;

	if (likely(stat->over)) {
		stat->dosize = 0;
	} else {
		stat->dosize += done;
	}

	/*每次执行http_parser_execute()后检查该字段，如果为1则需要继续等待后续数据*/
	if (! hp->upgrade) {
		/*
		 * 如果本次解析的数据长度与给出数据长度一致，
		 * 则返回分析器状态的标志
		 * （可能已解析一次完整的请求、也可能需要接续后续数据）
		 */
		if (done == todo) {
			return stat->over;
		} else {
			/*分析长度不一致，则一定发生了错误，获取错误*/
			/*it's error request,change to over and shoud broken socket*/
			/* Handle error. Usually just close the connection.  处理错误,通常是关闭这个连接*/
			stat->err = HTTP_PARSER_ERRNO(hp);

			if (unlikely(HPE_OK != stat->err)) {
				x_printf(E, "\n*** server expected %s, but saw %s ***\n%.*s",
					http_errno_name(HPE_OK),
					http_errno_name(stat->err),
					MAX((int)(todo - done), 128),
					(*stat->data + stat->dosize));
			}

			/*出错时需要重新初始化*/
			stat->dosize = 0;
			return true;
		}
	}

	return false;
}

void http_parse_init(struct http_parse_t *info, char *const *buff, unsigned const *size)
{
	assert(info);

	char *const     *p_data = (buff != NULL) ? buff : info->hs.data;
	unsigned const    *p_size = (size != NULL) ? size : info->hs.size;

	memset(info, 0, sizeof(*info));

	info->hp.data = &info->hs;
	info->hs.data = p_data;
	info->hs.size = p_size;
	info->hs.step = 0;
	info->hs.status_code = 0;
}

bool http_parse_response(struct http_parse_t *parseinfo)
{
	assert(parseinfo);

	if ((parseinfo->hs.dosize == 0) || (parseinfo->hs.step == false)) {
		/*初始化，出错时需要重新初始化*/
		http_parse_init(parseinfo, NULL, NULL);
		http_parser_init(&parseinfo->hp, HTTP_RESPONSE);
	}

	return _http_parse_handle(&parseinfo->hp, &_parse_response_settings);
}

bool http_parse_request(struct http_parse_t *parseinfo)
{
	assert(parseinfo);

	if ((parseinfo->hs.dosize == 0) || (parseinfo->hs.step == false)) {
		/*初始化，出错时需要重新初始化*/
		http_parse_init(parseinfo, NULL, NULL);
		http_parser_init(&parseinfo->hp, HTTP_REQUEST);
	}

	return _http_parse_handle(&parseinfo->hp, &_parse_request_settings);
}

bool http_parse_both(struct http_parse_t *parseinfo)
{
	assert(parseinfo);

	if ((parseinfo->hs.dosize == 0) || (parseinfo->hs.step == 0)) {
		/*初始化，出错时需要重新初始化*/
		http_parse_init(parseinfo, NULL, NULL);
		http_parser_init(&parseinfo->hp, HTTP_BOTH);
	}

	return _http_parse_handle(&parseinfo->hp, &_parse_response_settings);
}

