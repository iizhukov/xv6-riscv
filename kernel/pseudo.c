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

static struct spinlock pseudo_lock;
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
    char buf[256];
    int bufsize = sizeof(buf);
    memset(buf, 0, bufsize);

    int total = 0;
    while (total < n) {
      int chunk = n - total;
      if (chunk > bufsize)
        chunk = bufsize;

      if (either_copyout(user_dst, dst + total, buf, chunk) < 0)
        return -1;

      total += chunk;
    }

    return n;
  }
  else if (minor == PSEUDO_URANDOM) {
    char buf[256];
    int bufsize = sizeof(buf);
    int total = 0;

    acquire(&pseudo_lock);
    while (total < n) {
      int chunk = n - total;
      if (chunk > bufsize)
        chunk = bufsize;

      for (int i = 0; i < chunk; i++) {
        uint64 x = lcg_next();
        buf[i] = (char)(x & 0xFF);
      }

      release(&pseudo_lock);
      if (either_copyout(user_dst, dst + total, buf, chunk) < 0)
        return -1;
      acquire(&pseudo_lock);

      total += chunk;
    }
    release(&pseudo_lock);

    return n;
  }
  else if (minor == PSEUDO_NULLSTAT) {
    if (n != (int)sizeof(nullstat_written))
      return -1;

    uint64 val;
    acquire(&pseudo_lock);
    val = nullstat_written;
    release(&pseudo_lock);

    if (either_copyout(user_dst, dst, &val, sizeof(val)) < 0)
      return -1;

    return sizeof(val);
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

    uint64 newseed;
    if (either_copyin(&newseed, user_src, src, sizeof(newseed)) < 0)
      return -1;

    acquire(&pseudo_lock);
    urandom_seed = newseed;
    release(&pseudo_lock);

    return sizeof(urandom_seed);
  }
  else if (minor == PSEUDO_NULLSTAT) {
    acquire(&pseudo_lock);
    nullstat_written += (uint64)n;
    release(&pseudo_lock);
    return n;
  }

  return -1;
}

void
pseudoinit(void)
{
  initlock(&pseudo_lock, "pseudo");
  devsw[PSEUDO].read = pseudoread;
  devsw[PSEUDO].write = pseudowrite;
}
