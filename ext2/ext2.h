#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define EXT2_SUPER_MAGIC                    0xEF53
#define EXT2_SUPERBLOCK_OFFSET              1024
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE   0x0002

#define EXT2_S_IFSOCK 0xC000
#define EXT2_S_IFLNK  0xA000
#define EXT2_S_IFREG  0x8000
#define EXT2_S_IFBLK  0x6000
#define EXT2_S_IFDIR  0x4000
#define EXT2_S_IFCHR  0x2000
#define EXT2_S_IFIFO  0x1000
#define EXT2_S_IFMT   0xF000

struct ext2_super_block {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algo_bitmap;
} __attribute__((packed));

struct ext2_group_desc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint8_t  bg_reserved[12];
} __attribute__((packed));

struct ext2_inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint8_t  i_osd2[12];
} __attribute__((packed));

struct ext2_dir_entry_2 {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[255];
} __attribute__((packed));

static inline uint32_t ext2_block_size(const struct ext2_super_block *sb) {
    return 1024u << sb->s_log_block_size;
}

static inline uint16_t ext2_inode_size(const struct ext2_super_block *sb) {
    return sb->s_rev_level >= 1 ? sb->s_inode_size : 128;
}

static inline uint32_t ext2_ptrs_per_block(const struct ext2_super_block *sb) {
    return ext2_block_size(sb) / 4;
}

static inline int pread_full(int fd, void *buf, size_t n, off_t off) {
    ssize_t r = pread(fd, buf, n, off);
    if (r < 0) { perror("pread"); return -1; }
    if ((size_t)r != n) {
        fprintf(stderr, "pread: short read (%zd/%zu)\n", r, n);
        return -1;
    }
    return 0;
}

static inline int read_block(int fd, uint32_t bsz, uint32_t blk, void *buf) {
    return pread_full(fd, buf, bsz, (off_t)blk * bsz);
}

static inline int read_superblock(int fd, struct ext2_super_block *sb) {
    if (pread_full(fd, sb, sizeof(*sb), EXT2_SUPERBLOCK_OFFSET) != 0)
        return -1;
    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Not an ext2 filesystem (magic=0x%04x)\n", sb->s_magic);
        return -1;
    }
    return 0;
}

static inline int read_group_desc(
    int fd,
    const struct ext2_super_block *sb,
    uint32_t group,
    struct ext2_group_desc *gd
) {
    uint32_t bsz = ext2_block_size(sb);
    off_t off = (off_t)(sb->s_first_data_block + 1) * bsz + (off_t)group * sizeof(struct ext2_group_desc);
    return pread_full(fd, gd, sizeof(*gd), off);
}

static inline int read_inode(
    int fd,
    const struct ext2_super_block *sb,
    uint32_t ino,
    struct ext2_inode *inode
) {
    if (ino == 0) {
        fprintf(stderr, "Invalid inode 0\n");
        return -1;
    }

    uint32_t bsz = ext2_block_size(sb);
    uint16_t isz = ext2_inode_size(sb);
    uint32_t group = (ino - 1) / sb->s_inodes_per_group;
    uint32_t local = (ino - 1) % sb->s_inodes_per_group;

    struct ext2_group_desc gd;
    if (read_group_desc(fd, sb, group, &gd) != 0)
        return -1;

    off_t off = (off_t)gd.bg_inode_table * bsz + (off_t)local * isz;
    uint32_t read_sz = isz < sizeof(*inode) ? isz : (uint32_t)sizeof(*inode);
    memset(inode, 0, sizeof(*inode));

    return pread_full(fd, inode, read_sz, off);
}

#endif
