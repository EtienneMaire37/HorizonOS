#include "initrd.h"
#include "../inode.h"
#include "../vnode.h"
#include "../../initrd/initrd.h"
#include "../../util/assert.h"
#include "../../util/string.h"
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

ino_t initrd_generate_ino()
{
    static ino_t num = 1;
    return num++;
}

int initrd_explore(vnode_ref_t vnode)
{
    char path[PATH_MAX];
    vfs_get_relative_path_to_node_from_mountpoint(vnode, path, sizeof(path));
    for (size_t i = 0; i < initrd_files_count; i++)
    {
        if (str_starts_with(initrd_files[i].name, path))
        {
            const char* node_name = &initrd_files[i].name[strlen(path) + 1];
            size_t* fs_specific = malloc(sizeof(size_t));
            *fs_specific = i;
            vfs_add_new_child_node(vnode, node_name, &initrd_files[i].st, fs_specific, free);
        }
    }
    return 0;
}
ssize_t initrd_read(inode_ref_t inode, void* buf, size_t count, off_t offset)
{
    ASSERT(inode.ptr);
    if (offset < 0) return -EINVAL;
    initrd_file_t* file = &initrd_files[*(size_t*)inode.ptr->fs_specific];
    size_t filesize =    S_ISLNK(file->st.st_mode) ? strlen(file->link) :
                        (S_ISREG(file->st.st_mode) ? (size_t)file->st.st_size : 0);
    if ((size_t)offset >= filesize)
        return 0;
    if ((size_t)(offset + count) > filesize)
        count = filesize - offset;
    memcpy(buf, (S_ISREG(file->st.st_mode) ? file->data : (uint8_t*)file->link) + offset, count);
    return count;
}
ssize_t initrd_write(inode_ref_t inode, const void* buf, size_t count, off_t offset)
{
    (void)inode;
    (void)buf;
    (void)count;
    (void)offset;
    return -EROFS;
}

int initrd_create(const char* name, vnode_ref_t parent, struct stat* st, void** data, void (**free_data)(void*))
{
    (void)name;
    (void)parent;
    (void)st;
    (void)data;
    (void)free_data;
    return EROFS;
}

inode_ref_t initrd_create_root_inode(mountpoint_t* mp)
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

    st.st_ino = initrd_generate_ino();
    st.st_mode = 0755 | S_IFDIR;

    st.st_size = 0;

    inode_ref_t inode = vfs_create_new_inode(&st, NULL, NULL, mp_ref, false);
    if (!inode.ptr) return inode;
    inode.ptr->st.st_nlink = 1;
    return inode;
}
