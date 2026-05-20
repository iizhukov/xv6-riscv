#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "ext2.h"

int main(void) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        perror("malloc");
        return 1;
    }

    while (1) {
        if (len == cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);

            if (!nb) {
                perror("realloc");
                free(buf);
                return 1;
            }
            buf = nb;
        }
        ssize_t r = read(STDIN_FILENO, buf + len, cap - len);
        if (r < 0) {
            perror("read");
            free(buf);
            return 1;
        }

        if (r == 0)
            break;
        len += (size_t)r;
    }

    printf("%-10s  %s\n", "inode", "name");
    printf("%-10s  %s\n", "----------", "----");

    size_t pos = 0;
    while (pos + 8 <= len) {
        struct ext2_dir_entry_2 *de = (struct ext2_dir_entry_2 *)(buf + pos);

        if (de->rec_len == 0 || pos + de->rec_len > len)
            break;

        if (de->inode != 0 && de->name_len > 0)
            printf("%-10u  %.*s\n", de->inode, (int)de->name_len, de->name);
        
        pos += de->rec_len;
    }

    free(buf);
    return 0;
}
