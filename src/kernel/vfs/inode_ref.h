#pragma once

#include "../util/refcount.h"
#include "mountpoint_ref.h"
#include <stdbool.h>
#include <sys/types.h>

typedef struct inode inode_t;

DEFINE_REFCOUNT_INC1(inode)

typedef struct
{
    const struct stat* st;
    void* fs_specific;
    void (*free_fs_specific_data)(void*);
    mountpoint_strong_ref_t mp;
    bool isatty;
    ssize_t (*read)(inode_strong_ref_t, void*, size_t, off_t);
    ssize_t (*write)(inode_strong_ref_t, void*, size_t, off_t);
} inode_create_params_t;

DEFINE_REFCOUNT_INC2(inode, inode_create_params_t)

void ___inode_free(inode_strong_ref_t inode);
inode_t* ___inode_create(inode_create_params_t params);
