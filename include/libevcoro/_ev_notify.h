//
//  _ev_notify.h
//  libmini
//
//  Created by 周凯 on 15/12/5.
//  Copyright © 2015年 zk. All rights reserved.
//

#ifndef _ev_notify_h
#define _ev_notify_h

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "_evcoro_common.h"

#if __cplusplus
extern "C" {
#endif

struct ev_coro;
struct ev_notify
{
	struct ev_notify        *prev;
	struct ev_notify        *next;
	struct ev_coro          *coro;
	int                     *addr;
	int                     old;
};

static inline struct ev_notify *_ev_notify_create(int *addr, struct ev_coro *coro)
{
	struct ev_notify *notify = calloc(1, sizeof(*notify));

	if (unlikely(!notify)) {
		return false;
	}

	notify->addr = addr;
	notify->old = *addr;
	notify->coro = coro;

	return notify;
}

#define _ev_notify_destroy(notify) (free(notify))

int _ev_notify_wait(struct ev_notify *notify);

int _ev_notify_wake(int *uaddr, int n);

#if __cplusplus
}
#endif
#endif	/* _ev_notify_h */

