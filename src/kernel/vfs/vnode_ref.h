#pragma once

#include "../util/refcount.h"
#include "inode_ref.h"

typedef struct vnode vnode_t;

DEFINE_REFCOUNT_INC1(vnode)

typedef struct
{
    const char* name;
    inode_strong_ref_t inode;
} vnode_create_params_t;

DEFINE_REFCOUNT_INC2(vnode, vnode_create_params_t)

void ___vnode_free(vnode_t* vnode);
vnode_t* ___vnode_create(vnode_create_params_t params);
