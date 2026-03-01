#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char **argv)
{
    int pid, i, pipefd[2];

    pipe(pipefd);
    pid = fork();

    if (pid < 0)
    {
        printf("fork failed\n");
        return 1;
    } 

    if (pid == 0)
    {
        close(pipefd[1]);
        close(0);

        if (dup(pipefd[0]) != 0)
        {
            printf("dup failed\n");
            return 1;
        }

        close(pipefd[0]);

        char *wc_argv[] = { "wc", 0 };
        int status = exec("/wc", wc_argv);
        if (status == -1)
        {
            printf("exec failed\n");
            return 1;
        }

        return 0;
    }

    close(pipefd[0]);
    for (i = 1; i < argc; i++)
    {
        char *arg = argv[i];
        int len = strlen(arg);
        int written = 0;
        int n;
        
        while (written < len) {
            n = write(pipefd[1], arg + written, len - written);

            if (n < 0) {
                printf("write failed\n");
                return 1;
            }

            written += n;
        }
        
        n = write(pipefd[1], "\n", 1);

        if (n != 1) {
            printf("write failed\n");
            return 1;
        }
    }
    close(pipefd[1]);

    int status;
    wait(&status);

    return status;
}