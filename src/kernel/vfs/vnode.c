#include "vnode.h"
#include "explore.h"
#include "flags.h"
#include "inode_ref.h"
#include "inode_stack.h"
#include "mountpoint_ref.h"
#include "inode.h"
#include "../util/ref.h"
#include "../multicore/spinlock.h"
#include "../debug/out.h"
#include "../util/string.h"
#include "mountpoint.h"
#include "fs/virtual.h"
#include "fs/initrd.h"
#include "rdev.h"
#include "vnode_ref.h"
#include "../util/assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>

vnode_ref_t vfs_root_node = (vnode_ref_t){ NULL };
_Atomic size_t vfs_total_nodes = 0;

void vfs_create_root_node()
{
    LOG(DEBUG, "Creating root vnode and inode");
    
    struct stat st;
    st.st_atim = (struct timespec){ 0, 0 };
    st.st_ctim = (struct timespec){ 0, 0 };
    st.st_mtim = (struct timespec){ 0, 0 };
    st.st_blksize = 4096;
    st.st_blocks = 0;
    st.st_dev = 0;
    st.st_rdev = 0;

    st.st_gid = 0;
    st.st_uid = 0;

    st.st_ino = 1;
    st.st_mode = 0755 | S_IFDIR;

    st.st_nlink = 1;
    st.st_size = 0;

    inode_ref_t inode = vfs_create_new_inode(&st, NULL, NULL, (mountpoint_ref_t){ NULL }, false);
    vfs_root_node = vfs_create_new_vnode("/", inode);
    inode_delete_ref(&inode);

    LOG(DEBUG, "Done");
}

vnode_ref_t vfs_create_new_vnode(const char* name, inode_ref_t inode)
{
    ASSERT(name && inode.ptr);
    vnode_t* newn = calloc(1, sizeof(vnode_t));
    if (!newn) return (vnode_ref_t){ NULL };
    newn->name = strdup(name);
    if (!newn->name)
    {
        free(newn);
        return (vnode_ref_t){ NULL };
    }
    vfs_total_nodes++;
    inode_stack_push(&newn->inodes, inode);
    __sync_fetch_and_add(&newn->inodes.data[0].ptr->st.st_nlink, 1);
    newn->lock = SPINLOCK_NOINT_INIT;
    newn->flags = VNODE_INIT;
    newn->ref = VNODE_REF_INIT;
    return (vnode_ref_t){ newn };
}

void vnode_delete_ref(vnode_ref_t* ref)
{
    if (!ref) return;
    vnode_ref_t ref_ref = (vnode_ref_t){ __atomic_exchange_n((void**)&ref->ptr, 0, __ATOMIC_RELEASE ) };
    struct ref* ref_ref_ref = ref_ref.ptr ? &ref_ref.ptr->ref : NULL;
    if (ref_ref_ref)
        ref_dec(ref_ref_ref);
}

vnode_ref_t ___vnode_dereference_vnode(vnode_ref_t node, size_t field_offset, bool locked)
{
    ASSERT(node.ptr);
    uint32_t flags = locked ? 0 : acquire_spinlock_noint(&node.ptr->lock);
    vnode_t* field_value = *(vnode_t**)((uintptr_t)node.ptr + field_offset);
    if (field_value)
        ref_inc(&field_value->ref);
    if (!locked)
        release_spinlock_noint(&node.ptr->lock, flags);
    return (vnode_ref_t){ field_value };
}

void ___vnode_move_reference(vnode_ref_t* ref, size_t field_offset)
{
    ASSERT(ref && ref->ptr);
    vnode_ref_t new_ref = ___vnode_dereference_vnode(*ref, field_offset, false);
    vnode_delete_ref(ref);
    *ref = new_ref;
}

