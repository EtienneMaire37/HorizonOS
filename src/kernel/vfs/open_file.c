#include "open_file.h"
#include <stdlib.h>
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
#include "open_file_descriptor_vector.h"
#include "../multitasking/multitasking.h"

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
    open_file_descriptor_ref_t ref = { calloc(1, sizeof(open_file_descriptor_t)) };
    if (!ref.ptr) return ref;
    ref.ptr->ref = OPEN_FD_REF_INIT;
    ref.ptr->st = *st;
    ref.ptr->vnode = vnode_copy_ref(vnode);
    ref.ptr->lock = SPINLOCK_NOINT_INIT;
    ref.ptr->flags = flags;
    ref.ptr->offset = 0;
    // TODO: Make a wrapper around vnode's io functions
    // or just design this better
    return ref;
}

open_file_descriptor_ref_t vfs_create_new_file_descriptor_from_vnode(int* _errno, int flags, vnode_ref_t vnode)
{
    ASSERT(_errno);
    ASSERT(vnode.ptr);
    *_errno = 0;
    struct stat st = vnode_stat(vnode);
    open_file_descriptor_ref_t ofd = vfs_allocate_new_open_file_descriptor(flags, vnode, &st);
    if (!ofd.ptr)
    {
        *_errno = ENOMEM;
        return ofd;
    }
    mountpoint_ref_t mp = inode_stack_top_mountpoint(&vnode.ptr->inodes);
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

int vfs_open(const char* path, int flags, mode_t mode,
             uid_t euid, gid_t egid,
             vnode_ref_t root, vnode_ref_t cwd,
             mode_t umask)
{
    ASSERT(root.ptr); // Should always be the root of the current process (probably)
    ASSERT(cwd.ptr);  // Should always be the current working directory of the current process (not null)

    // Not implemented
    if (flags & ~(O_NOFOLLOW | O_DIRECTORY | O_CLOEXEC)) return EINVAL;

    // Invalid flag combinations
    if ((flags & O_EXCL) && !(flags & O_CREAT)) return EINVAL;

    // TODO: Implement ACLs
    mode &= ~umask;
    
    int _errno;
    vnode_ref_t vnode = vfs_get_vnode_from_path(&_errno, euid, egid, path, root, cwd, !(flags & O_NOFOLLOW));

    int ret = -ENOSYS;
    if (!vnode.ptr)
    {
        if (_errno == ENOENT && (flags & O_CREAT))
        {
            // TODO: Implement file creation
        }
        ret = -_errno;
        goto end;
    }
    else
    {
        mode = vnode_stat(vnode).st_mode;
        if ((flags & O_DIRECTORY) && !S_ISDIR(mode))
        {
            ret = -ENOTDIR;
            goto end;
        }
        if ((flags & O_CREAT) && (flags & O_EXCL))
        {
            ret = -EEXIST;
            goto end;
        }
        if ((flags & O_TRUNC) && S_ISREG(mode))
        {
            // TODO: trunc
        }
    }
    open_file_descriptor_ref_t desc = vfs_create_new_file_descriptor_from_vnode(&_errno, flags, vnode);
    if (!desc.ptr)
    {
        ret = -_errno;
        goto end;
    }
    ret = ofdv_open(&current_task->open_file_table, (open_file_descriptor_entry_t){ .flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0, .fd = desc });
    open_file_descriptor_delete_ref(&desc);
    if (ret == -1) ret = -EMFILE;
end:
    vnode_delete_ref(&vnode);
    return ret;
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
