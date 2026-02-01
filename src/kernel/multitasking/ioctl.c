#include "ioctl.h"
#include "termios.h"
#include "task.h"

void syscall_ioctl(syscall_registers_t* registers, int fd, unsigned long request, void* arg)
{
    switch (request)
    {
    case TIOCSPGRP:
        pid_t pgrp = (pid_t)registers->rcx;
        if (!is_fd_valid(fd))
        {
            sc_ret_errno = EBADF;
            break;
        }
        file_entry_t* entry = &file_table[__CURRENT_TASK.file_table[fd]];
        if (!vfs_isatty(entry))
        {
            sc_ret_errno = ENOTTY;
            break;
        }
        if (pgrp <= 0)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        tty_foreground_pgrp = pgrp;
        sc_ret_errno = 0;
        break;
    default:
        kill_current_task(SIGILL);
    }
}