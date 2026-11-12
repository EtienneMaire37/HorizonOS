#pragma once

#define _GNU_SOURCE
#include <dirent.h>
#include <sys/stat.h>
#include "../multicore/spinlock.h"
#include "vnode_ref.h"
#include "../multitasking/queue_def.h"

typedef struct open_file_descriptor open_file_descriptor_t;
typedef struct open_file_descriptor
{
    int flags;
    vnode_strong_ref_t vnode;
    struct stat st;
    _Atomic off_t offset;
    spinlock_noint_t lock;

    ssize_t (*read)(open_file_descriptor_t*, void*, size_t); 
    ssize_t (*write)(open_file_descriptor_t*, void*, size_t);

    thread_queue_t blocked_on_io;
    thread_queue_t blocked_on_poll;
} open_file_descriptor_t;

DEFINE_REFCOUNT_INC1(open_file_descriptor)

typedef struct
{
    int flags;
    vnode_strong_ref_t vnode;
    struct stat* st;
} open_file_descriptor_create_params_t;

DEFINE_REFCOUNT_INC2(open_file_descriptor, open_file_descriptor_create_params_t)

typedef struct
{
    int flags; // FD_CLOEXEC
    open_file_descriptor_strong_ref_t fd;
} open_file_descriptor_entry_t;

void ___open_file_descriptor_free(open_file_descriptor_strong_ref_t ofd);
open_file_descriptor_t* ___open_file_descriptor_create(open_file_descriptor_create_params_t params);

open_file_descriptor_strong_ref_t vfs_create_new_file_descriptor_from_vnode(int* _errno, int flags, vnode_strong_ref_t vnode, struct stat* st);
void vfs_free_open_file_descriptor(int ofd);

void vfs_log_thread_fds();
struct dirent64 vfs_find_new_child_entry(open_file_descriptor_strong_ref_t open_file);

