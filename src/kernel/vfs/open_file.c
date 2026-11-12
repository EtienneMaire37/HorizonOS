#include "open_file.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <threads.h>
#include "dirent.h"
#include "explore.h"
#include "inode_stack.h"
#include "mountpoint.h"
#include "vnode.h"
#include "vnode_ref.h"
#include "../multicore/spinlock.h"
#include "../util/assert.h"
#include "../multitasking/queue.h"
#include "../util/lambda.h"
#include "inode.h"
#include "open_file_descriptor_vector.h"

ssize_t ofd_vnode_read(open_file_descriptor_t* ofd, void* buf, size_t bytes)
{
    ASSERT(ofd);
    ASSERT(ofd->vnode.ptr);
    inode_strong_ref_t inode = inode_stack_top_inode(&ofd->vnode.ptr->ptr->inodes);
    if (!inode.ptr) return -EBADF;
    if (S_ISDIR(inode.ptr->ptr->st.st_mode)) return -EISDIR;
    ASSERT(inode.ptr->ptr->read);
    ssize_t ret = inode.ptr->ptr->read(inode, buf, bytes, ofd->offset);
    if (ret > 0) ofd->offset += ret;
    inode_delete_strong_ref(&inode);
    return ret;
}
ssize_t ofd_vnode_write(open_file_descriptor_t* ofd, void* buf, size_t bytes)
{
    ASSERT(ofd);
    ASSERT(ofd->vnode.ptr);
    inode_strong_ref_t inode = inode_stack_top_inode(&ofd->vnode.ptr->ptr->inodes);
    if (!inode.ptr) return -EBADF;
    if (S_ISDIR(inode.ptr->ptr->st.st_mode)) return -EISDIR;
    ASSERT(inode.ptr->ptr->write);
    ssize_t ret = inode.ptr->ptr->write(inode, buf, bytes, ofd->offset);
    if (ret > 0) ofd->offset += ret;
    inode_delete_strong_ref(&inode);
    return ret;
}

open_file_descriptor_strong_ref_t vfs_create_new_file_descriptor_from_vnode(int* _errno, int flags, vnode_strong_ref_t vnode, struct stat* _st)
{
    ASSERT(_errno);
    ASSERT(vnode.ptr);
    *_errno = 0;
    struct stat st = _st ? *_st : vnode_stat(vnode);
    open_file_descriptor_strong_ref_t ofd = open_file_descriptor_create((open_file_descriptor_create_params_t){ flags, vnode, &st });
    if (!ofd.ptr)
    {
        *_errno = ENOMEM;
        return ofd;
    }
    mountpoint_strong_ref_t mp = inode_stack_top_mountpoint(&vnode.ptr->ptr->inodes);
    ofd.ptr->ptr->read = ofd_vnode_read;
    ofd.ptr->ptr->write = ofd_vnode_write;
    if (mp.ptr)
    {
        uint32_t eflags = acquire_spinlock_noint(&mp.ptr->ptr->lock);
        bool unmounting = mp.ptr->ptr->unmounting;
        if (!unmounting)
            mp.ptr->ptr->busy++;
        release_spinlock_noint(&mp.ptr->ptr->lock, eflags);
        mountpoint_delete_strong_ref(&mp);
        if (unmounting)
        {
            open_file_descriptor_delete_strong_ref(&ofd);
            *_errno = ENOENT;
            return ofd;
        }
    }
    return ofd;
}

