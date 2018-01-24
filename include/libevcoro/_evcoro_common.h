//
//  _evcoro_common.h
//  libmini
//
//  Created by 周凯 on 15/12/5.
//  Copyright © 2015年 zk. All rights reserved.
//

#ifndef _evcoro_common_h
#define _evcoro_common_h

/* ------------------------------------------------------ */
#ifndef likely
  #if defined __GNUC__
    #define likely(x)   __builtin_expect(!!(x), 1)
    #define unlikely(x) __builtin_expect(!!(x), 0)
  #else
    #define likely(x)   (!!(x))
    #define unlikely(x) (!!(x))
  #endif
#endif

#if !defined(__LINUX__) && (defined(__linux__) || defined(__KERNEL__) \
	|| defined(_LINUX) || defined(LINUX) || defined(__linux))
  #define  __LINUX__    (1)
#elif !defined(__APPLE__) && defined(__MacOSX__)
  #define  __APPLE__    (1)
#elif !defined(__CYGWIN__) && (defined(__CYGWIN32__) || defined(CYGWIN))
  #define  __CYGWIN__   (1)
#endif
#endif	/* _evcoro_common_h */

