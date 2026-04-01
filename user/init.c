// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  int fd;
  if ((fd = open("null", O_RDWR)) < 0) {
    mknod("null", PSEUDO, 0);
  }
  else
    close(fd);

  if ((fd = open("zero", O_RDONLY)) < 0) {
    mknod("zero", PSEUDO, 1);
  }
  else
    close(fd);

  if ((fd = open("urandom", O_RDWR)) < 0) {
    mknod("urandom", PSEUDO, 2);
  }
  else
    close(fd);

  if ((fd = open("nullstat", O_RDWR)) < 0) {
    mknod("nullstat", PSEUDO, 3);
  }
  else
    close(fd);

  for(;;){
    printf("init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      exec("sh", argv);
      printf("init: exec sh failed\n");
      exit(1);
    }

    for(;;){
      // this call to wait() returns if the shell exits,
      // or if a parentless process exits.
      wpid = wait((int *) 0);
      if(wpid == pid){
        // the shell exited; restart it.
        break;
      } else if(wpid < 0){
        printf("init: wait returned an error\n");
        exit(1);
      } else {
        // it was a parentless process; do nothing.
      }
    }
  }
}
