#define _GNU_SOURCE
#include "syscall.h"
#include "ioctl.h"
#include "task.h"
#include "multitasking.h"
#include "../vfs/table.h"
#include "../graphics/linear_framebuffer.h"

#include <termios.h>
#include <asm-generic/ioctls.h>

void syscall_ioctl(interrupt_registers_t* registers, int fd, unsigned long request, void* arg)
{
    sc_ret_errno = 0;
    switch (request)
    {
    case TIOCSPGRP:
    {
        sc_ret(1) = 0;
        pid_t pgrp = *(pid_t*)arg;
        if (!is_fd_valid(fd))
        {
            sc_ret_errno = EBADF;
            sc_ret(1) = -1;
            break;
        }
        file_entry_t* entry = get_global_file_entry(fd);
        if (!vfs_isatty(entry))
        {
            sc_ret_errno = ENOTTY;
            sc_ret(1) = -1;
            break;
        }
        if (pgrp <= 0)
        {
            sc_ret_errno = EINVAL;
            sc_ret(1) = -1;
            break;
        }
        tty_foreground_pgrp = pgrp;
        break;
    }
    case TIOCGPGRP:
    {
        sc_ret(1) = -1;
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
        sc_ret(1) = 0;
        break;
    }
    case TIOCGWINSZ:
    {
        sc_ret(1) = -1;
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
        ws->ws_xpixel = framebuffer.width;
        ws->ws_ypixel = framebuffer.height;
        sc_ret(1) = 0;
        break;
    }
    case TIOCSWINSZ:
    {
        sc_ret(1) = -1;
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
        task_send_signal_to_pgrp(SIGWINCH, tty_foreground_pgrp);
        sc_ret(1) = 0;
        break;
    }
    case TCXONC:
    {
        int action = (int)((uintptr_t)arg);
        sc_ret(1) = 0;
        switch (action)
        {
        case TCOOFF:
            break;
        case TCOON:
            break;
        case TCIOFF:
            break;
        case TCION:
            break;
        default:
            sc_ret_errno = EINVAL;
            sc_ret(1) = -1;
        }
        break;
    }
    default:
        LOG(DEBUG, "Unknown ioctl request: %#lx, %p", request, arg);
        // task_send_signal(current_task, SIGILL);
        sc_ret_errno = ENOSYS;
    }
}
