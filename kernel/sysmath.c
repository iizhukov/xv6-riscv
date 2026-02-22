#include "types.h"
#include "syscall.h"

void argint(int n, int *ip);

int
sys_add(void)
{
    int a, b;

    argint(0, &a);
    argint(1, &b);

    return a + b;
}
