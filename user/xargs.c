/***********************************************
#
#      Filename: xargs.c
#
#        Author: Yu Zhenbo- yzbwcx@gmail.com
#   Description: ---
#        Create: 2021-06-06 11:07:02
# Last Modified: 2021-06-06 11:07:02
***********************************************/

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

#define MAX_STR_LEN 512 

char * parse(char * str){
	char *end;
	for(end = str; end < str + strlen(str) && (*end != '\\' || *(end+1) != 'n') && *end != '\n'; end++);
	*end = '\0';
	return end;
}

int main(int argc, char * argv[])
{

	// 从标准输入读入字符串
	char buf[MAX_STR_LEN];
	read(0, buf, MAX_STR_LEN);


//	int p[2];
//	pipe(p);
//	while(1){
//		int pid = fork();
//		if (pid == 0){
//			// 子进程
//			// 执行
//			close(p[1]);
//			char * paras[MAXARG];
//			char cmd[MAX_STR_LEN] = {'\0'};
//			strcpy(cmd, "bin/");
//			char *p = cmd + strlen(cmd);
//			p++;
//			strcpy(p, argv[1]);
//			//for (int i = 0; i < MAXARG; i++)
//
//			printf("cmd: %s\n", cmd);
////
////			// 从父进程接收命令和参数
////			read(p[0], &)
////			exec(argv[1], )
//			exit(0);
//		}else{
//			// 父进程
//			// 整理命令，发送到子进程
//
//			close(p[0]);
//			
//			char * start = buf;
//			while(*start != '\0'){
//				char *end = parse(start);	
//				write(p[1], &start, end-start+1);
//				start = end + 2;	
//			}
//
//			wait((int *)0);
//			break;
//		}
//	}

	char * start = buf;
	while(*start != '\0'){

		char *end = parse(start);	
		if(fork() == 0){
			char cmd[MAX_STR_LEN] = {'\0'};
			strcpy(cmd, "");
			char *p = cmd + strlen(cmd);
			strcpy(p, argv[1]);
			

			char * paras[MAXARG];
			int i;
			for (i = 1; i < argc; i++){
				paras[i-1] = argv[i];	
			}
			paras[i-1] = start;

			exec(cmd, paras);
			printf("ERROR while exec()\n");
			exit(0);
		}
		start = end + 2;	
		wait((int *)0);
	}



	exit(0);
}
