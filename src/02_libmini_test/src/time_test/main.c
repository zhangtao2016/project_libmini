#include "libmini.h"

int main(int argc, char **argv)
{
	time_t  sec = 0;
	char    buff[TM_STRING_SIZE] = {};
	char    *ptr = NULL;

	TM_GetTimeStamp(&sec, NULL);

	ptr = TM_FormatTime(buff, sizeof(buff), sec, TM_FORMAT_DATETIME);

	x_printf(I, "current time : %s.", SWITCH_NULL_STR(ptr));

	return EXIT_SUCCESS;
}

