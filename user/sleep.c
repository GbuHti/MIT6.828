/***********************************************
#
#      Filename: sleep.c
#
#        Author: Yu Zhenbo- yzbwcx@gmail.com
#   Description: ---
#        Create: 2021-05-26 07:14:44
# Last Modified: 2021-05-26 07:14:44
***********************************************/

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char *argv[])
{
	if (argc != 2){
		char message[30] = "usage: sleep seconds\n";
		write(1, message, 30);
		exit(1);
	}
	sleep(atoi(argv[1]));

	exit(0);
}
