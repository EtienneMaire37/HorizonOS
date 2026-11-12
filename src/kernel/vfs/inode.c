#include <stdlib.h>
#include "inode_ref.h"
#include "inode.h"

DEFINE_REFCOUNT_IMPL(inode, ___inode_create, ___inode_free, inode_create_params_t)

void ___inode_free(inode_strong_ref_t node)
{
    inode_t* inode = node.ptr->ptr;
    if (inode->free_fs_specific_data) inode->free_fs_specific_data(inode->fs_specific);
    mountpoint_delete_strong_ref(&inode->mountpoint);
    free(inode);
}

