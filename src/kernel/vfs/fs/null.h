#pragma once

#include <sys/types.h>
#include "../inode_ref.h"

ssize_t null_read(inode_ref_t node, void* buf, size_t count, off_t off);
ssize_t null_write(inode_ref_t node, void* buf, size_t count, off_t off);
