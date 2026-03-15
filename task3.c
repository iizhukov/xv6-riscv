#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#define BUF_SIZE 8 * 1024

int main(int argc, char *argv[])
{
    int pipefd[2];
    pid_t pid;
    
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        return 1;
    }
    
    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return 1;
    }
    
    if (pid == 0)
    {
        if (close(pipefd[1]) == -1)
        {
            perror("close pipe write end");
            return 1;
        }
        
        char buffer[BUF_SIZE];
        ssize_t read_cnt;
        
        while ((read_cnt = read(
            pipefd[0], buffer, sizeof(buffer)
        )) > 0)
        {
            ssize_t write_cnt = 0;

            while (write_cnt < read_cnt)
            {
                ssize_t n = write(
                    STDOUT_FILENO, buffer + write_cnt, read_cnt - write_cnt
                );

                if (n == -1)
                {
                    perror("write");
                    return 1;
                }

                write_cnt += n;
            }
        }
        
        if (read_cnt == -1)
        {
            perror("read");
            return 1;
        }
        
        if (close(pipefd[0]) == -1)
        {
            perror("close pipe read end");
            return 1;
        }
        return 0;
    }

    if (close(pipefd[0]) == -1)
    {
        perror("close pipe read end");
        return 1;
    }
    
    for (int i = 1; i < argc; i++)
    {
        char *arg = argv[i];
        int len = strlen(arg);
        int written = 0;
        
        while (written < len)
        {
            ssize_t n = write(pipefd[1], arg + written, len - written);

            if (n == -1)
            {
                perror("write");
                return 1;
            }

            written += n;
        }
        
        char newline = '\n';

        if (write(pipefd[1], &newline, 1) != 1)
        {
            perror("write");
            return 1;
        }
    }
    
    if (close(pipefd[1]) == -1)
    {
        perror("close pipe write end");
        return 1;
    }

    int status;
    wait(&status);

    return status;
}
