/***********************************************
#
#      Filename: pingpong.c
#
#        Author: Yu Zhenbo- yzbwcx@gmail.com
#   Description: ---
#        Create: 2021-05-26 07:33:02
# Last Modified: 2021-05-26 07:33:02
***********************************************/

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void){
	
	int pL[2];
	int pR[2];
	pipe(pR);
	pipe(pL);
	int pid = fork();
	if (pid == 0){
		char received;
		read(pL[0], &received, 1);
		close(pL[0]);
		//write(1, &received, 1);
		//write(1, "\n", 1);
		int pid = getpid();
		printf("%d: received ping\n", pid);
		write(pR[1], "Z", 1);
		close(pR[1]);
		exit(0);
	}else{
		write(pL[1], "x", 1);
		close(pL[1]);
		char received;
		read(pR[0], &received, 1);
		close(pR[0]);
		//write(1, &received, 1);
		//write(1, "\n", 1);
		int pid = getpid();
		printf("%d: received pong\n", pid);
	}
	exit(0);
}
