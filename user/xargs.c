#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"
int
read_arg(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
	if(c == '\n' || c == '\r'||c==' ')
      break;
    buf[i++] = c;
  }
  buf[i] = '\0';
  return i;
}
int main(int argc,char *argv[]){
	if (argc<2) {
		exit(0);
	}
	char **newArray = malloc((argc+1) * sizeof(char*));
	int i = 0;
	for (; i+1<argc; i++) {
		newArray[i] = argv[i+1];
	}
	char buf[25];
	while (read_arg(buf, 25)) {
		newArray[argc-1] = buf;
		newArray[argc]=0;
		int pid = fork();
		if (pid==0) {
			exec(newArray[0], newArray);
		}else if (pid>0) {
			wait(0);
		}else {
		
		}
	}
	return 0;
}

