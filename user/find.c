#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/fs.h"
void extract_filename(char *buf,const char *path){
	int len = strlen(path);
	int i;
	for (i = len-1 ; i>=0; i--) {
		if (path[i]=='/') {
			break;
		}
	}
	if (i<0) {
		memcpy(buf,path,len+1);
	}else {
		memset(buf, 0, len+1);
		memcpy(buf,path+i+1,len-i-1);
	}
}

void find(char * path,int file_cnt,char *filename[]){
	// printf("Current path:\t%s\n",path);
	char buf[512], *p;
  	int fd;
  	struct dirent de;
  	struct stat st;
	// printf("Path:\t%s\n",path);
	if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  	}
	if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  	}
	switch(st.type){
    case T_DEVICE:
    case T_FILE:
	fprintf(2,"%s is not a directory",buf);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        continue;
      }
	  if (st.type==T_DIR&&buf[strlen(buf)-1]!='.') {
		find(buf,file_cnt, filename);
	  }else if (st.type==T_FILE) {
		if (file_cnt!=0) {
			char temp[15];
			extract_filename(temp, buf);
			for (int i = 0 ; i<file_cnt; i++) {
				if (strcmp(temp, filename[i])==0) {
					printf("%s\n",buf);
				}
			}
		}else {
			printf("%s\n", (buf));
		}
	  }
    }
    break;
  }
    close(fd);
}

int main(int argc,char * argv[]){
	if (argc<3) {
		find(".", 0, 0);
	}else {
		find(argv[1], argc-2, argv+2);
	}
	exit(0);
}

