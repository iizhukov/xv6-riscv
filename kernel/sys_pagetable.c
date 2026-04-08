#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

static void
flags_to_string(uint64 flags, char *buf)
{
  buf[0] = (flags & PTE_R) ? 'R' : '_';
  buf[1] = (flags & PTE_W) ? 'W' : '_';
  buf[2] = (flags & PTE_X) ? 'X' : '_';
  buf[3] = (flags & PTE_U) ? 'U' : '_';
  buf[4] = (flags & PTE_G) ? 'G' : '_';
  buf[5] = (flags & PTE_A) ? 'A' : '_';
  buf[6] = (flags & PTE_D) ? 'D' : '_';
  buf[7] = '\0';
}

static void
print_pagetable_recursive(pagetable_t pagetable, int level, int depth)
{
  pte_t *pte;
  char flags_str[8];

  for (int i = 0; i < 512; i++) {
    pte = &pagetable[i];

    if (*pte & PTE_V) {
      uint64 pa = PTE2PA(*pte);

      for (int d = 0; d < depth; d++)
        printf("........... ");

      printf("%x -> %x", i, (uint32)pa);

      if (level == 0) {
        flags_to_string(*pte, flags_str);
        printf(" %s", flags_str);
      }
      printf("\n");

      if (level > 0)
        print_pagetable_recursive((pagetable_t)pa, level - 1, depth + 1);
    }
  }
}

uint64
sys_pagetable_print(void)
{
  struct proc *p = myproc();

  printf("\nPAGETABLE ");
  printf("%x\n", (uint32)(uint64)p->pagetable);

  print_pagetable_recursive(p->pagetable, 2, 0);

  printf("\n");
  return 0;
}

static int
get_buffer_pages(uint64 buf, uint64 len, uint64 *pages, int max_pages)
{
  if (len == 0)
    return 0;

  uint64 start_va = PGROUNDDOWN(buf);
  uint64 end_va = PGROUNDDOWN(buf + len - 1);
  int num_pages = 0;

  for (uint64 va = start_va; va <= end_va && num_pages < max_pages; va += PGSIZE) {
    pages[num_pages++] = va;
  }

  return num_pages;
}

static int
validate_buffer(uint64 buf, uint64 len)
{
  struct proc *p = myproc();

  if (buf >= p->sz || buf + len > p->sz)
    return -1;

  return 0;
}

uint64
sys_page_flags_clear(void)
{
  uint64 buf, len, flags_mask;
  uint64 pages[1024];
  int num_pages;
  pte_t *pte;
  struct proc *p = myproc();

  argaddr(0, &buf);
  argaddr(1, &len);
  argaddr(2, &flags_mask);

  if (validate_buffer(buf, len) < 0)
    return -1;

  if (flags_mask & ~(PTE_D | PTE_A))
    return -1;

  if (flags_mask == 0)
    return 0;

  num_pages = get_buffer_pages(buf, len, pages, 1024);

  for (int i = 0; i < num_pages; i++) {
    pte = walk(p->pagetable, pages[i], 0);
    if (pte != 0 && (*pte & PTE_V))
      *pte &= ~flags_mask;
  }

  sfence_vma();

  return 0;
}

uint64
sys_page_flags_check(void)
{
  uint64 buf, len, flags_mask;
  uint64 pages[1024];
  int num_pages;
  pte_t *pte;
  struct proc *p = myproc();

  argaddr(0, &buf);
  argaddr(1, &len);
  argaddr(2, &flags_mask);

  if (validate_buffer(buf, len) < 0)
    return -1;

  if (flags_mask & ~(PTE_D | PTE_A))
    return -1;

  if (flags_mask == 0)
    return 0;

  num_pages = get_buffer_pages(buf, len, pages, 1024);

  for (int i = 0; i < num_pages; i++) {
    pte = walk(p->pagetable, pages[i], 0);
    
    if (pte != 0 && (*pte & PTE_V)) {
      if (*pte & flags_mask)
        return 1;
    }
  }

  return 0;
}
