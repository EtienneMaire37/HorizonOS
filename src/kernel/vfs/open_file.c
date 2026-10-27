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
#include "../util/ref.h"
#include "vnode.h"
#include "vnode_ref.h"
#include "../multicore/spinlock.h"
#include "../util/assert.h"
#include "../multitasking/queue.h"
#include "../util/lambda.h"
#include "inode.h"
#include "open_file_descriptor_vector.h"

void open_file_descriptor_delete_ref(open_file_descriptor_ref_t* ref)
{
    if (!ref) return;
    open_file_descriptor_ref_t ref_ref = (open_file_descriptor_ref_t){ __atomic_exchange_n((void**)&ref->ptr, 0, __ATOMIC_RELEASE ) };
    struct ref* ref_ref_ref = ref_ref.ptr ? &ref_ref.ptr->ref : NULL;
    if (ref_ref_ref)
        ref_dec(ref_ref_ref);
}

open_file_descriptor_ref_t open_file_descriptor_copy_ref(open_file_descriptor_ref_t ref)
{
    if (!ref.ptr) return ref;
    ref_inc(&ref.ptr->ref);
    return ref;
}

open_file_descriptor_ref_t vfs_allocate_new_open_file_descriptor(int flags, vnode_ref_t vnode, struct stat *st)
{
    ASSERT(st);
    open_file_descriptor_t* ofd = calloc(1, sizeof(open_file_descriptor_t));
    open_file_descriptor_ref_t ref = { ofd };
    if (!ofd) return ref;
    ofd->ref = OPEN_FD_REF_INIT;
    ofd->st = *st;
    ofd->vnode = vnode_copy_ref(vnode);
    ofd->lock = SPINLOCK_NOINT_INIT;
    ofd->flags = flags;
    ofd->offset = 0;
    return ref;
}

ssize_t ofd_vnode_read(open_file_descriptor_t* ofd, void* buf, size_t bytes)
{
    ASSERT(ofd);
    ASSERT(ofd->vnode.ptr);
    inode_ref_t inode = inode_stack_top_inode(&ofd->vnode.ptr->inodes);
    if (!inode.ptr) return -EBADF;
    if (S_ISDIR(inode.ptr->st.st_mode)) return -EISDIR;
    ASSERT(inode.ptr->read);
    ssize_t ret = inode.ptr->read(inode, buf, bytes, ofd->offset);
    if (ret > 0) ofd->offset += ret;
    inode_delete_ref(&inode);
    return ret;
}
ssize_t ofd_vnode_write(open_file_descriptor_t* ofd, void* buf, size_t bytes)
{
    ASSERT(ofd);
    ASSERT(ofd->vnode.ptr);
    inode_ref_t inode = inode_stack_top_inode(&ofd->vnode.ptr->inodes);
    if (!inode.ptr) return -EBADF;
    if (S_ISDIR(inode.ptr->st.st_mode)) return -EISDIR;
    ASSERT(inode.ptr->write);
    ssize_t ret = inode.ptr->write(inode, buf, bytes, ofd->offset);
    if (ret > 0) ofd->offset += ret;
    inode_delete_ref(&inode);
    return ret;
}

open_file_descriptor_ref_t vfs_create_new_file_descriptor_from_vnode(int* _errno, int flags, vnode_ref_t vnode, struct stat* _st)
{
    ASSERT(_errno);
    ASSERT(vnode.ptr);
    *_errno = 0;
    struct stat st = _st ? *_st : vnode_stat(vnode);
    open_file_descriptor_ref_t ofd = vfs_allocate_new_open_file_descriptor(flags, vnode, &st);
    if (!ofd.ptr)
    {
        *_errno = ENOMEM;
        return ofd;
    }
    mountpoint_ref_t mp = inode_stack_top_mountpoint(&vnode.ptr->inodes);
    ofd.ptr->read = ofd_vnode_read;
    ofd.ptr->write = ofd_vnode_write;
    uint32_t eflags = acquire_spinlock_noint(&mp.ptr->lock);
    bool unmounting = mp.ptr->unmounting;
    if (!unmounting)
        mp.ptr->busy++;
    release_spinlock_noint(&mp.ptr->lock, eflags);
    mountpoint_delete_ref(&mp);
    if (unmounting)
    {
        open_file_descriptor_delete_ref(&ofd);
        *_errno = ENOENT;
        return ofd;
    }
    return ofd;
}

void ___open_file_descriptor_free(const struct ref *ref)
{
    open_file_descriptor_t* ofd = container_of(ref, open_file_descriptor_t, ref);
    if (ofd->vnode.ptr)
    {
        mountpoint_ref_t mp = inode_stack_top_mountpoint(&ofd->vnode.ptr->inodes);
        uint32_t eflags = acquire_spinlock_noint(&mp.ptr->lock);
        mp.ptr->busy--;
        release_spinlock_noint(&mp.ptr->lock, eflags);
        mountpoint_delete_ref(&mp);
        vnode_delete_ref(&ofd->vnode);
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

struct dirent64 vfs_find_new_child_entry(open_file_descriptor_ref_t open_file)
{
    vfs_explore(open_file.ptr->vnode);
    vnode_ref_t cur = vnode_dereference_vnode(open_file.ptr->vnode, children);
    off_t cur_index = 0;
    while (cur.ptr)
    {
        // Not really that important to make it atomic tbf
        if (cur_index == open_file.ptr->offset)
        {
            open_file.ptr->offset++;
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
            uint32_t flags = acquire_spinlock_noint(&cur.ptr->lock);
            snprintf(ret.d_name, 255, "%s", cur.ptr->name);
            release_spinlock_noint(&cur.ptr->lock, flags);
            vnode_delete_ref(&cur);
            ret.d_name[255] = 0;
            return ret;
        }
        vnode_move_reference(&cur, next);
        cur_index++;
    }
    open_file.ptr->offset = 0;
    return (struct dirent64){ .d_name = { 0 } };
}
