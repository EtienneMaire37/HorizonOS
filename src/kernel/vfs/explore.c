#include "explore.h"
#include "flags.h"
#include "inode_stack.h"
#include "vnode.h"
#include "mountpoint.h"
#include "../multicore/spinlock.h"
#include "../debug/out.h"

#include <assert.h>
#include <linux/limits.h>
#include <sys/dir.h>
#include <errno.h>

int _vfs_explore(vnode_ref_t ref, bool el)
{
    vnode_t* node = ref.ptr;
    if (!node) return ENOENT;
    uint32_t flags = acquire_spinlock_noint(&node->lock);
    uint32_t entry_flags = flags;
    if ((node->flags & VNODE_EXPLORED) || (node->flags & VNODE_EXPLORING))
    {
        release_spinlock_noint(&node->lock, flags);
        return 0;
    }
    node->flags |= VNODE_EXPLORING;
    uint32_t flags2 = el ? flags : acquire_spinlock_noint(&node->exploration_lock);
    mountpoint_ref_t mp = inode_stack_top_mountpoint(&node->inodes);
    vfs_unload_children_locked_exploration_locked(ref);
    release_spinlock_noint(&node->lock, flags2);

    int ret = ENOSYS; 
    if (!mp.ptr)
    // NOTE: Don't log anything because it is a normal occurence with virtfs mountpoints and unmounted roots
        ; // LOG(DEBUG, "No mountpoint")
    else
    {
        int (*explore)(vnode_ref_t) = mp.ptr->explore;
        ret = explore ? explore(ref) : ENOSYS;
    }
    mountpoint_delete_ref(&mp);

    flags = acquire_spinlock_noint(&node->lock);
    node->flags |= VNODE_EXPLORED;
    node->flags &= ~VNODE_EXPLORING;
    if (!el) release_spinlock_noint(&node->exploration_lock, flags2);
    release_spinlock_noint(&node->lock, entry_flags);

    return ret;
}