#define vadncne_ret(err) { release_spinlock_noint(&node.ptr->lock, flags); inode_delete_ref(&parent_inode); if (params.free_fs_specific_data && !params.inode.ptr) params.free_fs_specific_data(params.fs_specific); return (err); }
int _vfs_add_new_child_node_ex(vnode_ref_t node, const char* name, vfs_add_new_child_node_ex_params_t params)
{
    ASSERT(node.ptr);

    uint32_t flags = acquire_spinlock_noint(&node.ptr->lock);
    inode_ref_t parent_inode = inode_stack_top_inode(&node.ptr->inodes);
    ASSERT(parent_inode.ptr);
    if (!S_ISDIR(parent_inode.ptr->st.st_mode)) vadncne_ret(ENOTDIR);
    if (strchr(name, '/')) vadncne_ret(EINVAL);
    if (!*name) vadncne_ret(EINVAL);

    mountpoint_ref_t parent_mountpoint = mountpoint_copy_ref(parent_inode.ptr->mountpoint);

    params.st.st_blksize = parent_mountpoint.ptr ? parent_mountpoint.ptr->blksize : 4096;
    params.st.st_dev = parent_mountpoint.ptr ? parent_mountpoint.ptr->dev : (dev_t)-1;
    params.st.st_ino = params.st.st_ino;
    release_spinlock_noint(&node.ptr->lock, flags);
    inode_delete_ref(&parent_inode);

    inode_ref_t inode = params.inode.ptr ? (inode_ref_t){ NULL } : vfs_create_new_inode(&params.st, params.fs_specific, params.free_fs_specific_data, parent_mountpoint, params.isatty);
    mountpoint_delete_ref(&parent_mountpoint);
    vnode_ref_t child = vfs_create_new_vnode(name, inode.ptr ? inode : params.inode );
    inode_delete_ref(&inode);

    // * From this point on if we free correctly the "child" vnode fs_specific is cleaned up automatically
    // * That means we MUST NOT free it manually as it would cause a double free

    // NOTE: undef it just in case
#undef vadncne_ret
    if (!child.ptr)
    {
        LOG(WARNING, "Couldn't allocate child");
        vnode_delete_ref(&child);
        return ENOMEM;
    }
    child.ptr->flags = params.explored ? VNODE_EXPLORED : VNODE_INIT;

    vfs_explore(node);

    vnode_ref_t ref = vnode_dereference_vnode(node, children);
    flags = acquire_spinlock_noint(&node.ptr->lock);

    vnode_ref_t test_ref = vnode_copy_ref(ref);
    while (test_ref.ptr)
    {
        if (strcmp(test_ref.ptr->name, name) == 0)
        {
            release_spinlock_noint(&node.ptr->lock, flags);
            vnode_delete_ref(&test_ref);
            vnode_delete_ref(&child);
            vnode_delete_ref(&ref);
            return EEXIST;
        }
        vnode_move_reference(&test_ref, next);
    }

    child.ptr->next = ref.ptr;
    child.ptr->parent = node.ptr;
    node.ptr->children.ptr = child.ptr;

    release_spinlock_noint(&node.ptr->lock, flags);

    vnode_delete_ref(&ref);

    return 0;
}
int vfs_add_new_child_node(vnode_ref_t node, const char* name, const struct stat* st, void* fs_specific, void (*free_fs_specific_data)(void*))
{
    return vfs_add_new_child_node_ex(node, name, .inode = (inode_ref_t){ NULL }, .st = *st, .fs_specific = fs_specific, .free_fs_specific_data = free_fs_specific_data);
}
int vfs_add_new_child_node__hardlink(vnode_ref_t node, const char* name, inode_ref_t inode)
{
    return vfs_add_new_child_node_ex(node, name, .inode = inode, .st = (struct stat){});
}
int vfs_add_new_special_child_node(vnode_ref_t node, const char* name, mode_t mode, uid_t uid, gid_t gid,
    ssize_t (*read)(vnode_ref_t, void*, size_t, off_t), ssize_t (*write)(vnode_ref_t, const void*, size_t, off_t),
    void* fs_specific, void (*free_fs_specific_data)(void*))
{
    ASSERT(S_ISBLK(mode) || S_ISCHR(mode));
    return vfs_add_new_child_node_ex(node, name, .inode = (inode_ref_t){ NULL }, .st = (struct stat){.st_dev = 0, .st_mode = mode, .st_uid = uid, .st_gid = gid, .st_rdev = vfs_generate_rdev(), .st_size = 0, .st_blocks = 0, .st_atim = {0, 0}, .st_mtim = {0, 0}, .st_ctim = {0, 0}}, .read = read, .write = write, .fs_specific = fs_specific, .free_fs_specific_data = free_fs_specific_data);
}

