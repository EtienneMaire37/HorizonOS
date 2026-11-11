#include "mountpoint.h"
#include "mountpoint_ref.h"
#include <stdlib.h>

DEFINE_REFCOUNT_IMPL(mountpoint, ___mountpoint_create, ___mountpoint_free, mountpoint_create_params_t)

void ___mountpoint_free(mountpoint_t* mountpoint)
{
    void (*free_data)(void* data) = mountpoint->free_data;
    if (free_data)
        free_data(mountpoint->data);
    vnode_delete_strong_ref(&mountpoint->root);
    vnode_delete_strong_ref(&mountpoint->dev_node);
    free(mountpoint);
}
