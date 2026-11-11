#pragma once

#include <stddef.h>
#include "inode_ref.h"
#include "mountpoint_ref.h"
#include "../multicore/spinlock.h"

typedef struct
{
    inode_strong_ref_t* data;
    size_t allocated, size;
    spinlock_noint_t lock;
} inode_stack_t;

#define INODE_STACK_INIT    ((inode_stack_t){ .allocated = 0, .size = 0, .data = NULL })

int inode_stack_push(inode_stack_t* stack, inode_strong_ref_t ref);
inode_strong_ref_t __attribute__((warn_unused_result)) inode_stack_pop(inode_stack_t* stack);
inode_strong_ref_t __attribute__((warn_unused_result)) inode_stack_top_inode(inode_stack_t* stack);
mountpoint_strong_ref_t __attribute__((warn_unused_result)) inode_stack_top_mountpoint(inode_stack_t* stack);