void _vfs_unload_children(vnode_ref_t node, bool locked, bool exploration_locked)
{
    if (!node.ptr)
    {
        LOG(WARNING, "Tried to unload a null vnode");
        return;
    }

    uint32_t flags = locked ? 0 : acquire_spinlock_noint(&node.ptr->lock);
    uint32_t flags2 = exploration_locked ? 0 : acquire_spinlock_noint(&node.ptr->exploration_lock);
    vnode_ref_t child = vnode_dereference_vnode_locked(node, children);
    node.ptr->children.ptr = NULL;
    node.ptr->flags &= ~VNODE_EXPLORED;
    while (child.ptr)
    {
        ref_dec(&child.ptr->ref);
        vnode_ref_t old_child = vnode_copy_ref(child);
        vnode_move_reference(&child, next);
        old_child.ptr->next = NULL;
        vnode_delete_ref(&old_child);
    }
    if (!exploration_locked)
        release_spinlock_noint(&node.ptr->exploration_lock, flags2);
    if (!locked)
        release_spinlock_noint(&node.ptr->lock, flags);
}

void vfs_unparent_children(vnode_ref_t node)
{
    ASSERT(node.ptr);

    vnode_ref_t child = vnode_dereference_vnode(node, children);
    while (child.ptr)
    {
        child.ptr->parent = NULL;
        vnode_move_reference(&child, next);
    }
}

int vfs_unmount(vnode_ref_t ref, bool lazy)
{
    ASSERT(ref.ptr);
    uint32_t flags = acquire_spinlock_noint(&ref.ptr->lock);
    mountpoint_ref_t mp = inode_stack_top_mountpoint(&ref.ptr->inodes);
    if (!mp.ptr)
    {
        release_spinlock_noint(&ref.ptr->lock, flags);
        return EINVAL;
    }
    if (mp.ptr->root.ptr != ref.ptr)
    {
        mountpoint_delete_ref(&mp);
        release_spinlock_noint(&ref.ptr->lock, flags);
        return EINVAL;
    }
    uint32_t mp_flags = acquire_spinlock_noint(&mp.ptr->lock);
    if (mp.ptr->busy && !lazy)
    {
        mountpoint_delete_ref(&mp);
        release_spinlock_noint(&ref.ptr->lock, flags);
        release_spinlock_noint(&mp.ptr->lock, mp_flags);
        return EBUSY;
    }
    mp.ptr->unmounting = true;
    release_spinlock_noint(&mp.ptr->lock, mp_flags);
    uint32_t flags2 = acquire_spinlock_noint(&ref.ptr->exploration_lock);
    inode_ref_t poped = inode_stack_pop(&ref.ptr->inodes);
    inode_delete_ref(&poped);
    vfs_unload_children_locked_exploration_locked(ref);
    release_spinlock_noint(&ref.ptr->exploration_lock, flags2);
    release_spinlock_noint(&ref.ptr->lock, flags);
    mountpoint_delete_ref(&mp);
    return 0;
}

