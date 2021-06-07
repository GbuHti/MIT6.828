/***********************************************
#
#      Filename: primes.c
#
#        Author: Yu Zhenbo- yzbwcx@gmail.com
#   Description: ---
#        Create: 2021-05-30 21:40:30
# Last Modified: 2021-05-30 21:40:30
***********************************************/

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
void pipeline(int ifd){
	// 开辟一个process, 在这个空间里执行判断整理和输出动作
	// 习惯做法：先开辟空间，再根据pid分别执行不同任务
	// 现在不同: 先在主空间判断能否完成任务，不行再开辟空间，分发任务（pipe)
	

	// 如果数组中有剩余数字，递归调用pipeline
	int p[2];
	pipe(p);
	int pid = fork();
	if ( pid == 0){
		// 从左侧接收父进程的数据，进入流水线处理, 
		close(p[1]);
		int v;
		if (read(p[0], &v, 1)){  // 利用先导符,判断是否需要创建新进程
			pipeline(p[0]);	
		}
	}else{
		/************ part1: 进程初始化 ****************/
		// 进程开始时，从左侧读入数据, 默认数据有效
		int min;
		read(ifd, &min, sizeof(int));
		printf("prime %d\n", min);
		close(p[0]);
		
		/************ part2: 循环体执行**************/
		int v;
		int number;
		// 从左侧接收父进程的数据 --> 怎么区分这些pipe对？
		while(read(ifd, &v, 1)){
			// 在数组中找出最小数的倍数以外的数，将其送入p[1]
			read(ifd, &number, sizeof(int));
			if (number % min != 0){
				// 向右侧子进程写入数据
				write(p[1], "v", 1);
				write(p[1], &number, sizeof(int));	
			}
		}
		close(p[1]);
		wait((int *)0);
		exit(0);
	}

	//wait((int *)0);
}

int main(int argc, char *argv[])
{
	// 何时开辟进程?
	// 2, 3, 4, 5, 6, 7, 8, 9, 10
	// 当本进程处理完毕后，缓存中还剩有数字

	// 怎么确定需要的进程/管道数量？
	// 递归的过程
	
	// 开辟进程
	int p[2];
	pipe(p);
	int pid = fork();
	if (pid == 0){
		close(p[1]);
		char v;
		if (read(p[0], &v, 1)){
			pipeline(p[0]);
		}
	}else{
		close(p[0]);	
		for(int i = 2; i <= 35; i++){
			//printf("sended %d\n", i);
			write(p[1], "v", 1);		// 在发送数据之前，发送先导符“v"
			write(p[1], &i, sizeof(int));	
		}
		close(p[1]);
		wait((int *)0);
	}

	exit(0);
}

