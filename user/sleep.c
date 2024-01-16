#include "kernel/types.h"
#include "user/user.h"
int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: sleep 10 for sleeing 10 seconds\n");
    exit(1);
  }
  int time_to_sleep = atoi(argv[1]);
  sleep(time_to_sleep);
  return 0;
}