#pragma once

#include "vnode.h"
#include <stdbool.h>

#define vfs_explore(node)    _vfs_explore(node, false)
#define vfs_explore_exploration_locked(node)    _vfs_explore(node, true)
int _vfs_explore(vnode_strong_ref_t node, bool el);
