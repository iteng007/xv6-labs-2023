#include "kernel/types.h"
#include "user/user.h"
#define MAX 35
void redirect_msg(int in, int out);
int main() {
  int chan[2];
  if (pipe(chan) == -1) {
    printf("Wrong when create pipe\n");
  }
  int pid = fork();
  if (pid > 0) {
    int *buf = malloc(sizeof(int));
	printf("prime 2\n");
    for (int i = 2; i <= MAX; i++) {
      if (i % 2 != 0) {
        *buf = i;
        write(chan[1], buf, sizeof(int));
      }
    }
    close(chan[0]);
    close(chan[1]);
    while (wait(0) != -1) {
    }
  } else if (pid == 0) {
    while (1) {
      int in = chan[0];       // load old channel read end
      close(chan[1]);         // close old channel write end
      if (pipe(chan) == -1) { // create new chan for new child
        printf("Create pipe failed\n");
      };
      // Non-blocking write behavior when the pipe buffer is not full:
      // Writing data to the pipe is non-blocking as long as the buffer isn't
      // full. The write operation returns immediately, placing data into the
      // pipe's buffer.

      redirect_msg(in, chan[1]);
      int pid = fork();
      if (pid == 0) {
      } else if (pid > 0) {
        // This child needs to wait its child,our or someone's grandchild (:
        while (wait(0) != -1) {
        }
        break;
      }
    }
  } else {
    printf("Wrong when fork");
    exit(0);
  }
  return 0;
}

// This will redirect in to out and close(in) close(out);
void redirect_msg(int in, int out) {
  int *buf = malloc(sizeof(int));
  if (read(in, buf, sizeof(int)) != sizeof(int)) {
    printf("Child : %d error when reading from pipe\n", getpid());
    exit(-1);
  }
  int read_cnd = 1;
  int prime = *buf;
  printf("prime %d\n", prime);
  int read_size;
  while ((read_size = read(in, buf, sizeof(int))) > 0) {
    if (read_size < sizeof(int)) {
      printf("Error: Not enough data in pipe\n");
      break;
    }
    read_cnd++;
    if (*buf % prime != 0) {
      if (write(out, buf, sizeof(int)) != sizeof(int)) {
        printf("Parent: %d fail to write to pipe", getpid());
      }
    }
  }
  close(in);
  close(out);
  if (read_cnd == 1) {
    exit(0);
  }
}