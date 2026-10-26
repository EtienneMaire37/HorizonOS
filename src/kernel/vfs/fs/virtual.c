#include "virtual.h"
#include "../inode.h"
#include "../mountpoint.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

ino_t virtfs_generate_ino()
{
    static ino_t num = 1;
    return num++;
}

int virtfs_explore(vnode_ref_t vnode)
{
    char path[PATH_MAX];
    vfs_get_relative_path_to_node_from_mountpoint(vnode, path, sizeof(path));
    int _errno;
    // We can safely assume the top mountpoint is the current one because the node is exploration locked
    mountpoint_ref_t mp = inode_stack_top_mountpoint(&vnode.ptr->inodes);
    vnode_ref_t root = { mp.ptr->data };
    vnode_ref_t node = vfs_get_vnode_from_path(&_errno, 0, 0, path, root, (vnode_ref_t){ NULL }, false);
    if (!node.ptr)
    {
        mountpoint_delete_ref(&mp);
        return _errno;
    }
    vnode_move_reference(&node, children);
    while (node.ptr)
    {
        inode_ref_t inode = inode_stack_top_inode(&node.ptr->inodes);
        vfs_add_new_child_node(vnode, node.ptr->name, &inode.ptr->st, NULL, NULL);
        inode_delete_ref(&inode);
        vnode_move_reference(&node, next);
    }
    mountpoint_delete_ref(&mp);
    
    return 0;
}

ssize_t virtfs_read(inode_ref_t inode, void* buf, size_t count, off_t offset)
{
    (void)inode;
    (void)buf;
    (void)count;
    (void)offset;
    return -ENOSYS;
}
ssize_t virtfs_write(inode_ref_t inode, void* buf, size_t count, off_t offset)
{
    (void)inode;
    (void)buf;
    (void)count;
    (void)offset;
    return -ENOSYS;
}
int virtfs_create(const char* name, vnode_ref_t parent, struct stat* st, void** data, void (**free_data)(void*))
{
    (void)data;
    (void)free_data;
    char path[PATH_MAX];
    vfs_get_relative_path_to_node_from_mountpoint(parent, path, sizeof(path));
    int _errno;
    mountpoint_ref_t mp = inode_stack_top_mountpoint(&parent.ptr->inodes);
    vnode_ref_t root = { mp.ptr->data };
    mountpoint_delete_ref(&mp);
    vnode_ref_t node = vfs_get_vnode_from_path(&_errno, 0, 0, path, root, (vnode_ref_t){ NULL }, false);
    if (!node.ptr) return _errno;
    st->st_ino = virtfs_generate_ino();
    vfs_add_new_child_node(node, name, st, NULL, NULL);
    vnode_delete_ref(&node);
    return 0;
}

void virtfs_flush(vnode_ref_t vnode)
{
    (void)vnode;
}
vnode_t* virtfs_create_data()
{
    struct stat st = {0};
    st.st_mode = S_IFDIR;

    inode_ref_t inode = vfs_create_new_inode(&st, NULL, NULL, (mountpoint_ref_t){ NULL }, false, NULL, NULL);
    vnode_ref_t ref = vfs_create_new_vnode("virtfs_root", inode);
    inode_delete_ref(&inode);
    return ref.ptr;
}
void virtfs_free_data(void* node_ref)
{
    if (!node_ref) return;
    vnode_ref_t ref = { node_ref };
    vfs_unload_children(ref);
    vnode_delete_ref(&ref);
}

inode_ref_t virtfs_create_root_inode(mountpoint_t* mp)
{
    mountpoint_ref_t mp_ref = (mountpoint_ref_t){ mp };
    struct stat st;
    st.st_atim = (struct timespec){ 0, 0 };
    st.st_ctim = (struct timespec){ 0, 0 };
    st.st_mtim = (struct timespec){ 0, 0 };
    st.st_blksize = 4096;
    st.st_blocks = 0;
    st.st_rdev = 0;

    st.st_gid = 0;
    st.st_uid = 0;

    st.st_ino = virtfs_generate_ino();
    st.st_mode = 0755 | S_IFDIR;

    st.st_nlink = 1;
    st.st_size = 0;

    inode_ref_t inode = vfs_create_new_inode(&st, NULL, NULL, mp_ref, false, NULL, NULL);
    if (!inode.ptr) return inode;
    inode.ptr->st.st_nlink = 1;
    return inode;
}
