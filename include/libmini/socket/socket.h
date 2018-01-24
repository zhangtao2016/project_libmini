//
//  socket.h
//
//
//  Created by 周凯 on 15/9/7.
//
//

#ifndef __libmini__socket__
#define __libmini__socket__

#include "socket_base.h"

__BEGIN_DECLS

typedef void (*SetSocketCB)(int, void *);

int TcpListen(const char *host, const char *serv, SetSocketCB cb, void *usr);

int TcpConnect(const char *host, const char *serv, SetSocketCB cb, void *usr);

__END_DECLS
#endif	/* defined(__libmini__socket__) */

