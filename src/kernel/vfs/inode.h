#pragma once

#include "mountpoint_ref.h"
#include "inode_ref.h"
#include "../multicore/spinlock.h"
#include <sys/stat.h>
#include <sys/types.h>

struct inode
{
    spinlock_noint_t lock;

    mountpoint_strong_ref_t mountpoint;

    void* fs_specific;
    void (*free_fs_specific_data)(void*);
    struct stat st;

    ssize_t (*read)(inode_strong_ref_t, void*, size_t, off_t); 
    ssize_t (*write)(inode_strong_ref_t, void*, size_t, off_t);
    bool isatty;
};
