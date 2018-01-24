//
//  libmini.h
//  minilib
//
//  Created by 周凯 on 15/8/21.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __minilib_libmini_h__
#define __minilib_libmini_h__

/* -------------                                          */

#include "utils.h"

/* ----------------- common             ----------------- */

#include "atomic/atomic.h"
#include "slog/slog.h"
#include "string/string.h"
#include "time/time.h"

/* ----------------- filesystem         ----------------- */
#include "filesystem/filesystem.h"
#include "filesystem/filelock.h"

/* ----------------- io                 ----------------- */

#include "io/io_base.h"

/* ----------------- ipc                ----------------- */

#include "ipc/futex.h"
#include "ipc/pipe.h"
#include "ipc/fork.h"
#include "ipc/ipc_base.h"
#include "ipc/signal.h"

/* ----------------- thread             ----------------- */

#include "thread/thread.h"

/* ----------------- exception          ----------------- */

#include "except/except.h"
#include "except/exception.h"

/* ----------------- ADT                ----------------- */
#include "adt/adt_proto.h"
#include "adt/dynamic/dqueue.h"
#include "adt/dynamic/dstack.h"
#include "adt/dynamic/dhash.h"
#include "adt/static/squeue.h"
#include "adt/static/slist.h"
#include "adt/static/shmqueue.h"
#include "adt/static/filequeue.h"
#include "adt/static/memqueue.h"

/* ----------------- socket             ----------------- */
#include "socket/socket_base.h"
#include "socket/socket_opt.h"
#include "socket/socket.h"

/* -------------                                          */
#endif	/* ifndef __minilib_libmini_h__ */

