#include "types.h"

#define PROC_NAME_MAX 16

struct procinfo {
  int pid;
  char name[PROC_NAME_MAX];
  int state;
  int ppid;
};
