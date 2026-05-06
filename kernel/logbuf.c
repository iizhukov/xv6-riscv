#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"
#include "log.h"

#define LOGBUF_SIZE (LOGBUF_PAGES * PGSIZE)

static struct {
  struct spinlock lock;
  char data[LOGBUF_SIZE];
  int  head;
  int  tail;
} lb;

static struct {
  struct spinlock lock;
  int  flags;
  uint expire_ticks;
} lctl;

static void
putc_locked(char c)
{
  int next = (lb.tail + 1) % LOGBUF_SIZE;
  if (next == lb.head)
    lb.head = (lb.head + 1) % LOGBUF_SIZE;
  lb.data[lb.tail] = c;
  lb.tail = next;
}

static void
printint_locked(long long xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[20];
  int i = 0;
  unsigned long long x;

  if (sign && (sign = (xx < 0)))
    x = (unsigned long long)-xx;
  else
    x = (unsigned long long)xx;

  do {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  if (sign)
    buf[i++] = '-';

  while (--i >= 0)
    putc_locked(buf[i]);
}

static void
printptr_locked(uint64 x)
{
  putc_locked('0');
  putc_locked('x');

  for (int i = 0; i < (int)(sizeof(uint64) * 2); i++, x <<= 4)
    putc_locked("0123456789abcdef"[x >> (sizeof(uint64) * 8 - 4)]);
}

void
logbufinit(void)
{
  initlock(&lb.lock, "logbuf");
  lb.head = 0;
  lb.tail = 0;
  lb.data[0] = '\n';
  lb.tail = 1;

  initlock(&lctl.lock, "logctl");
  lctl.flags = 0;
  lctl.expire_ticks = 0;
}

void
pr_msg(const char *fmt, ...)
{
  va_list ap;
  int cx, c0, c1, c2;
  char *s;

  acquire(&tickslock);
  uint xticks = ticks;
  release(&tickslock);

  acquire(&lb.lock);

  putc_locked('[');
  printint_locked(xticks, 10, 0);
  putc_locked(']');
  putc_locked(' ');

  va_start(ap, fmt);
  for (int i = 0; (cx = fmt[i] & 0xff) != 0; i++) {
    if (cx != '%') {
      putc_locked(cx);
      continue;
    }

    i++;
    c0 = fmt[i+0] & 0xff;
    c1 = c2 = 0;
    
    if (c0)
      c1 = fmt[i+1] & 0xff;

    if (c1)
      c2 = fmt[i+2] & 0xff;

    if (c0 == 'd') {
      printint_locked(va_arg(ap, int), 10, 1);
    }
    else if (c0 == 'l' && c1 == 'd') {
      printint_locked(va_arg(ap, uint64), 10, 1); i++;
    }
    else if (c0 == 'l' && c1 == 'l' && c2 == 'd') {
      printint_locked(va_arg(ap, uint64), 10, 1); i += 2;
    }
    else if (c0 == 'u') {
      printint_locked(va_arg(ap, uint32), 10, 0);
    }
    else if (c0 == 'l' && c1 == 'u') {
      printint_locked(va_arg(ap, uint64), 10, 0); i++;
    }
    else if (c0 == 'l' && c1 == 'l' && c2 == 'u') {
      printint_locked(va_arg(ap, uint64), 10, 0); i += 2;
    }
    else if (c0 == 'x') {
      printint_locked(va_arg(ap, uint32), 16, 0);
    }
    else if (c0 == 'l' && c1 == 'x') {
      printint_locked(va_arg(ap, uint64), 16, 0); i++;
    }
    else if (c0 == 'l' && c1 == 'l' && c2 == 'x') {
      printint_locked(va_arg(ap, uint64), 16, 0); i += 2;
    }
    else if (c0 == 'p') {
      printptr_locked(va_arg(ap, uint64));
    }
    else if (c0 == 'c') {
      putc_locked(va_arg(ap, uint));
    }
    else if (c0 == 's') {
      if ((s = va_arg(ap, char *)) == 0) s = "(null)";
      for (; *s; s++) putc_locked(*s);
    }
    else if (c0 == '%') {
      putc_locked('%');
    }
    else if (c0 == 0) {
      break;
    }
    else {
      putc_locked('%');
      putc_locked(c0);
    }
  }
  va_end(ap);

  putc_locked('\n');
  release(&lb.lock);
}

int
dmesg_copy(pagetable_t pt, uint64 uaddr, int maxlen)
{
  if (maxlen <= 0)
    return 0;

  acquire(&lb.lock);

  int tail = lb.tail;

  int start = lb.head;
  while (start != tail) {
    char c = lb.data[start];
    start = (start + 1) % LOGBUF_SIZE;

    if (c == '\n')
      break;
  }

  int n = 0;
  int i = start;
  while (i != tail && n < maxlen - 1) {
    int end = (i < tail) ? tail : LOGBUF_SIZE;
    int chunk = end - i;

    if (n + chunk > maxlen - 1)
      chunk = maxlen - 1 - n;
    if (copyout(pt, uaddr + n, &lb.data[i], chunk) < 0) {
      release(&lb.lock);
      return -1;
    }

    n += chunk;
    i = (i + chunk) % LOGBUF_SIZE;
  }

  char zero = '\0';
  copyout(pt, uaddr + n, &zero, 1);

  release(&lb.lock);
  return n;
}

int
log_enabled(int category)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);

  acquire(&lctl.lock);

  if ((lctl.flags & category) == 0) {
    release(&lctl.lock);
    return 0;
  }

  if (lctl.expire_ticks != 0 && xticks >= lctl.expire_ticks) {
    lctl.flags = 0;
    lctl.expire_ticks = 0;

    release(&lctl.lock);
    return 0;
  }

  release(&lctl.lock);
  return 1;
}

int
logctl_set(int flags, int duration)
{
  uint xticks;
  int old;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);

  acquire(&lctl.lock);
  old = lctl.flags;

  if (flags != -1) {
    lctl.flags = flags & LOG_ALL;

    if (lctl.flags != 0 && duration > 0)
      lctl.expire_ticks = xticks + (uint)duration;
    else
      lctl.expire_ticks = 0;
  }

  release(&lctl.lock);

  return old;
}
