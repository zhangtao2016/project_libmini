//
//  main.c
//  libmini
//
//  Created by 周凯 on 15/11/19.
//  Copyright © 2015年 zk. All rights reserved.
//

#include <stdio.h>
#include "libmini.h"

const ExceptT e = {
	"A Exception has been captured ...",
	0xffff0001
};

int main()
{
	volatile int sfd = -1;

	x_printf(D, "start ...");

#if 1
	TRY
	{
		TRY
		{
			sfd = TcpListen("0.0.0.0", "10000", NULL, NULL);
			RAISE_SYS_ERROR(sfd);
		}
		CATCH
		{
			int error = GetErrno();
			SetExcept(e);
			const ExceptT *eptr = GetExcept();
		}
		FINALLY
		{
			close(sfd);
		}
		END;

		TRY
		{
			sfd = TcpListen("0.0.0.1", "10000", NULL, NULL);
			RAISE_SYS_ERROR(sfd);
		}
		CATCH
		{}
		FINALLY
		{
			close(sfd);
		}
		END;

		RAISE(e);
		//		RAISE(EXCEPT_SYS);
	}
	EXCEPT(e)
	{}
	//	CATCH
	//	{}
	FINALLY
	{}
	END;
#else
	sfd = TcpListen("0.0.0.0", "10000", NULL, NULL);
	RAISE_SYS_ERROR(sfd);
	close(sfd);

	sfd = TcpListen("0.0.0.0", "10000", NULL, NULL);
	RAISE_SYS_ERROR(sfd);
	close(sfd);
#endif	/* if 1 */
	x_printf(D, "end ...");
	return EXIT_SUCCESS;
}

