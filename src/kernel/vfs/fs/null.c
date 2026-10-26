#include "null.h"

ssize_t null_read(inode_ref_t node, void* buf, size_t count, off_t off)
{
// Reads from /dev/null shall always return end-of-file (EOF).
    (void)node;
    (void)buf;
    (void)count;
    (void)off;
    return 0;
}
ssize_t null_write(inode_ref_t node, void* buf, size_t count, off_t off)
{
// Data written to /dev/null shall be discarded.
    (void)node;
    (void)buf;
    (void)count;
    (void)off;
    return count;
}

