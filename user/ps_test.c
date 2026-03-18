#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  int n;

  printf("Test ps_listinfo(NULL, 0) -> count\n");
  n = ps_listinfo(0, 0);

  if (n < 0) {
    fprintf(2, "FAIL\n");
    exit(1);
  }

  printf("OK\n");


  printf("Test lim=0, plist non-NULL\n");
  struct procinfo buf;
  n = ps_listinfo(&buf, 0);

  if (n <= 0) {
    fprintf(2, "FAIL\n");
    exit(1);
  }

  printf("OK\n");



  printf("Test invalid user address\n");
  n = ps_listinfo((struct procinfo *)0xFFFFFFFF, 10);

  if (n >= 0) {
    fprintf(2, "FAIL\n");
    exit(1);
  }

  printf("OK\n");



  printf("Test call with enough buffer\n");
  int cnt = ps_listinfo(0, 0);

  if (cnt < 0) {
    fprintf(2, "FAIL\n");
    exit(1);
  }

  struct procinfo *plist = malloc(sizeof(struct procinfo) * (cnt + 4));

  if (!plist) {
    fprintf(2, "FAIL\n");
    exit(1);
  }

  n = ps_listinfo(plist, cnt + 4);

  if (n != cnt) {
    fprintf(2, "FAIL\n");
    free(plist);
    exit(1);
  }
  
  printf("OK\n");

  free(plist);

  printf("All tests passed.\n");
  exit(0);
}
