//
//  redis_parse.c
//  supex
//
//  Created by 周凯 on 16/1/14.
//  Copyright © 2016年 zhoukai. All rights reserved.
//
#include "tcp_api/redis_api/redis_parse.h"

static int message_begin_cb(redis_parser *parser, int64_t value);

static int content_len_cb(redis_parser *parser, int64_t len);

static int content_cb(redis_parser *parser, const char *data, size_t len);

static int message_complete_cb(redis_parser *parser, int64_t fields);

static bool _redis_parse_handle(redis_parser *parser, redis_parser_settings *settings);

static redis_parser_settings _parse_settings = {
	.on_message_begin = message_begin_cb,
	.on_content_len = content_len_cb,
	.on_content = content_cb,
	.on_message_complete = message_complete_cb,
};

void redis_parse_init(struct redis_parse_t *info, char *const *data, unsigned const *size)
{
	assert(info);
	
	char *const     *p_data = (data != NULL) ? data : info->rs.data;
	unsigned const    *p_size = (size != NULL) ? size : info->rs.size;
	
	memset(info, 0, sizeof(*info));
	
	info->rp.data = &info->rs;
	info->rs.data = p_data;
	info->rs.size = p_size;
	info->rs.over = false;
}


bool redis_parse_response(struct redis_parse_t *info)
{
	assert(info);
	if (unlikely(info->rs.dosize == 0 || info->rs.over == false)) {
		redis_parse_init(info, NULL, NULL);
		redis_parser_init(&info->rp, REDIS_REPLY);
	}
	return _redis_parse_handle(&info->rp, &_parse_settings);
}

bool redis_parse_request(struct redis_parse_t *info)
{
	assert(info);
	if (unlikely(info->rs.dosize == 0 || info->rs.over == false)) {
		redis_parse_init(info, NULL, NULL);
		redis_parser_init(&info->rp, REDIS_REQUEST);
	}
	return _redis_parse_handle(&info->rp, &_parse_settings);
}

static int message_begin_cb(redis_parser *parser, int64_t value)
{
	struct redis_presult *stat = parser->data;
	assert(stat && stat->data && stat->size && *stat->size);
	
	stat->over = false;
	memset(stat->field, 0, sizeof(stat->field));
	/*
	 * 保存总字段数
	 * 当为－1或0时不会回调content_len_cb()和content_cb()
	 * 如果当前为数字／成功消息／错误消息则不会掉用content_len_cb()
	 */
	stat->fields = parser->fields;
	stat->command_type = parser->command_type;
	stat->reply_type = parser->reply_type;
	stat->type = parser->type;
	
	return 0;
}

static int content_len_cb(redis_parser *parser, int64_t len)
{
	struct redis_presult *stat = parser->data;
	return_val_if_fail(len <= INT32_MAX && len >= INT32_MIN, -1);
	
	/*根据分析器的剩余处理字段，计算下标*/
	int pos = stat->fields - parser->fields;
	return_val_if_fail(pos < DIM(stat->field), -1);
	stat->field[pos].len = (int)len;
	return 0;
}

static int content_cb(redis_parser *parser, const char *data, size_t len)
{
	struct redis_presult *stat = parser->data;
	return_val_if_fail(len <= UINT32_MAX, -1);
	/*根据分析器的剩余处理字段，计算下标*/
	int pos = stat->fields - parser->fields;
	return_val_if_fail(pos < DIM(stat->field), -1);

	if (unlikely(stat->field[pos].offset == 0)) {
		assert(stat->data && *stat->data);
		stat->field[pos].offset = (unsigned)(data - *stat->data);
	}
	
	return 0;
}

static int message_complete_cb(redis_parser *parser, int64_t fields)
{
	struct redis_presult *stat = parser->data;
	stat->over = true;
	return 0;
}

static bool _redis_parse_handle(redis_parser *parser, redis_parser_settings *settings)
{
	struct redis_presult *stat = parser->data;
	assert(stat && stat->data && stat->size && *stat->size);
	
	/*计算本次需要分析的数据起始位置和长度*/
	const char *data = *stat->data + stat->dosize;
	size_t size = *stat->size - stat->dosize;
	bool ret = false;
	
	unsigned done = (unsigned)redis_parser_execute(parser, settings, data, size);
	stat->step++;
	if (likely(stat->over && parser->redis_errno == 0)) {
		stat->dosize = 0;
		ret = true;
	} else {
		stat->dosize += done;
		if (unlikely(parser->redis_errno != 0)) {
			x_printf(E, "\n*** server expected %s, but saw %s ***\n%.*s",
					 redis_errno_name(0),
					 redis_errno_name(parser->redis_errno),
					 MAX((int)(size - done), 128),
					 (*stat->data + stat->dosize));
			ret = true;
			stat->dosize = 0;
			stat->step = 0;
			stat->error = parser->redis_errno;
		}
	}

	return ret;
}