void ___open_file_descriptor_free(open_file_descriptor_strong_ref_t _ofd)
{
    open_file_descriptor_t* ofd = _ofd.ptr->ptr;
    if (ofd->vnode.ptr)
    {
        mountpoint_strong_ref_t mp = inode_stack_top_mountpoint(&ofd->vnode.ptr->ptr->inodes);
        if (mp.ptr)
        {
            uint32_t eflags = acquire_spinlock_noint(&mp.ptr->ptr->lock);
            mp.ptr->ptr->busy--;
            release_spinlock_noint(&mp.ptr->ptr->lock, eflags);
            mountpoint_delete_strong_ref(&mp);
        }
        vnode_delete_strong_ref(&ofd->vnode);
    }
    uint32_t flags = lock_scheduler();
    __move_all_tasks_to_running_queue(&ofd->blocked_on_io);
    __run_it_on_queue(&ofd->blocked_on_poll, lambda(void, (thread_t* thread)
    {
        ll_remove(&thread->_poll_tqs, ll_find_item_by_data(&thread->_poll_tqs, &ofd->blocked_on_poll));
        __task_stop_polling(thread);
    }));
    __remove_all_tasks_from_queue(&ofd->blocked_on_poll);    free(ofd);
    unlock_scheduler(flags);
    free(ofd);
}

struct dirent64 create_dirent_from_vnode(vnode_strong_ref_t cur, off_t cur_index, const char* name)
{
    struct dirent64 ret;
    struct stat st = vnode_stat(cur);
    ret.d_ino = st.st_ino;
    ret.d_off = cur_index;
    ret.d_reclen = sizeof(struct dirent64);
    if      (S_ISREG(st.st_mode))
        ret.d_type = DT_REG;
    else if (S_ISDIR(st.st_mode))
        ret.d_type = DT_DIR;
    else if (S_ISBLK(st.st_mode))
        ret.d_type = DT_BLK;
    else if (S_ISCHR(st.st_mode))
        ret.d_type = DT_CHR;
    else if (S_ISFIFO(st.st_mode))
        ret.d_type = DT_FIFO;
    else if (S_ISSOCK(st.st_mode))
        ret.d_type = DT_SOCK;
    else if (S_ISLNK(st.st_mode))
        ret.d_type = DT_LNK;
    else
        ret.d_type = DT_UNKNOWN;
    // Lock to avoid use after free when renaming node
    uint32_t flags = acquire_spinlock_noint(&cur.ptr->ptr->lock);
    snprintf(ret.d_name, 255, "%s", name);
    release_spinlock_noint(&cur.ptr->ptr->lock, flags);
    ret.d_name[255] = 0;
    return ret;
}

struct dirent64 vfs_find_new_child_entry(open_file_descriptor_strong_ref_t open_file)
{
    ASSERT(open_file.ptr);
    ASSERT(open_file.ptr->ptr->vnode.ptr);
    vfs_explore(open_file.ptr->ptr->vnode);
    vnode_strong_ref_t cur = vnode_dereference_vnode(open_file.ptr->ptr->vnode, children);
    off_t cur_index = 0;
    while (cur.ptr)
    {
        // Not really that important to make it atomic tbf
        if (cur_index == open_file.ptr->ptr->offset)
        {
            open_file.ptr->ptr->offset++;
            struct dirent64 ret = create_dirent_from_vnode(cur, cur_index, cur.ptr->ptr->name);
            vnode_delete_strong_ref(&cur);
            return ret;
        }
        vnode_move_reference(&cur, next);
        cur_index++;
    }
    if (open_file.ptr->ptr->offset == cur_index + 0)
        // .
    {
        open_file.ptr->ptr->offset++;
        return create_dirent_from_vnode(open_file.ptr->ptr->vnode, cur_index + 0, ".");
    }
    if (open_file.ptr->ptr->offset == cur_index + 1)
        // ..
    {
        open_file.ptr->ptr->offset++;
        vnode_strong_ref_t parent = vnode_dereference_vnode(open_file.ptr->ptr->vnode, parent);
        if (!parent.ptr) parent = vnode_dup_strong_ref(open_file.ptr->ptr->vnode);
        struct dirent64 ret = create_dirent_from_vnode(parent, cur_index + 1, "..");
        vnode_delete_strong_ref(&parent);
        return ret;
    }
    if (open_file.ptr->ptr->offset == cur_index + 2)
        // EOF
    {
        open_file.ptr->ptr->offset++;
        return (struct dirent64){ .d_name = { 0 } };
    }
    open_file.ptr->ptr->offset = 0;
    return (struct dirent64){ .d_name = { 0 } };
}
