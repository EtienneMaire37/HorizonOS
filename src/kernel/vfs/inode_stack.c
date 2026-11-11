#include "inode_stack.h"
#include "inode.h"
#include "mountpoint.h"
#include "../util/assert.h"
#include <errno.h>
#include <stdlib.h>

int inode_stack_push(inode_stack_t* stack, inode_strong_ref_t ref)
{
    ASSERT(ref.ptr);
    uint32_t flags = acquire_spinlock_noint(&stack->lock);
    if (stack->size * sizeof(inode_strong_ref_t) == stack->allocated)
    {
        if (stack->allocated == 0)
        // Most vnodes won't have multiple mountpoints on them so it'd be wasteful to use more than one
            stack->allocated = sizeof(inode_strong_ref_t); 
        else
            stack->allocated *= 2;

        inode_strong_ref_t* data = (inode_strong_ref_t*)realloc(stack->data, stack->allocated);
        if (!data) return (release_spinlock_noint(&stack->lock, flags), ENOMEM);
        stack->data = data;
    }

    stack->data[stack->size] = inode_dup_strong_ref(ref);
    stack->size++;
    release_spinlock_noint(&stack->lock, flags);
    
    return 0;
}

inode_strong_ref_t inode_stack_pop(inode_stack_t* stack)
{
    uint32_t flags = acquire_spinlock_noint(&stack->lock);
    if (!stack->size) return (release_spinlock_noint(&stack->lock, flags), (inode_strong_ref_t){ NULL });
    stack->size--;
    inode_strong_ref_t ret = stack->data[stack->size];
    ASSERT(ret.ptr);

    if (2 * stack->size * sizeof(inode_strong_ref_t) == stack->allocated)
    {
        if (stack->allocated >= sizeof(inode_strong_ref_t))
            stack->allocated /= 2;
        else
            stack->allocated = 0;

        inode_strong_ref_t* data = (inode_strong_ref_t*)realloc(stack->data, stack->allocated);
        if (data || !stack->allocated)
            stack->data = data;
    }
    release_spinlock_noint(&stack->lock, flags);
    return ret;
}

inode_strong_ref_t inode_stack_top_inode(inode_stack_t* stack)
{
    uint32_t flags = acquire_spinlock_noint(&stack->lock);
    if (!stack->size) return (release_spinlock_noint(&stack->lock, flags), (inode_strong_ref_t){ NULL });
    inode_strong_ref_t ret = inode_dup_strong_ref(stack->data[stack->size - 1]);
    release_spinlock_noint(&stack->lock, flags);
    return ret;
}
mountpoint_strong_ref_t inode_stack_top_mountpoint(inode_stack_t* stack)
{
    inode_strong_ref_t inode = inode_stack_top_inode(stack);
    if (!inode.ptr) return (mountpoint_strong_ref_t){ NULL };
    mountpoint_strong_ref_t mp = mountpoint_dup_strong_ref(inode.ptr->ptr->mountpoint);
    inode_delete_strong_ref(&inode);
    return mp;
}

