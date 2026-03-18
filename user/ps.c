#include "kernel/types.h"
#include "user/user.h"

#define W_PID   5
#define W_NAME  9
#define W_STATE 10
#define W_PPID  7

static const char *state_str(int s) {
  switch (s) {
    case 0: return "UNUSED";
    case 1: return "USED";
    case 2: return "SLEEPING";
    case 3: return "RUNNABLE";
    case 4: return "RUNNING";
    case 5: return "ZOMBIE";
    default: return "?";
  }
}

static int ndigits(int n) {
  int d = 1;
  if (n < 0) n = -n;
  while (n >= 10) { d++; n /= 10; }
  return d;
}

static void pad(int w) {
  if (w <= 0) return;
  for (int i = 0; i < w; i++)
    printf(" ");
}

static const char *pname_by_ppid(struct procinfo *plist, int n, int ppid) {
  for (int i = 0; i < n; i++)
    if (plist[i].pid == ppid)
      return plist[i].name;

  return "-";
}

int main(int argc, char *argv[]) {
  int cnt = ps_listinfo(0, 0);

  if (cnt < 0) {
    fprintf(2, "ps: ps_listinfo failed\n");
    exit(1);
  }

  if (cnt == 0) {
    printf("pid");
    pad(W_PID - 3);
    printf("name");
    pad(W_NAME - 4);
    printf("state");
    pad(W_STATE - 5);
    printf("ppid");
    pad(W_PPID - 4);
    printf("pname\n");
    exit(0);
  }

  int lim = cnt + 8;
  struct procinfo *plist = malloc(sizeof(struct procinfo) * lim);

  if (!plist) {
    fprintf(2, "ps: malloc failed\n");
    exit(1);
  }

  for (;;) {
    int n = ps_listinfo(plist, lim);

    if (n >= 0 && n <= lim) {
      cnt = n;
      break;
    }

    if (n > lim) {
      free(plist);
      lim = n + 8;
      plist = malloc(sizeof(struct procinfo) * lim);

      if (!plist) {
        fprintf(2, "ps: malloc failed\n");
        exit(1);
      }

      continue;
    }

    fprintf(2, "ps: ps_listinfo error %d\n", n);
    free(plist);
    exit(1);
  }

  printf("pid");
  pad(W_PID - 3);
  printf("name");
  pad(W_NAME - 4);
  printf("state");
  pad(W_STATE - 5);
  printf("ppid");
  pad(W_PPID - 4);
  printf("pname\n");

  for (int i = 0; i < cnt; i++) {
    const char *pn = pname_by_ppid(plist, cnt, plist[i].ppid);
    const char *st = state_str(plist[i].state);

    printf("%d", plist[i].pid);
    pad(W_PID - ndigits(plist[i].pid));

    printf("%s", plist[i].name);
    pad(W_NAME - strlen(plist[i].name));

    printf("%s", st);
    pad(W_STATE - strlen(st));

    printf("%d", plist[i].ppid);
    pad(W_PPID - ndigits(plist[i].ppid));

    printf("%s\n", pn);
  }

  free(plist);
  exit(0);
}
