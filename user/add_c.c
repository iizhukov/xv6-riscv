#include "kernel/types.h"
#include "user/user.h"

#define BUFFER_SIZE 64

void read_line(char *buffer) {
    char *p = buffer;
    int status;
    char symb;

    while (p < buffer + BUFFER_SIZE - 1) {
        status = read(0, &symb, 1);

        if (status < 0) {
            fprintf(2, "ERROR\n");
            exit(1);
        }
        
        if (status == 0 || symb == '\n' || symb == '\r')
            break;

        *p++ = symb;
    }

    *p = '\0';
}

int parse_num(int *num, char **ptr) {
    while (**ptr == ' ')
        (*ptr)++;

    if (**ptr == '\0')
        return -1;

    char *start = *ptr;
    
    while (**ptr != ' ' && **ptr != '\0')
        (*ptr)++;

    char tmp = **ptr;
    if (**ptr != '\0')
        **ptr = '\0';

    *num = atoi(start);

    if (tmp != '\0')
        **ptr = tmp;

    return 0;
}

int main(int argc, char *argv[]) {
    char buffer[BUFFER_SIZE];

    read_line(buffer);
    printf("|%s|\n", buffer);

    if (buffer[0] == '\0') {
        fprintf(2, "EMPTY\n");
        return 1;
    }

    int a, b;
    char *ptr = buffer;

    if (parse_num(&a, &ptr) < 0) {
        fprintf(2, "ERROR\n");
        return 1;
    }

    if (parse_num(&b, &ptr) < 0) {
        fprintf(2, "ERROR\n");
        return 1;
    }

    int sum = add(a, b);
    printf("add(%d, %d) = %d\n", a, b, sum);

    return 0;
}
