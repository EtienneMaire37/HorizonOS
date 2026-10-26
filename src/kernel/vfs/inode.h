#pragma once

#include "mountpoint_ref.h"
#include "../util/ref.h"
#include "inode_ref.h"
#include "../multicore/spinlock.h"
#include <sys/stat.h>
#include <sys/types.h>

struct inode
{
    struct ref ref;
    spinlock_noint_t lock;

    mountpoint_ref_t mountpoint;

    void* fs_specific;
    void (*free_fs_specific_data)(void*);
    struct stat st;

    ssize_t (*read)(inode_ref_t, void*, size_t, off_t); 
    ssize_t (*write)(inode_ref_t, const void*, size_t, off_t);
    bool isatty;
};

#define INODE_REF_INIT ((struct ref){___inode_free, 1})

void ___inode_free(const struct ref* _ref);
inode_ref_t vfs_create_new_inode(const struct stat* st, void* fs_specific, void (*free_fs_specific_data)(void*), mountpoint_ref_t mp, bool isatty);
void inode_delete_ref(inode_ref_t* ref);
inode_ref_t inode_copy_ref(inode_ref_t ref);
