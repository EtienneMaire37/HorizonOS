#pragma once

#include <sys/stat.h>
#include "../inode_ref.h"
#include "../vnode_ref.h"
#include "../mountpoint_ref.h"
#include <sys/types.h>

ino_t initrd_generate_ino();
int initrd_explore(vnode_ref_t vnode);
ssize_t initrd_read(inode_ref_t inode, void* buf, size_t count, off_t offset);
ssize_t initrd_write(inode_ref_t inode, void* buf, size_t count, off_t offset);
int initrd_create(const char* name, vnode_ref_t parent, struct stat* st, void** data, void (**free_data)(void*));
void initrd_init(const char* path);
inode_ref_t initrd_create_root_inode(mountpoint_t* mp);