void ___vnode_free(const struct ref* _ref)
{
    vnode_t* node = container_of(_ref, vnode_t, ref);
    LOG(TRACE, "Freeing vnode \"%s\"", node->name);
    if (node == vfs_root_node.ptr)
    {
        LOG(CRITICAL, "Tried to free root node");
        abort();
    }
    vnode_ref_t ref = { node };
    vfs_unparent_children(ref);
    vfs_unload_children(ref);

    inode_ref_t inode = inode_stack_pop(&node->inodes);
    while (inode.ptr)
    {
        LOG(TRACE, "Freeing inode and mountpoint");
        __sync_fetch_and_sub(&inode.ptr->st.st_nlink, 1);
        inode_delete_ref(&inode);
        inode = inode_stack_pop(&node->inodes);
    }
    free(node->inodes.data);
    free(node->name);
    free(node);
    vfs_total_nodes--;
}

struct stat vnode_stat(vnode_ref_t vnode)
{
    ASSERT(vnode.ptr);
    inode_ref_t inode = inode_stack_top_inode(&vnode.ptr->inodes);
    if (!inode.ptr)
        // Default unmounted root stat
    {
        struct stat st;
        st.st_atim = (struct timespec){ 0, 0 };
        st.st_ctim = (struct timespec){ 0, 0 };
        st.st_mtim = (struct timespec){ 0, 0 };
        st.st_blksize = 4096;
        st.st_blocks = 0;
        st.st_dev = 0;
        st.st_rdev = 0;

        st.st_gid = 0;
        st.st_uid = 0;

        st.st_ino = 1;
        st.st_mode = 0755 | S_IFDIR;

        st.st_nlink = 1;
        st.st_size = 0;
        return st;
    }
    struct stat st = inode.ptr->st;
    inode_delete_ref(&inode);
    return st;
}

void vfs_log_structure_helper(vnode_ref_t node, int depth)
{
    ASSERT(node.ptr);

    struct stat st = vnode_stat(node);
    mountpoint_ref_t mp = inode_stack_top_mountpoint(&node.ptr->inodes);
    LOG(DEBUG, "%*s- \"%s\" (inode %ld) [%d hardlinks] [%d references]%s%s", depth, "",
        node.ptr->name, (long)st.st_ino,
        (int)st.st_nlink,
        node.ptr->ref.count,
        ((node.ptr->flags & VNODE_EXPLORED) || (!S_ISDIR(st.st_mode))) ? "" : " <NOT EXPLORED>",
        (mp.ptr && mp.ptr->root.ptr == node.ptr) ? " <MOUNTPOINT>" : ""
    );
    mountpoint_delete_ref(&mp);
    vnode_ref_t child = vnode_dereference_vnode(node, children);
    while (child.ptr)
    {
        vfs_log_structure_helper(child, depth + 4);
        vnode_move_reference(&child, next);
    }
}

void vfs_log_structure(vnode_ref_t node)
{
    vfs_log_structure_helper(node, 0);
}

size_t vfs_get_absolute_path_to_node_helper(vnode_ref_t ref, char* buf, size_t bufsiz)
{
    vnode_t* node = ref.ptr;
    ASSERT(node);
    size_t offset = 0;
    vnode_ref_t parent_ref = vnode_dereference_vnode(ref, parent);
    if (parent_ref.ptr)
    {
        offset = vfs_get_absolute_path_to_node_helper(parent_ref, buf, bufsiz);
        vnode_delete_ref(&parent_ref);
        if (offset < bufsiz)
        {
            int maxwrite = bufsiz - offset - 1; // -1 to take the / into account
            int len = strlen(node->name);
            offset += snprintf(&buf[offset], bufsiz, "/%*s", len > maxwrite ? maxwrite : len, node->name);
        }
    }
    return offset;
}

size_t vfs_get_absolute_path_to_node(vnode_ref_t ref, char* buf, size_t bufsiz)
{
    vnode_t* node = ref.ptr;
    ASSERT(bufsiz > 2);
    ASSERT(buf);
    ASSERT(node);
    size_t ret;
    if (node->parent)
        buf[(ret = vfs_get_absolute_path_to_node_helper(ref, buf, bufsiz - 1) + 1)] = 0;
    else
        strcpy(buf, (ret = 1, "/"));
    return ret;
}

