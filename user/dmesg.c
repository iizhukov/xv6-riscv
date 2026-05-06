#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define DMESG_BUF (LOGBUF_PAGES * 4096 + 1)

static char buf[DMESG_BUF];

int
main(void)
{
  int n = dmesg(buf, DMESG_BUF);
  
  if (n < 0) {
    fprintf(2, "dmesg: syscall failed\n");
    exit(1);
  }

  printf("%s", buf);
  exit(0);
}
