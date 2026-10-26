#include "console.h"
#include <unistd.h>
#include <errno.h>

ssize_t console_read(inode_ref_t node, void* buf, size_t count, off_t off)
{
    (void)node;
    (void)buf;
    (void)count;
    (void)off;
    return -ENOSYS;
}
ssize_t console_write(inode_ref_t node, void* buf, size_t count, off_t off)
{
    (void)node;
    (void)off;
    return write(STDOUT_FILENO, buf, count);
}