size_t vfs_get_relative_path_to_node_from_mountpoint_helper(vnode_ref_t root_node, vnode_ref_t ref, char* buf, size_t bufsiz)
{
    vnode_t* node = ref.ptr;
    ASSERT(node);
    size_t offset = 0;
    vnode_ref_t parent_ref = vnode_dereference_vnode(ref, parent);
    if (parent_ref.ptr && root_node.ptr != ref.ptr)
    {
        offset = vfs_get_relative_path_to_node_from_mountpoint_helper(root_node, parent_ref, buf, bufsiz);
        vnode_delete_ref(&parent_ref);
        if (offset < bufsiz)
        {
            int maxwrite = bufsiz - offset - 1; // -1 to take the / into account
            int len = strlen(node->name);
            offset += snprintf(&buf[offset], bufsiz, "/%*s", len > maxwrite ? maxwrite : len, node->name);
        }
    }
    vnode_delete_ref(&parent_ref);
    return offset;
}

size_t vfs_get_relative_path_to_node_from_mountpoint(vnode_ref_t ref, char* buf, size_t bufsiz)
{
    vnode_t* node = ref.ptr;
    ASSERT(bufsiz > 2);
    ASSERT(buf);
    ASSERT(node);
    size_t ret;
    // BUG: Can return invalid paths on a file which was on an unmounted mountpoint,
    // as its parents can be unloaded before it.
    // shouldn't cause any real problems though (?)
    // TODO: Handle this cleanly
    if (node->parent)
    {
        mountpoint_ref_t mp = inode_stack_top_mountpoint(&node->inodes);
        vnode_ref_t root = vnode_copy_ref(mp.ptr->root);
        mountpoint_delete_ref(&mp);
        buf[(ret = vfs_get_relative_path_to_node_from_mountpoint_helper(root, ref, buf, bufsiz - 1) + 1)] = 0;
        vnode_delete_ref(&root);
    }
    else
        strcpy(buf, (ret = 0, ""));
    return ret;
}

size_t vfs_count_nodes(vnode_ref_t ref)
{
    vnode_t* node = ref.ptr;
    ASSERT(node);
    size_t total = 0;
    vnode_ref_t child = vnode_dereference_vnode(ref, children);
    while (child.ptr)
    {
        total++;
        total += vfs_count_nodes(child);
        vnode_move_reference(&child, next);
    }
    return total;
}

bool vfs_verify_tree_integrity()
{
    size_t nodes = vfs_count_nodes(vfs_root_node) + 1;
    bool ret = nodes == vfs_total_nodes;
    if (!ret)
    {
        LOG(ERROR, "Total refcounted nodes: %zu", vfs_total_nodes);
        LOG(ERROR, "Total nodes in tree: %zu", nodes);
    }
    return ret;
}

vnode_ref_t vnode_copy_ref(vnode_ref_t ref)
{
    if (!ref.ptr) return ref;
    ref_inc(&ref.ptr->ref);
    return ref;
}

