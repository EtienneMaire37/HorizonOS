#define _GNU_SOURCE
#include "ioctl.h"
#include "task.h"
#include "multitasking.h"

#include <termios.h>

void syscall_ioctl(interrupt_registers_t* registers, int fd, unsigned long request, void* arg)
{
    switch (request)
    {
    case TIOCSPGRP:
    {
        pid_t pgrp = *(pid_t*)arg;
        if (!is_fd_valid(fd))
        {
            sc_ret_errno = EBADF;
            break;
        }
        file_entry_t* entry = get_global_file_entry(fd);
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
    }
    case TIOCGPGRP:
    {
        if (!is_fd_valid(fd))
        {
            sc_ret_errno = EBADF;
            break;
        }
        file_entry_t* entry = get_global_file_entry(fd);
        if (!vfs_isatty(entry))
        {
            sc_ret_errno = ENOTTY;
            break;
        }
        *(pid_t*)arg = tty_foreground_pgrp;
        sc_ret_errno = 0;
        break;
    }
    case TIOCGWINSZ:
    {
        if (!arg)
        {
            sc_ret_errno = EFAULT;
            break;
        }
        if (!is_fd_valid(fd))
        {
            sc_ret_errno = EBADF;
            break;
        }
        file_entry_t* entry = get_global_file_entry(fd);
        if (!vfs_isatty(entry))
        {
            sc_ret_errno = ENOTTY;
            break;
        }
        struct winsize* ws = arg;
        ws->ws_col = tty_res_x;
        ws->ws_row = tty_res_y;
        sc_ret_errno = 0;
        break;
    }
    case TIOCSWINSZ:
    {
        if (!arg)
        {
            sc_ret_errno = EFAULT;
            break;
        }
        if (!is_fd_valid(fd))
        {
            sc_ret_errno = EBADF;
            break;
        }
        file_entry_t* entry = get_global_file_entry(fd);
        if (!vfs_isatty(entry))
        {
            sc_ret_errno = ENOTTY;
            break;
        }
        struct winsize* ws = arg;
        tty_set_window_size(ws->ws_col, ws->ws_row);
        sc_ret_errno = 0;
        task_send_signal_to_pgrp(SIGWINCH, tty_foreground_pgrp);
        break;
    }
    default:
        LOG(DEBUG, "unknown ioctl request: %#lx, %p", request, arg);
        task_send_signal(current_task, SIGILL);
        sc_ret_errno = ENOSYS;
    }
}