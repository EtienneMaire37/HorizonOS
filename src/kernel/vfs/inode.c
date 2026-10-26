#include <stdlib.h>
#include "inode.h"
#include "mountpoint.h"
#include "../util/ref.h"
#include "../util/assert.h"

inode_ref_t vfs_create_new_inode(const struct stat* st, void* fs_specific, void (*free_fs_specific_data)(void*), mountpoint_ref_t mp, bool isatty, ssize_t (*read)(inode_ref_t, void*, size_t, off_t), ssize_t (*write)(inode_ref_t, void*, size_t, off_t))
{
    ASSERT(st);
    inode_t* newn = calloc(1, sizeof(inode_t));
    if (!newn) return (inode_ref_t){ NULL };
    newn->st = *st;
    newn->st.st_nlink = 0;
    newn->lock = SPINLOCK_NOINT_INIT;
    newn->fs_specific = fs_specific;
    newn->free_fs_specific_data = free_fs_specific_data;
    newn->ref = INODE_REF_INIT;
    newn->mountpoint = mountpoint_copy_ref(mp);
    newn->isatty = isatty;

    if (mp.ptr)
    {
        newn->read = mp.ptr->read;
        newn->write = mp.ptr->write;
    }
    if (read)
        newn->read = read;
    if (write)
        newn->write = write;
    return (inode_ref_t){ newn };
}

void ___inode_free(const struct ref* _ref)
{
    inode_t* inode = container_of(_ref, inode_t, ref);
    if (inode->free_fs_specific_data) inode->free_fs_specific_data(inode->fs_specific);
    mountpoint_delete_ref(&inode->mountpoint);
    free(inode);
}

void inode_delete_ref(inode_ref_t* ref)
{
    if (!ref) return;
    inode_ref_t ref_ref = (inode_ref_t){ __atomic_exchange_n((void**)&ref->ptr, 0, __ATOMIC_RELEASE ) };
    struct ref* ref_ref_ref = ref_ref.ptr ? &ref_ref.ptr->ref : NULL;
    if (ref_ref_ref)
        ref_dec(ref_ref_ref);
}

inode_ref_t inode_copy_ref(inode_ref_t ref)
{
    if (!ref.ptr) return ref;
    ref_inc(&ref.ptr->ref);
    return ref;
}