vnode_ref_t vfs_get_vnode_from_path(int* _errno, uid_t uid, gid_t gid, const char* path, vnode_ref_t root, vnode_ref_t cwd, bool follow_symlinks)
{
    // TODO: Cache path --> vnode in a hashmap (and then iterate on successive parents for permissions)
    ASSERT(_errno);
    ASSERT(path);
    if (!root.ptr) root = vfs_root_node;
    *_errno = 0;
    ASSERT(root.ptr);
    vnode_ref_t ecwd = (cwd.ptr && !S_ISDIR(vnode_stat(cwd).st_mode)) ? vnode_dereference_vnode(cwd, parent) : (cwd.ptr ? vnode_copy_ref(cwd) : vnode_copy_ref(root));
    bool absolute_path = *path == '/';
    while (*path == '/') path++;
    if (!*path)
    {
        vnode_ref_t ret = absolute_path ? vnode_copy_ref(root) : vnode_copy_ref(ecwd);
        vnode_delete_ref(&ecwd);
        ASSERT(ret.ptr);
        return ret;
    }
    vnode_ref_t current = (ecwd.ptr && !absolute_path) ? vnode_copy_ref(ecwd) : vnode_copy_ref(root);
    if ((*_errno = vfs_explore(current)))
    {
        vnode_delete_ref(&ecwd);
        vnode_delete_ref(&current);
        return (vnode_ref_t){ NULL };
    }
    struct stat cur_stat = vnode_stat(current);
    bool x = (uid == cur_stat.st_uid) ? (cur_stat.st_mode & S_IXUSR) :
            ((gid == cur_stat.st_gid) ? (cur_stat.st_mode & S_IXGRP) :
                                             (cur_stat.st_mode & S_IXOTH));
    if (uid != 0 && !x)
    {
        vnode_delete_ref(&ecwd);
        vnode_delete_ref(&current);
        *_errno = EACCES;
        return (vnode_ref_t){ NULL };
    }

    vnode_ref_t child = vnode_dereference_vnode(current, children);
    size_t len = strlen_slash(path);
    int fake_entries = 2;
    while (fake_entries || child.ptr)
    {
        if (fake_entries)
        {
            if (fake_entries == 2)
            {
                if (strcmp_slash(".", path) == 0)
                {
                    path += len;
                    while (*path == '/')
                        path++;
                    if (!*path)
                    {
                        vnode_delete_ref(&ecwd);
                        // vnode_delete_ref(&current);
                        return current;
                    }
                    len = strlen_slash(path);
                    fake_entries = 2;
                    continue;
                }
            }
            else // fake_entries == 1
            {
                if (strcmp_slash("..", path) == 0)
                {
                    path += len;
                    while (*path == '/')
                        path++;
                    if (current.ptr != root.ptr)
                    {
                        vnode_move_reference(&current, parent);
                        cur_stat = vnode_stat(current);
                        bool x = (uid == cur_stat.st_uid) ? (cur_stat.st_mode & S_IXUSR) :
                                ((gid == cur_stat.st_gid) ? (cur_stat.st_mode & S_IXGRP) :
                                                                 (cur_stat.st_mode & S_IXOTH));
                        if (uid != 0 && !x)
                        {
                            vnode_delete_ref(&ecwd);
                            vnode_delete_ref(&current);
                            *_errno = EACCES;
                            return (vnode_ref_t){ NULL };
                        }
                        vnode_delete_ref(&child);
                        child = vnode_dereference_vnode(current, children);
                    }
                    if (!*path)
                    {
                        vnode_delete_ref(&ecwd);
                        return current;
                    }
                    len = strlen_slash(path);
                    fake_entries = 2;
                    continue;
                }
            }
            fake_entries--;
            continue;
        }
        if (strcmp_slash(child.ptr->name, path) == 0)
        {
            path += len;
            while (*path == '/')
                path++;
            vnode_delete_ref(&current);
            cur_stat = vnode_stat(child);
            if ((*path && !S_ISDIR(cur_stat.st_mode)) || (!*path && S_ISLNK(cur_stat.st_mode) && follow_symlinks))
            {
                if (*path && !S_ISLNK(cur_stat.st_mode))
                {
                    vnode_delete_ref(&ecwd);
                    vnode_delete_ref(&current);
                    *_errno = ENOTDIR;
                    return (vnode_ref_t){ NULL };
                }
                vnode_ref_t _child = child;
                char _path[PATH_MAX];
                ssize_t ret;
                if ((ret = vnode_read(child, _path, sizeof(_path) - 1, 0)) < 0)
                {
                    vnode_delete_ref(&ecwd);
                    vnode_delete_ref(&current);
                    *_errno = -ret;
                    return (vnode_ref_t){ NULL };
                }
                _path[ret] = 0;
                child = vfs_get_vnode_from_path(_errno, uid, gid, _path, root, _child, follow_symlinks);
                vnode_delete_ref(&_child);
            }
            if (!child.ptr)
            {
                vnode_delete_ref(&ecwd);
                vnode_delete_ref(&current);
                *_errno = ENOENT;
                return (vnode_ref_t){ NULL };
                
            }
            if (!*path)
            {
                vnode_delete_ref(&ecwd);
                vnode_delete_ref(&current);
                return child;
            }
            cur_stat = vnode_stat(child);
            bool x = (uid == cur_stat.st_uid) ? (cur_stat.st_mode & S_IXUSR) :
                    ((gid == cur_stat.st_gid) ? (cur_stat.st_mode & S_IXGRP) :
                                                     (cur_stat.st_mode & S_IXOTH));
            if (uid != 0 && !x)
            {
                vnode_delete_ref(&ecwd);
                vnode_delete_ref(&current);
                *_errno = EACCES;
                return (vnode_ref_t){ NULL };
            }
            current = child;
            if ((*_errno = vfs_explore(current)))
            {
                vnode_delete_ref(&ecwd);
                vnode_delete_ref(&current);
                return (vnode_ref_t){ NULL };
            }
            child = vnode_dereference_vnode(current, children);
            len = strlen_slash(path);
            fake_entries = 2;
            continue;
        }
        vnode_move_reference(&child, next);
    }
    vnode_delete_ref(&current);
    vnode_delete_ref(&ecwd);
    *_errno = ENOENT;
    return (vnode_ref_t){ NULL };
}

