#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include "ext2.h"

static void fmt_time(uint32_t ts, char *buf, size_t len) {
    if (ts == 0) {
        snprintf(buf, len, "(none)");
        return;
    }

    time_t t = (time_t)ts;
    struct tm *tm = gmtime(&t);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S UTC", tm);
}

static const char *filetype_str(uint16_t mode) {
    switch (mode & EXT2_S_IFMT) {
    case EXT2_S_IFREG: return "regular file";
    case EXT2_S_IFDIR: return "directory";
    case EXT2_S_IFLNK: return "symbolic link";
    case EXT2_S_IFBLK: return "block device";
    case EXT2_S_IFCHR: return "character device";
    case EXT2_S_IFIFO: return "fifo";
    case EXT2_S_IFSOCK: return "socket";
    default: return "unknown";
    }
}

static void fmt_mode(uint16_t mode, char *buf) {
    static const char ft[] = { '?', 'p', 'c', '?', 'd', '?', 'b', '?', '-', '?', 'l', '?', 's', '?', '?', '?' };

    buf[0] = ft[(mode & EXT2_S_IFMT) >> 12];
    buf[1] = (mode & 0400) ? 'r' : '-';
    buf[2] = (mode & 0200) ? 'w' : '-';
    buf[3] = (mode & 0100) ? ((mode & 04000) ? 's' : 'x') : ((mode & 04000) ? 'S' : '-');
    buf[4] = (mode & 0040) ? 'r' : '-';
    buf[5] = (mode & 0020) ? 'w' : '-';
    buf[6] = (mode & 0010) ? ((mode & 02000) ? 's' : 'x') : ((mode & 02000) ? 'S' : '-');
    buf[7] = (mode & 0004) ? 'r' : '-';
    buf[8] = (mode & 0002) ? 'w' : '-';
    buf[9] = (mode & 0001) ? ((mode & 01000) ? 't' : 'x') : ((mode & 01000) ? 'T' : '-');
    buf[10] = '\0';
}

static int print_indirect(
    int fd,
    uint32_t bsz,
    uint32_t ind_blk,
    uint32_t ppb,
    const char *prefix
) {
    uint32_t *ptrs = malloc(bsz);

    if (!ptrs) {
        perror("malloc");
        return -1;
    }

    if (read_block(fd, bsz, ind_blk, ptrs) != 0) {
        free(ptrs);
        return -1;
    }

    printf("%s", prefix);
    int first = 1;
    for (uint32_t i = 0; i < ppb; i++) {
        if (ptrs[i] == 0)
            continue;

        if (!first)
            printf(", ");

        printf("%u", ptrs[i]);
        first = 0;
    }

    printf("]\n");
    free(ptrs);

    return 0;
}

static int print_double_indirect(int fd, uint32_t bsz, uint32_t dind_blk, uint32_t ppb) {
    uint32_t *l1 = malloc(bsz);

    if (!l1) {
        perror("malloc");
        return -1;
    }

    if (read_block(fd, bsz, dind_blk, l1) != 0) {
        free(l1);
        return -1;
    }

    for (uint32_t i = 0; i < ppb; i++) {
        if (l1[i] == 0)
            continue;

        char prefix[64];
        snprintf(prefix, sizeof(prefix), "      L1[%u]=%u: [", i, l1[i]);
        if (print_indirect(fd, bsz, l1[i], ppb, prefix) != 0) {
            free(l1);
            return -1;
        }
    }
    free(l1);

    return 0;
}

