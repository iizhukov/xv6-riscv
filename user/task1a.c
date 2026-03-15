#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char **argv)
{
    int pid = fork();
    int status;

    if (pid == 0)
    {
        pause(5 * 10);
        return 1;
    }

    if (pid < 0)
    {
        printf("fork failed\n");
        return 1;
    }
    
    printf("parent pid: %d\n", getpid());
    printf("child pid: %d\n", pid);
    pid = wait(&status);

    printf("child %d is done with %d status\n", pid, status);
    return 0;
}