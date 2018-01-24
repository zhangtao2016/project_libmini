//
//  netpkg_statistic.c
//  supex
//
//  Created by 周凯 on 16/1/5.
//  Copyright © 2016年 zhoukai. All rights reserved.
//

#include "netpkg_statistic/netpkg_statistic.h"

struct netpkg_stat
{
	int     magic;		/**<魔数*/
	SListT  pkgstat;	/**<网络包信息表*/
	DHashT  actconn;	/**<活跃连接表*/
};

struct pkginfo
{
	char            hostaddr[IPADDR_MAXSIZE];
	unsigned        counter;
	time_t          firstconn;
	time_t          laststat;
};

static int _pkgstat_find4new(const char *buff, int size, void *user, int usrsize);

static bool _pkgstat_info4print(const char *buff, int size, void *user);

static struct netpkg_stat g_netpkg_stat = {};

#ifndef NETPKG_STAT_HOSTS
  #define NETPKG_STAT_HOSTS 256
#endif

#if NETPKG_STAT_HOSTS < 1
  #error "ERROR PARAMRTER `NETPKG_STAT_HOSTS`"
#endif

#ifndef NETPKG_STAT_HASHSIZE
  #define NETPKG_STAT_HASHSIZE 111
#endif

#if NETPKG_STAT_HASHSIZE < 1
  #error "ERROR PARAMRTER `NETPKG_STAT_HASHSIZE`"
#endif

bool netpkg_stat_init(int hosts)
{
	return_val_if_fail(REFOBJ(&g_netpkg_stat), true);

	hosts = hosts < 1 ? NETPKG_STAT_HOSTS : hosts;
	SListT volatile pkgstat = NULL;
	DHashT volatile actconn = NULL;
	char *volatile  ptr = NULL;
	long            size = 0;
	bool            ret = false;

	size = SListCalculateSize(hosts, sizeof(struct pkginfo));

	TRY
	{
		NewArray(size, ptr);
		AssertError(ptr, ENOMEM);

		pkgstat = SListInit(ptr, size, sizeof(struct pkginfo), false);
		assert(pkgstat);

		actconn = DHashCreate(11, NULL, NULL);
		assert(actconn);

		g_netpkg_stat.pkgstat = pkgstat;
		g_netpkg_stat.actconn = actconn;

		ret = true;
	}
	CATCH
	{
		ret = false;
		Free(ptr);
		DHashDestroy((DHashT *)&actconn);
		UNREFOBJ(&g_netpkg_stat);
	}
	END;

	return ret;
}

void netpkg_stat_new(int sockfd)
{
	return_if_fail(sockfd > -1 && ISOBJ(&g_netpkg_stat));
	struct pkginfo          pkginfo = {};
	struct pkginfo          *pkginfoptr = NULL;
	struct sockaddr_storage saddr = {};
	socklen_t               slen = sizeof(saddr);
	int                     flag = 0;

	flag = getpeername(sockfd, (SA)&saddr, &slen);

	if (unlikely(flag < 0)) {
		x_perror("getpeername() error : %s.", x_strerror(errno));
		return;
	}

	SA_ntop((SA)&saddr, pkginfo.hostaddr, sizeof(pkginfo.hostaddr));

	SListLock(g_netpkg_stat.pkgstat);
	pkginfoptr = (struct pkginfo *)SListLookup(g_netpkg_stat.pkgstat,
			_pkgstat_find4new, &pkginfo, sizeof(pkginfo), NULL);

	if (likely(pkginfoptr)) {
		// update
	} else {
		// new add
		time(&pkginfo.firstconn);
		bool flag = false;
		flag = SListAddHead(g_netpkg_stat.pkgstat, (char *)&pkginfo,
				sizeof(pkginfo), NULL, (void **)&pkginfoptr);

		if (unlikely(!flag)) {
			x_perror("not enough space.");
		}
	}

	SListUnlock(g_netpkg_stat.pkgstat);

	if (likely(pkginfoptr)) {
		// update
		bool flag = false;
		flag = DHashPut(g_netpkg_stat.actconn, (char *)&sockfd, sizeof(sockfd),
				(char *)&pkginfoptr, sizeof(pkginfoptr));

		if (unlikely(!flag)) {
			x_perror("not enough space.");
		}
	}
}

void netpkg_stat_increase(int sockfd)
{
	return_if_fail(sockfd > -1 && ISOBJ(&g_netpkg_stat));
	struct pkginfo  *pkginfoptr = NULL;
	unsigned        size = sizeof(pkginfoptr);
	bool            flag = false;
	flag = DHashGet(g_netpkg_stat.actconn, (char *)&sockfd, sizeof(sockfd),
			(char *)&pkginfoptr, &size);

	if (likely(flag)) {
		// update
		time_t  t = 0;
		time_t  o = 0;
		time(&t);
		o = pkginfoptr->laststat;
		ATOMIC_CAS(&pkginfoptr->laststat, o, t);
		ATOMIC_INC(&pkginfoptr->counter);
	} else {
		x_perror("not get information about socket.");
	}
}

void netpkg_stat_remove(int sockfd)
{
	return_if_fail(sockfd > -1 && ISOBJ(&g_netpkg_stat));
	bool flag = false;
	flag = DHashDel(g_netpkg_stat.actconn, (char *)&sockfd, sizeof(sockfd));

	if (unlikely(!flag)) {
		x_perror("logic error");
	}
}

void netpkg_stat_print(int desfd, const char *file)
{
	return_if_fail((desfd > -1 || file) && ISOBJ(&g_netpkg_stat));

	if (likely(desfd > -1)) {
		SListTravel(g_netpkg_stat.pkgstat, _pkgstat_info4print, &desfd);
	} else {
		desfd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0644);

		if (likely(desfd > 0)) {
			SListTravel(g_netpkg_stat.pkgstat, _pkgstat_info4print, &desfd);
			close(desfd);
		} else {
			x_perror("Can't open [%s] for write : %s.",
				SWITCH_NULL_STR(file),
				x_strerror(errno));
		}
	}
}

static int _pkgstat_find4new(const char *buff, int size, void *user, int usrsize)
{
	struct pkginfo  *stat = NULL;
	struct pkginfo  *data = NULL;

	assert(user);

	if (unlikely(usrsize == 0)) {
		return -1;
	}

	assert(usrsize == sizeof(*stat));
	stat = (struct pkginfo *)user;

	assert(size = sizeof(*data));
	data = (struct pkginfo *)buff;

	if (memcmp(&stat->hostaddr, &data->hostaddr, sizeof(data->hostaddr)) == 0) {
		return 0;
	}

	return -1;
}

static bool _pkgstat_info4print(const char *buff, int size, void *user)
{
	int                     *sfd = (int *)user;
	char                    time1[TM_STRING_SIZE] = {};
	char                    time2[TM_STRING_SIZE] = {};
	const struct pkginfo    *data = (const struct pkginfo *)buff;

	assert(sfd && size == sizeof(*data));

	char    *ptr = NULL;
	int     flag = 0;

	ptr = TM_FormatTime(time1, sizeof(time1), data->firstconn, TM_FORMAT_DATETIME);
	return_val_if_fail(ptr, false);

	ptr = TM_FormatTime(time2, sizeof(time2), data->laststat, TM_FORMAT_DATETIME);
	return_val_if_fail(ptr, false);

	flag = dprintf(*sfd, "%15s | %15u | %20s | %20s \n",
			data->hostaddr, data->counter, time1, time2);

	if (unlikely(flag < 0)) {
		x_perror("Occurred a error : %s.\n", x_strerror(errno));
		return false;
	}

	return true;
}
