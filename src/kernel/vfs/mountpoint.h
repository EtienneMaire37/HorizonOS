#pragma once

#include "mountpoint_ref.h"
#include "../multicore/spinlock.h"
#include "vnode_ref.h"
#include "inode_ref.h"
#include <sys/types.h>
#include <sys/stat.h>

struct mountpoint
{
    vnode_strong_ref_t root;
    dev_t dev;
    vnode_strong_ref_t dev_node;
    blksize_t blksize;

    // Stuff to fix race conditions
    spinlock_noint_t lock;
    bool unmounting;
    int busy;

    ino_t (*generate_ino)();
    inode_strong_ref_t (*create_root_inode)(mountpoint_t*);

    int (*explore)(vnode_strong_ref_t); 
    ssize_t (*read)(inode_strong_ref_t, void*, size_t, off_t); 
    ssize_t (*write)(inode_strong_ref_t, void*, size_t, off_t);
    int (*create)(const char*, vnode_strong_ref_t, struct stat*, void**, void (**)(void*));
    // TODO: Add more operations
    void (*flush)(vnode_strong_ref_t);

    void* data;
    void (*free_data)(void*);
};

typedef enum
{
    FSTYPE_VIRTUAL,
    FSTYPE_INITRD,
    FSTYPE_UNKNOWN
} fstype_t;
