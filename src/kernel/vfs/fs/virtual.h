#pragma once

#include <sys/stat.h>
#include "../vnode.h"

ino_t virtfs_generate_ino();
int virtfs_explore(vnode_ref_t vnode);
ssize_t virtfs_read(inode_ref_t inode, void* buf, size_t count, off_t offset);
ssize_t virtfs_write(inode_ref_t inode, void* buf, size_t count, off_t offset);
int virtfs_create(const char* name, vnode_ref_t parent, struct stat* st, void** data, void (**free_data)(void*));
void virtfs_flush(vnode_ref_t vnode);
vnode_t* virtfs_create_data();
void virtfs_free_data(void* node_ref);
inode_ref_t virtfs_create_root_inode(mountpoint_t* mp);
