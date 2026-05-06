#include "kernel/types.h"
#include "kernel/log.h"
#include "user/user.h"

static void
usage(void)
{
  fprintf(2,
    "usage:\n"
    "  logctl                          show current state\n"
    "  logctl on  [syscall|intr|proc|exec|all] [N]\n"
    "  logctl off [syscall|intr|proc|exec|all]\n"
    "  logctl clear\n"
    "  N = duration in ticks (0 = indefinite)\n"
  );
  exit(1);
}

static int
parse_cats(int argc, char **argv, int start)
{
  int mask = 0;
  for(int i = start; i < argc; i++) {
    if (strcmp(argv[i], "syscall") == 0)   mask |= LOG_SYSCALL;
    else if (strcmp(argv[i], "intr") == 0) mask |= LOG_INTR;
    else if (strcmp(argv[i], "proc") == 0) mask |= LOG_PROC;
    else if (strcmp(argv[i], "exec") == 0) mask |= LOG_EXEC;
    else if (strcmp(argv[i], "all")  == 0) mask |= LOG_ALL;
    else break;
  }
  return mask;
}

static void
print_flags(int f)
{
  printf("logging: %s%s%s%s(flags=0x%x)\n",
         (f & LOG_SYSCALL) ? "syscall " : "",
         (f & LOG_INTR)    ? "intr "    : "",
         (f & LOG_PROC)    ? "proc "    : "",
         (f & LOG_EXEC)    ? "exec "    : "",
         f);
}

int
main(int argc, char **argv)
{
  if (argc == 1) {
    int cur = logctl(-1, 0);
    print_flags(cur);
    exit(0);
  }

  if (strcmp(argv[1], "clear") == 0) {
    logctl(0, 0);
    exit(0);
  }

  if (strcmp(argv[1], "on") == 0) {
    if (argc < 3)
      usage();

    int mask = parse_cats(argc, argv, 2);

    if (mask == 0)
      usage();

    int duration = 0;

    if (argc > 2) {
      int last = atoi(argv[argc - 1]);

      if (last > 0)
        duration = last;
    }

    int cur = logctl(-1, 0);
    logctl(cur | mask, duration);
    exit(0);
  }

  if (strcmp(argv[1], "off") == 0) {
    if (argc < 3)
      usage();

    int mask = parse_cats(argc, argv, 2);

    if (mask == 0)
      usage();

    int cur = logctl(-1, 0);
    logctl(cur & ~mask, 0);
    exit(0);
  }

  usage();
  return 1;
}
