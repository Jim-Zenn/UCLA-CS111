/*
 * lab3a.h
 * Copyright (C) 2019 Jim Zenn <zenn@ucla.edu>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef LAB3A_H
#define LAB3A_H

#include <stdio.h>

#ifndef EXT2_FS_H
#define EXT2_FS_H
#include "ext2_fs.h"
#endif

/* typedefs */
typedef struct ext2_super_block superblock_t;
typedef struct ext2_group_desc group_descriptor_t;
typedef struct ext2_inode inode_t;
typedef struct ext2_dir_entry dir_entry_t;

/* macros */
#define BOOT_BLOCK_SIZE 1024
#define BIT_PER_BYTE 8
#define TIME_STRING_SIZE 80

#endif /* !LAB3A_H */
