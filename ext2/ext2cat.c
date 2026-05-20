#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "ext2.h"

static int write_all(const void *buf, size_t n) {
    const char *p = buf;

    while (n > 0) {
        ssize_t w = write(STDOUT_FILENO, p, n);

        if (w < 0) {
            perror("write");
            return -1;
        }

        p += w;
        n -= (size_t)w;
    }

    return 0;
}

static int write_zeros(size_t n, void *zerobuf, size_t zbsz)
{
    while (n > 0) {
        size_t chunk = n < zbsz ? n : zbsz;
        if (write_all(zerobuf, chunk) != 0) return -1;
        n -= chunk;
    }

    return 0;
}

static ssize_t emit_block(
    int fd,
    uint32_t bsz,
    uint32_t blk,
    uint64_t remain,
    void *blkbuf,
    void *zerobuf
) {
    uint32_t to_write = (remain < bsz) ? (uint32_t)remain : bsz;

    if (blk == 0) {
        if (write_zeros(to_write, zerobuf, bsz) != 0)
            return -1;
    } else {
        if (read_block(fd, bsz, blk, blkbuf) != 0)
            return -1;

        if (write_all(blkbuf, to_write) != 0)
            return -1;
    }

    return (ssize_t)to_write;
}

static int walk_indirect(
    int fd,
    uint32_t bsz,
    uint32_t ind_blk,
    uint32_t ppb,
    uint64_t *remain,
    void *blkbuf,
    void *zerobuf,
    uint32_t *ptrbuf
) {
    if (read_block(fd, bsz, ind_blk, ptrbuf) != 0)
        return -1;

    for (uint32_t i = 0; i < ppb && *remain > 0; i++) {
        ssize_t n = emit_block(fd, bsz, ptrbuf[i], *remain, blkbuf, zerobuf);

        if (n < 0)
            return -1;

        *remain -= (uint64_t)n;
    }

    return 0;
}

static int walk_double(
    int fd,
    uint32_t bsz,
    uint32_t dind_blk,
    uint32_t ppb,
    uint64_t *remain,
    void *blkbuf,
    void *zerobuf,
    uint32_t *l1buf,
    uint32_t *l2buf
) {
    if (read_block(fd, bsz, dind_blk, l1buf) != 0)
        return -1;

    for (uint32_t i = 0; i < ppb && *remain > 0; i++) {
        if (l1buf[i] == 0) {
            uint64_t hole = (uint64_t)ppb * bsz;

            if (hole > *remain)
                hole = *remain;

            if (write_zeros((size_t)hole, zerobuf, bsz) != 0)
                return -1;

            *remain -= hole;
        } else {
            if (walk_indirect(fd, bsz, l1buf[i], ppb, remain, blkbuf, zerobuf, l2buf) != 0)
                return -1;
        }
    }
    
    return 0;
}

static int walk_triple(
    int fd,
    uint32_t bsz,
    uint32_t tind_blk,
    uint32_t ppb,
    uint64_t *remain,
    void *blkbuf,
    void *zerobuf,
    uint32_t *l1buf,
    uint32_t *l2buf,
    uint32_t *l3buf
) {
    if (read_block(fd, bsz, tind_blk, l1buf) != 0)
        return -1;

    for (uint32_t i = 0; i < ppb && *remain > 0; i++) {
        if (l1buf[i] == 0) {
            uint64_t hole = (uint64_t)ppb * ppb * bsz;

            if (hole > *remain)
                hole = *remain;

            if (write_zeros((size_t)hole, zerobuf, bsz) != 0)
                return -1;

            *remain -= hole;
        } else {
            if (walk_double(fd, bsz, l1buf[i], ppb, remain, blkbuf, zerobuf, l2buf, l3buf) != 0)
                return -1;
        }
    }

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

    if (file_size == 0) {
        close(fd);
        return 0;
    }

    void *blkbuf  = malloc(bsz);
    void *zerobuf = calloc(1, bsz);
    uint32_t *l1 = malloc(bsz);
    uint32_t *l2 = malloc(bsz);
    uint32_t *l3 = malloc(bsz);

    if (!blkbuf || !zerobuf || !l1 || !l2 || !l3) {
        perror("malloc");
        free(blkbuf); free(zerobuf); free(l1); free(l2); free(l3);
        close(fd);
        return 1;
    }

    uint64_t remain = file_size;
    int rc = 0;

    for (int i = 0; i < 12 && remain > 0; i++) {
        ssize_t n = emit_block(fd, bsz, inode.i_block[i], remain, blkbuf, zerobuf);

        if (n < 0) {
            rc = 1;
            goto done;
        }

        remain -= (uint64_t)n;
    }

#define EMIT_HOLE(size) do {                                                    \
    uint64_t hole = (size);                                                     \
    if (hole > remain) hole = remain;                                           \
    if (write_zeros((size_t)hole, zerobuf, bsz) != 0) { rc = 1; goto done; }    \
    remain -= hole;                                                             \
} while (0)

    if (remain > 0 && inode.i_block[12] != 0) {
        if (walk_indirect(fd, bsz, inode.i_block[12], ppb, &remain, blkbuf, zerobuf, l1) != 0) {
            rc = 1;
            goto done;
        }
    } else if (remain > 0) {
        EMIT_HOLE((uint64_t)ppb * bsz);
    }

    if (remain > 0 && inode.i_block[13] != 0) {
        if (walk_double(fd, bsz, inode.i_block[13], ppb, &remain, blkbuf, zerobuf, l1, l2) != 0) {
            rc = 1;
            goto done;
        }
    } else if (remain > 0) {
        EMIT_HOLE((uint64_t)ppb * ppb * bsz);
    }

    if (remain > 0 && inode.i_block[14] != 0) {
        if (walk_triple(fd, bsz, inode.i_block[14], ppb, &remain, blkbuf, zerobuf, l1, l2, l3) != 0) {
            rc = 1;
            goto done;
        }
    } else if (remain > 0) {
        EMIT_HOLE(remain);
    }

done:
    free(blkbuf); free(zerobuf); free(l1); free(l2); free(l3);
    close(fd);
    return rc;
}
