//
//  ipc_base.h
//  minilib
//
//  Created by 周凯 on 15/8/25.
//  Copyright (c) 2015年 zk. All rights reserved.
//

#ifndef __minilib__ipc_base__
#define __minilib__ipc_base__

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>

#include "../utils.h"

__BEGIN_DECLS

/*
 * System V 权限位
 */
#ifndef IPC_R
  #define IPC_R 0400
  #define IPC_W 0200
#endif

#if 0
  #define     SVMSG_MODE        (IPC_R | IPC_W | (IPC_R >> 3) | (IPC_R >> 6))
  #define     SVSEM_MODE        (SEM_R | SEM_W | (SEM_R >> 3) | (SEM_R >> 6))
  #define     SVSHM_MODE        (SHM_R | SHM_W | (SHM_R >> 3) | (SHM_R >> 6))
#endif

#define SVIPC_MODE              (IPC_R | IPC_W | (IPC_R >> 3) | (IPC_R >> 6))
#define PSIPC_MODE              (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)
#define FILE_MODE               (PSIPC_MODE)

/*
 * IPC前缀的环境变量名
 */
#define PX_IPC_ENV_PRENAME      ("PX_IPC_PRENAME")

/*
 * 默认IPC前缀名
 */
#ifdef __LINUX__
  #define PX_IPC_PRENAME        ("/")
#else
  #define PX_IPC_PRENAME        ("/tmp/")
#endif

char *PosixIPCName(char *buff, size_t size, const char *ipcname);

__END_DECLS
#endif	/* defined(__minilib__ipc_base__) */

