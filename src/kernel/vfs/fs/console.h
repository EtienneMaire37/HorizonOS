#pragma once

#include <sys/types.h>
#include "../inode_ref.h"

ssize_t console_read(inode_strong_ref_t node, void* buf, size_t count, off_t off);
ssize_t console_write(inode_strong_ref_t node, void* buf, size_t count, off_t off);