static int print_triple_indirect(int fd, uint32_t bsz, uint32_t tind_blk, uint32_t ppb) {
    uint32_t *l1 = malloc(bsz);
    if (!l1) {
        perror("malloc");
        return -1;
    }

    if (read_block(fd, bsz, tind_blk, l1) != 0) {
        free(l1);
        return -1;
    }

    for (uint32_t i = 0; i < ppb; i++) {
        if (l1[i] == 0)
            continue;

        printf("    L1[%u]=%u:\n", i, l1[i]);
        uint32_t *l2 = malloc(bsz);

        if (!l2) {
            perror("malloc");
            free(l1);
            return -1;
        }

        if (read_block(fd, bsz, l1[i], l2) != 0) {
            free(l2);
            free(l1);
            return -1;
        }

        for (uint32_t j = 0; j < ppb; j++) {
            if (l2[j] == 0)
                continue;

            char prefix[64];
            snprintf(prefix, sizeof(prefix), "      L2[%u]=%u: [", j, l2[j]);
            if (print_indirect(fd, bsz, l2[j], ppb, prefix) != 0) {
                free(l2); free(l1); return -1;
            }
        }

        free(l2);
    }

    free(l1);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <device/image> <inode>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror(argv[1]);
        return 1;
    }

    struct ext2_super_block sb;
    if (read_superblock(fd, &sb) != 0) {
        close(fd);
        return 1;
    }

    uint32_t ino = (uint32_t)strtoul(argv[2], NULL, 10);
    struct ext2_inode inode;
    if (read_inode(fd, &sb, ino, &inode) != 0) {
        close(fd);
        return 1;
    }

    uint32_t bsz = ext2_block_size(&sb);
    uint32_t ppb = ext2_ptrs_per_block(&sb);

    uint64_t file_size = inode.i_size;
    if ((inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFREG &&
        (sb.s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_LARGE_FILE))
        file_size |= (uint64_t)inode.i_dir_acl << 32;

    char modebuf[11];
    fmt_mode(inode.i_mode, modebuf);
    char atime[32], ctime_[32], mtime[32], dtime[32];
    fmt_time(inode.i_atime, atime, sizeof(atime));
    fmt_time(inode.i_ctime, ctime_, sizeof(ctime_));
    fmt_time(inode.i_mtime, mtime, sizeof(mtime));
    fmt_time(inode.i_dtime, dtime, sizeof(dtime));

    printf("Inode:       %u\n",   ino);
    printf("Type:        %s\n",   filetype_str(inode.i_mode));
    printf("Mode:        %04o (%s)\n", inode.i_mode & 07777, modebuf);
    printf("UID:         %u\n",   inode.i_uid);
    printf("GID:         %u\n",   inode.i_gid);
    printf("Size:        %llu bytes\n", (unsigned long long)file_size);
    printf("Links:       %u\n",   inode.i_links_count);
    printf("Blocks:      %u (512-byte units)\n", inode.i_blocks);
    printf("Flags:       0x%08x\n", inode.i_flags);
    printf("Generation:  %u\n",   inode.i_generation);
    printf("File ACL:    %u\n",   inode.i_file_acl);
    printf("Access:      %s\n",   atime);
    printf("Change:      %s\n",   ctime_);
    printf("Modify:      %s\n",   mtime);
    printf("Delete:      %s\n",   dtime);

    printf("\nBlock addresses (block size: %u bytes, ptrs/block: %u):\n", bsz, ppb);

    for (int i = 0; i < 12; i++) {
        if (inode.i_block[i] == 0)
            continue;

        printf("  Direct[%2d]: %u\n", i, inode.i_block[i]);
    }

    if (inode.i_block[12] != 0) {
        printf("  Single-indirect -> block %u: [", inode.i_block[12]);
        print_indirect(fd, bsz, inode.i_block[12], ppb, "");
    }

    if (inode.i_block[13] != 0) {
        printf("  Double-indirect -> block %u:\n", inode.i_block[13]);
        print_double_indirect(fd, bsz, inode.i_block[13], ppb);
    }
    
    if (inode.i_block[14] != 0) {
        printf("  Triple-indirect -> block %u:\n", inode.i_block[14]);
        print_triple_indirect(fd, bsz, inode.i_block[14], ppb);
    }

    close(fd);
    return 0;
}
