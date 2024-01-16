#include "kernel/types.h"
#include "user/user.h"
// void itoa(char*buf,int *index,int num);
int
main(int argc, char *argv[]){
	int pipe1[2];//parent send to child
	int pipe2[2];//child send to parent
	pipe(pipe1);
	pipe(pipe2);
	int pid = fork();
	if (pid>0) {
	  //We are parent.
	  close(pipe1[0]);
	  close(pipe2[1]);
	  char * buf = malloc(sizeof(char));
	  
	  write(pipe1[1], buf, 1);//First, parent write to child
	  close(pipe1[1]);

	  read(pipe2[0], buf, 1);//Last,parent receive byte sent to child before from child
	  close(pipe2[0]);

	  printf("%d: received pong\n",getpid());
	  free(buf);
	}else if(pid==0) {
	  close(pipe1[1]);
	  close(pipe2[0]);

	  char * buf = malloc(sizeof(char));

	  read(pipe1[0],buf, 1);//Second, child read the first 
	  close(pipe1[1]);
	  
	  printf("%d: received ping\n",getpid());
	  
	  write(pipe2[1], buf, 1);//Third,child writes byte back to parent
	  close(pipe2[1]);	 

	  free(buf);
	}else {
		fprintf(2, "Wrong when fork a child\n");
	} 
	
	return 0;
}

//Well this function is written first for the case we 
//do not have getpid() implementation
//but we do have
// void itoa(char*buf,int *index,int num){
// 	if (num==0) {
// 		*(buf+*index+1)='\0';
// 		return;
// 	}
// 	itoa(buf,index,num/10);
// 	*(buf+*index) = num%10+'0';
// 	(*index)++;
// }