/***********************************************
#
#      Filename: find.c
#
#        Author: Yu Zhenbo- yzbwcx@gmail.com
#   Description: find all the files in a directory tree with a specific name
#        Create: 2021-06-06 08:34:37
# Last Modified: 2021-06-06 08:34:37
***********************************************/

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char * getLastSeg(char * path){
	char *p;
	for(p = path + strlen(path); p >= path && *p != '/'; p--);
	p++;
	return p;
}

void find(char *path, char *target){

	char buf[512], *p;
	int fd;
	struct dirent de;
	struct stat st;

	if((fd = open(path, 0)) < 0){
		fprintf(2, "ls: cannot open %s\n", path);
		return;
	}

	if(fstat(fd, &st) < 0){
		fprintf(2, "ls: cannot stat %s\n", path);
		close(fd);
		return;
	}

	// 递归
	if ( st.type == T_DIR/*如果是目录就一直递归*/){
		// 构造新的path	
		strcpy(buf, path);
		p = buf + strlen(buf);
		*(p++) = '/';
		while(read(fd, &de, sizeof(de)) == sizeof(de) ){
			if(de.inum == 0 || de.inum == 1 || strcmp(de.name, ".")==0 || strcmp(de.name, "..")==0)
				continue;
			memmove(p, de.name, DIRSIZ);
			p[DIRSIZ] = 0;
			find(buf, target);
		}
	}else{
		// 如果当前path中最后一个字段与target一致,就输出当前path
		char *segment = getLastSeg(path);
		if (strcmp(segment, target) == 0){
			fprintf(1, "%s\n", path);	
		}
	}
	close(fd);

	return;
}

int
main(int argc, char *argv[])
{
  int i;
  
  if(argc < 2){
	exit(0); 
  }else if(argc < 3){
    find(argv[1], (char*)"");
    exit(0);
  }
  for(i=2; i<argc; i++)
    find(argv[1], argv[i]);
  exit(0);
}