ssize_t vnode_read(vnode_ref_t ref, void* buf, size_t bytes, off_t offset)
{
    vnode_t* node = ref.ptr;
    if (!node) return -ENOENT;
    inode_ref_t inode = inode_stack_top_inode(&node->inodes);
    ssize_t ret = ENOSYS;
    if (!inode.ptr->read) goto end;
    ret = inode.ptr->read(inode, buf, bytes, offset);

end:
    inode_delete_ref(&inode);
    return ret;
}

ssize_t vnode_write(vnode_ref_t ref, const void* buf, size_t bytes, off_t offset)
{
    vnode_t* node = ref.ptr;
    if (!node) return -ENOENT;
    inode_ref_t inode = inode_stack_top_inode(&node->inodes);
    ssize_t ret = ENOSYS;
    if (!inode.ptr->write) goto end;
    ret = inode.ptr->write(inode, buf, bytes, offset);

end:
    inode_delete_ref(&inode);
    return ret;
}

int vfs_mount(vnode_ref_t ref, vnode_ref_t dev, const char* fstype)
{
    if (!S_ISDIR(vnode_stat(ref).st_mode))
        return ENOTDIR;

    fstype_t fstype_en;
    if (strcmp(fstype, "virt") == 0)
        fstype_en = FSTYPE_VIRTUAL;
    else if (strcmp(fstype, "initrd") == 0)
        fstype_en = FSTYPE_INITRD;
    else
        return ENODEV;

    bool need_vnode = fstype_en != FSTYPE_VIRTUAL && fstype_en != FSTYPE_INITRD;
    if (!ref.ptr || (!dev.ptr && need_vnode)) return ENOENT;
    if (need_vnode)
        if (!S_ISBLK(vnode_stat(dev).st_mode))
            return ENOTBLK;
    mountpoint_t* mountpoint = calloc(1, sizeof(mountpoint_t));
    if (!mountpoint)
        return ENOMEM;
    mountpoint->ref = MOUNTPOINT_REF_INIT;
    mountpoint->dev = dev.ptr ? vnode_stat(dev).st_rdev : vfs_generate_rdev();
    mountpoint->dev_node = vnode_copy_ref(dev);
    mountpoint->root = vnode_copy_ref(ref);
    switch (fstype_en)
    {
    case FSTYPE_VIRTUAL:
        mountpoint->generate_ino = virtfs_generate_ino;
        mountpoint->create_root_inode = virtfs_create_root_inode;
        mountpoint->explore = virtfs_explore;
        mountpoint->read = virtfs_read;
        mountpoint->write = virtfs_write;
        mountpoint->create = virtfs_create;
        mountpoint->flush = virtfs_flush;
        mountpoint->data = virtfs_create_data();
        mountpoint->free_data = virtfs_free_data;
        break;
    case FSTYPE_INITRD:
        mountpoint->generate_ino = initrd_generate_ino;
        mountpoint->create_root_inode = initrd_create_root_inode;
        mountpoint->explore = initrd_explore;
        mountpoint->read = initrd_read;
        mountpoint->write = initrd_write;
        mountpoint->create = initrd_create;
        break;

    default:
        ;
    }
    uint32_t flags = acquire_spinlock_noint(&ref.ptr->lock);
    uint32_t flags2 = acquire_spinlock_noint(&ref.ptr->exploration_lock);
    vfs_unload_children_locked_exploration_locked(ref);

    inode_ref_t new_inode = mountpoint->create_root_inode(mountpoint);
    
    inode_stack_push(&ref.ptr->inodes, new_inode);
    inode_delete_ref(&new_inode);
    mountpoint_ref_t mp_ref = { mountpoint };
    mountpoint_delete_ref(&mp_ref);

    release_spinlock_noint(&ref.ptr->exploration_lock, flags2);
    release_spinlock_noint(&ref.ptr->lock, flags);
    return 0;
}

