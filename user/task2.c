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
        fprintf(2, "fork failed\n");
        return 1;
    } 

    if (pid == 0)
    {
        if (close(pipefd[1]) != 0)
        {
            fprintf(2, "close pipe write end failed\n");
            return 1;
        }
        close(0);

        if (dup(pipefd[0]) != 0)
        {
            fprintf(2, "dup failed\n");
            return 1;
        }

        if (close(pipefd[0]) != 0)
        {
            fprintf(2, "close pipe read end failed\n");
            return 1;
        }

        char *wc_argv[] = { "wc", 0 };
        int status = exec("/wc", wc_argv);
        if (status == -1)
        {
            fprintf(2, "exec failed\n");
            return 1;
        }

        return 0;
    }

    if (close(pipefd[0]) != 0)
    {
        fprintf(2, "close pipe read end failed\n");
        return 1;
    }
    for (i = 1; i < argc; i++)
    {
        char *arg = argv[i];
        int len = strlen(arg);
        int written = 0;
        int n;
        
        while (written < len) {
            n = write(pipefd[1], arg + written, len - written);

            if (n < 0) {
                fprintf(2, "write failed\n");
                return 1;
            }

            written += n;
        }
        
        n = write(pipefd[1], "\n", 1);

        if (n != 1) {
            fprintf(2, "write failed\n");
            return 1;
        }
    }
    if (close(pipefd[1]) != 0)
    {
        fprintf(2, "close pipe write end failed\n");
        return 1;
    }

    int status;
    wait(&status);

    return status;
}