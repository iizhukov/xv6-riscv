#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

static int hexval(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;

  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;

  return -1;
}

int main(int argc, char *argv[])
{
  if (argc != 3) {
    fprintf(2, "Usage: hexwrite HEXBYTES FILE\n");
    exit(1);
  }

  char *hex = argv[1];
  int len = strlen(hex);
  if (len == 0 || (len % 2) != 0) {
    fprintf(2, "hexwrite: HEXBYTES must have even length\n");
    exit(1);
  }

  int fd = open(argv[2], O_WRONLY);
  if (fd < 0) {
    fprintf(2, "hexwrite: cannot open %s\n", argv[2]);
    exit(1);
  }

  int nbytes = len / 2;
  char *buf = malloc(nbytes);
  if (buf == 0) {
    fprintf(2, "hexwrite: out of memory\n");
    close(fd);
    exit(1);
  }

  for (int i = 0; i < len; i += 2) {
    int hi = hexval(hex[i]);
    int lo = hexval(hex[i+1]);

    if (hi < 0 || lo < 0) {
      fprintf(2, "hexwrite: invalid hex\n");
      free(buf);
      close(fd);
      exit(1);
    }

    buf[i/2] = (char)((hi << 4) | lo);
  }

  if (write(fd, buf, nbytes) != nbytes) {
    printf("Write error\n");
    free(buf);
    close(fd);
    exit(1);
  }

  free(buf);
  close(fd);
  exit(0);
}