int _vfs_create(const char* name, vnode_ref_t parent, mode_t mode, uid_t uid, gid_t gid, vfs_create_params_t params)
{
    if (!parent.ptr) return ENOENT;
    ASSERT(S_ISDIR(mode) || S_ISREG(mode) || S_ISBLK(mode) || S_ISCHR(mode) || S_ISFIFO(mode) || S_ISLNK(mode));
    struct stat st;
    st.st_mode = mode;
    st.st_rdev = (S_ISCHR(mode) || S_ISBLK(mode)) ? vfs_generate_rdev() : 0;
    st.st_uid = uid;
    st.st_gid = gid;
    st.st_size = params.size;
    st.st_blocks = (st.st_size + 511) / 512;
    // TODO: Actually set the time
    st.st_atim = (struct timespec){ 0, 0 };
    st.st_ctim = (struct timespec){ 0, 0 };
    st.st_mtim = (struct timespec){ 0, 0 };
    uint32_t flags = acquire_spinlock_noint(&parent.ptr->lock);
    mountpoint_ref_t mountpoint = inode_stack_top_mountpoint(&parent.ptr->inodes);
    uint32_t flags2 = acquire_spinlock_noint(&parent.ptr->exploration_lock);
    release_spinlock_noint(&parent.ptr->lock, flags2);
    if (!mountpoint.ptr) 
    {
        release_spinlock_noint(&parent.ptr->exploration_lock, flags);
        return 0;
    }
    vfs_explore_exploration_locked(parent);
    int ret = ENOSYS;
    void* fs_specific = NULL;
    void (*free_fs_specific_data)(void*) = NULL;
    if (mountpoint.ptr->create) ret = mountpoint.ptr->create(name, parent, &st, &fs_specific, &free_fs_specific_data);
    mountpoint_delete_ref(&mountpoint);
    if (ret)
    {
        release_spinlock_noint(&parent.ptr->exploration_lock, flags);
        return ret;
    }
    LOG(TRACE, "Adding vnode to tree...");
    ret = vfs_add_new_child_node_ex(parent, name, (inode_ref_t){ NULL }, st, .fs_specific = fs_specific, .free_fs_specific_data = free_fs_specific_data, .explored = true, .isatty = params.isatty);
    release_spinlock_noint(&parent.ptr->exploration_lock, flags);
    return ret;
}
