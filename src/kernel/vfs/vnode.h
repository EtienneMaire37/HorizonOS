#pragma once

#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>

#include "inode_stack.h"
#include "../util/ref.h"
#include "flags.h"
#include "vnode_ref.h"
#include "inode_ref.h"

struct vnode
{
    struct ref ref;

    char* _Atomic name;

    inode_stack_t inodes;

    vnode_ref_t children; // Owns a reference to each child
    vnode_t *_Atomic next, *_Atomic parent; // Non owning references to other nodes
    // ! No "prev" as it would cause problems to make the system MT-safe and is not really useful
    
    spinlock_noint_t lock, exploration_lock;
    vnode_flags_t flags;
};

#define vnode_dereference_vnode(vnode, field)       ___vnode_dereference_vnode((vnode), offsetof(vnode_t, field), false)
#define vnode_dereference_vnode_locked(vnode, field)       ___vnode_dereference_vnode((vnode), offsetof(vnode_t, field), true)
#define vnode_move_reference(vnode, field)       ___vnode_move_reference((vnode), offsetof(vnode_t, field))

#define VNODE_REF_INIT ((struct ref){___vnode_free, 1})

extern vnode_ref_t vfs_root_node;
extern _Atomic  size_t vfs_total_nodes;

typedef struct
{
    inode_ref_t inode;
    struct stat st;
    ssize_t (*read)(inode_ref_t, void*, size_t, off_t);
    ssize_t (*write)(inode_ref_t, void*, size_t, off_t);
    void* fs_specific;
    void (*free_fs_specific_data)(void*);
    bool explored;
    bool isatty;
    // Add here
} vfs_add_new_child_node_ex_params_t;
#define vfs_add_new_child_node_ex(node, name, ...) _vfs_add_new_child_node_ex(node, name, (vfs_add_new_child_node_ex_params_t){ \
            __VA_ARGS__ })

typedef struct
{
    ssize_t (*read)(inode_ref_t, void*, size_t, off_t);
    ssize_t (*write)(inode_ref_t, void*, size_t, off_t);
    size_t size; // For block special files
    bool isatty;
    // Add here
} vfs_create_params_t;
#define vfs_create(name, parent, mode, uid, gid, ...) _vfs_create(name, parent, mode, uid, gid, (vfs_create_params_t){ __VA_ARGS__ })
int _vfs_create(const char* name, vnode_ref_t parent, mode_t mode, uid_t uid, gid_t gid, vfs_create_params_t params);

void vfs_create_root_node();

vnode_ref_t ___vnode_dereference_vnode(vnode_ref_t node, size_t field_offset, bool locked);
void vnode_delete_ref(vnode_ref_t* ref);
void ___vnode_move_reference(vnode_ref_t* ref, size_t field_offset);

void ___vnode_free(const struct ref* ref);
vnode_ref_t vfs_create_new_vnode(const char* name, inode_ref_t inode);
int vfs_add_new_child_node(vnode_ref_t node, const char* name, const struct stat* st, void* fs_specific, void (*free_fs_specific_data)(void*));
int _vfs_add_new_child_node_ex(vnode_ref_t node, const char* name, vfs_add_new_child_node_ex_params_t params);
int vfs_add_new_child_node__hardlink(vnode_ref_t node, const char* name, inode_ref_t inode);
#define vfs_unload_children(node) _vfs_unload_children((node), false, false);
#define vfs_unload_children_locked(node) _vfs_unload_children((node), true, false);
// #define vfs_unload_children_exploration_locked(node) _vfs_unload_children((node), false, true);
#define vfs_unload_children_locked_exploration_locked(node) _vfs_unload_children((node), true, true);
void _vfs_unload_children(vnode_ref_t node, bool locked, bool exploration_locked);
void vfs_log_structure(vnode_ref_t node);
size_t vfs_get_absolute_path_to_node(vnode_ref_t node, char* buf, size_t bufsiz);
// Only call with the mountpoint_locked
size_t vfs_get_relative_path_to_node_from_mountpoint(vnode_ref_t ref, char* buf, size_t bufsiz);
bool vfs_verify_tree_integrity();
vnode_ref_t vnode_copy_ref(vnode_ref_t ref);
vnode_ref_t vfs_get_vnode_from_path(int* _errno, uid_t uid, gid_t gid, const char* path, vnode_ref_t root, vnode_ref_t cwd, bool follow_symlinks);
int vfs_unmount(vnode_ref_t ref, bool lazy);

ssize_t vnode_read(vnode_ref_t ref, void* buf, size_t bytes, off_t offset);
ssize_t vnode_write(vnode_ref_t ref, void* buf, size_t bytes, off_t offset);
struct stat vnode_stat(vnode_ref_t vnode);

int vfs_mount(vnode_ref_t ref, vnode_ref_t dev, const char* fstype);
