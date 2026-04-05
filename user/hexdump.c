#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

static void print_hex_byte(unsigned char b)
{
  char hex[] = "0123456789ABCDEF";
  char buf[2];
  buf[0] = hex[(b >> 4) & 0xF];
  buf[1] = hex[b & 0xF];
  write(1, buf, 2);
}

int main(int argc, char *argv[])
{
  if (argc != 3) {
    fprintf(2, "Usage: hexdump N FILE\n");
    exit(1);
  }

  int n = atoi(argv[1]);
  if (n < 0) {
    fprintf(2, "hexdump: invalid N\n");
    exit(1);
  }

  int fd = open(argv[2], O_RDONLY);
  if (fd < 0) {
    fprintf(2, "hexdump: cannot open %s\n", argv[2]);
    exit(1);
  }

  char *buf = malloc(n);
  if (buf == 0) {
    fprintf(2, "hexdump: out of memory\n");
    close(fd);
    exit(1);
  }

  int total = 0;
  while (total < n) {
    int r = read(fd, buf + total, n - total);
    if (r < 0) {
      fprintf(2, "hexdump: read error\n");
      free(buf);
      close(fd);
      exit(1);
    }

    if (r == 0)
      break;
    total += r;
  }

  if (total != n) {
    fprintf(2, "hexdump: could not read %d bytes (got %d)\n", n, total);
    free(buf);
    close(fd);
    exit(1);
  }

  for (int i = 0; i < n; i++) {
    print_hex_byte((unsigned char)buf[i]);
    if (i + 1 < n) {
      write(1, " ", 1);
    }
  }

  write(1, "\n", 1);
  free(buf);
  close(fd);
  exit(0);
}
