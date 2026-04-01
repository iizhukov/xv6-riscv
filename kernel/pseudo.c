#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
 
enum {
  PSEUDO_NULL = 0,
  PSEUDO_ZERO = 1,
  PSEUDO_URANDOM = 2,
  PSEUDO_NULLSTAT = 3,
};

static uint64 urandom_seed = 88172645463325252ULL;
static uint64 nullstat_written;

static uint64
lcg_next(void)
{
  urandom_seed = urandom_seed * 6364136223846793005ULL + 1;
  return urandom_seed;
}

int
pseudoread(int minor, int user_dst, uint64 dst, int n)
{
  if (n < 0)
    return -1;

  if (minor == PSEUDO_NULL) {
    return 0;
  }
  else if (minor == PSEUDO_ZERO) {
    char z = 0;

    for (int i = 0; i < n; i++) {
      if (either_copyout(user_dst, dst + i, &z, 1) < 0)
        return -1;
    }

    return n;
  }
  else if (minor == PSEUDO_URANDOM) {
    for (int i = 0; i < n; i++) {
      uint64 x = lcg_next();
      char b = (char)(x & 0xFF);

      if (either_copyout(user_dst, dst + i, &b, 1) < 0)
        return -1;
    }

    return n;

  }
  else if (minor == PSEUDO_NULLSTAT) {
    if (n != (int)sizeof(nullstat_written))
      return -1;

    if (either_copyout(user_dst, dst, &nullstat_written, sizeof(nullstat_written)) < 0)
      return -1;

    return sizeof(nullstat_written);
  }

  return -1;
}

int
pseudowrite(int minor, int user_src, uint64 src, int n)
{
  if (n < 0)
    return -1;

  if (minor == PSEUDO_NULL) {
    return n;
  }
  else if (minor == PSEUDO_ZERO) {
    return -1;
  }
  else if (minor == PSEUDO_URANDOM) {
    if (n != (int)sizeof(urandom_seed))
      return -1;

    if (either_copyin(&urandom_seed, user_src, src, sizeof(urandom_seed)) < 0)
      return -1;

    return sizeof(urandom_seed);
  }
  else if (minor == PSEUDO_NULLSTAT) {
    nullstat_written += (uint64)n;
    return n;
  }

  return -1;
}

void
pseudoinit(void)
{
  devsw[PSEUDO].read = pseudoread;
  devsw[PSEUDO].write = pseudowrite;
}
