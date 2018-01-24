//
//  http_string.h
//  supex
//
//  Created by 周凯 on 15/12/25.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#ifndef http_string_h
#define http_string_h

#include "../../utils.h"
#include "../../cache/cache.h"
__BEGIN_DECLS

/**
 * 根据指定http协议码组装固定的http协议响应字符串
 */
int http_string_resp(struct cache *cache, int code, bool keepalive);

__END_DECLS
#endif	/* http_string_h */

