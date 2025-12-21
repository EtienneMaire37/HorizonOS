#pragma once

#include "../int/int.h"
#include "../../libc/src/syscall_defines.h"
#include "../../libc/src/startup_data.h"
#include "../multitasking/loader.h"
#include "../../libc/include/signal.h"

ssize_t task_chr_stdin(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction)
{
    switch(direction)
    {
    case CHR_DIR_READ:
        if (count == 0)
            return 0;
        if (no_buffered_characters(__CURRENT_TASK.input_buffer))
        {
            __CURRENT_TASK.reading_stdin = true;
            switch_task();
        }
        uint64_t ret = minint(get_buffered_characters(__CURRENT_TASK.input_buffer), count);
        for (uint32_t i = 0; i < count; i++)
            // *** Only ASCII for now ***
            buf[i] = utf32_to_bios_oem(utf32_buffer_getchar(&__CURRENT_TASK.input_buffer));
        return ret;
    case CHR_DIR_WRITE:
        return 0;
    }
    return 0;
}

ssize_t task_chr_stdout(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction)
{
    switch(direction)
    {
    case CHR_DIR_READ:
        return 0;
    case CHR_DIR_WRITE:
        for (uint32_t i = 0; i < count; i++)
            tty_outc(buf[i]);
        return count;
    }
    return 0;
}

ssize_t task_chr_stderr(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction)
{
    return task_chr_stdout(entry, buf, count, direction);
}

void handle_syscall(interrupt_registers_t* registers)
{
    // LOG(TRACE, "syscall %llu", registers->rax);
    switch (registers->rax)
    {
    case SYSCALL_EXIT:     // * exit | exit_code = $rbx |
        LOG(WARNING, "Task \"%s\" (pid = %d) exited with return code ", __CURRENT_TASK.name, __CURRENT_TASK.pid);
        switch ((int)registers->rbx)
        {
        case EXIT_SUCCESS:
            CONTINUE_LOG(WARNING, "%s", "EXIT_SUCCESS");
            break;
        case EXIT_FAILURE:
            CONTINUE_LOG(WARNING, "%s", "EXIT_FAILURE");
            break;
        default:
            CONTINUE_LOG(WARNING, "%d", (int)registers->rbx);
        }
        lock_task_queue();
        __CURRENT_TASK.return_value = (registers->rbx & 0xff) | WEXITBIT;
        __CURRENT_TASK.is_dead = true;
        unlock_task_queue();
        switch_task();
        break;

    case SYSCALL_OPEN:      // * open | path = $rbx, oflag = $rcx, mode = $rdx | $rax = errno, $rbx = fd
    {
        const char* path = (const char*)registers->rbx;

        int fd = vfs_allocate_global_file();
        file_table[fd].flags = ((int)registers->rcx) & (O_CLOEXEC | O_RDONLY | O_RDWR | O_WRONLY);   // * | O_APPEND | O_CREAT
        file_table[fd].position = 0;

        if (file_table[fd].flags != (int)registers->rcx)
        {
            vfs_remove_global_file(fd);
            registers->rbx = (uint64_t)(-1);
            registers->rax = EINVAL;
            break;
        }

        struct stat st;
        int stat_ret = vfs_stat(path, __CURRENT_TASK.cwd, &st);
        if (stat_ret != 0)
        {
            vfs_remove_global_file(fd);
            registers->rbx = (uint64_t)(-1);
            registers->rax = stat_ret;
            break;
        }

        if ((!(st.st_mode & S_IRUSR)) && ((file_table[fd].flags & O_RDWR) | (file_table[fd].flags & O_RDONLY))) // * Assume we're the owner of every file
        {
            vfs_remove_global_file(fd);
            registers->rbx = (uint64_t)(-1);
            registers->rax = EACCES;
            break;
        }

        if ((!(st.st_mode & S_IWUSR)) && ((file_table[fd].flags & O_RDWR) | (file_table[fd].flags & O_WRONLY))) // * Assume we're the owner of every file
        {
            vfs_remove_global_file(fd);
            registers->rbx = (uint64_t)(-1);
            registers->rax = EACCES;
            break;
        }

        file_table[fd].entry_type = S_ISDIR(stat_ret) ? ET_FOLDER : ET_FILE;

        file_table[fd].flags &= ~(O_APPEND | O_CREAT);

        file_table[fd].tnode.file = NULL;
        file_table[fd].tnode.folder = NULL;

        if (file_table[fd].entry_type == ET_FILE)
            file_table[fd].tnode.file = vfs_get_file_tnode(path, __CURRENT_TASK.cwd);
        if (file_table[fd].entry_type == ET_FOLDER)
            file_table[fd].tnode.folder = vfs_get_folder_tnode(path, __CURRENT_TASK.cwd);

        int ret = vfs_allocate_thread_file(current_task_index);
        // LOG(DEBUG, "global fd : %d", fd);
        // LOG(DEBUG, "fd : %d", ret);
        if (ret == -1)
        {
            vfs_remove_global_file(fd);

            registers->rbx = (uint64_t)(-1);
            registers->rax = ENOMEM;
        }
        else
        {
            __CURRENT_TASK.file_table[ret] = fd;
            registers->rbx = *(uint32_t*)&ret;
            registers->rax = 0;
        }
        break;
    }

    case SYSCALL_CLOSE:     // * close | fildes = $rbx | $rax = errno, $rbx = ret
        int fd = (int)registers->rbx;
        if (!is_fd_valid(fd))
        {
            registers->rbx = (uint64_t)(-1);
            registers->rax = EBADF;
            break;
        }
        vfs_remove_global_file(__CURRENT_TASK.file_table[fd]);
        __CURRENT_TASK.file_table[fd] = invalid_fd;
        break;

    case SYSCALL_WRITE:     // * write | fildes = $rbx, buf = $rcx, nbyte = $rdx | $rax = bytes_written, $rbx = errno
    {
        int fd = registers->rbx;
        if (!is_fd_valid(fd))
        {
            registers->rax = (uint64_t)(-1);
            registers->rbx = EBADF;
            break;
        }
        registers->rbx = vfs_write(fd, (unsigned char*)registers->rcx, registers->rdx, &registers->rax);
        break;
    }

    case SYSCALL_READ:      // * read | fildes = $rbx, buf = $rcx, nbyte = $rdx | $rax = bytes_read, $rbx = errno
    {
        int fd = (int)registers->rbx;
        if (!is_fd_valid(fd))
        {
            registers->rax = (uint64_t)-1;
            registers->rbx = EBADF;
            break;
        }
        ssize_t bytes_read;
        registers->rbx = vfs_read(fd, (void*)registers->rcx, registers->rdx, &bytes_read);
        registers->rax = (uint32_t)bytes_read;
        break;
    }

    case SYSCALL_BRK:   // * brk | addr = $rbx, break_address = $rcx | $rax = errno, $rbx = break_address
    {
        uintptr_t addr = registers->rbx;
        uintptr_t break_address = registers->rcx;

        if (addr < break_address)
        {
            free_range((uint64_t*)__CURRENT_TASK.cr3, ((break_address + 0xfff) & ~0xfffULL), (break_address - addr + 0xfff) / 0x1000);
        }
        else
        {
            allocate_range((uint64_t*)__CURRENT_TASK.cr3, 
                    break_address & ~0xfffULL, (addr - break_address + 0xfff) / 0x1000, 
                    __CURRENT_TASK.ring == 3 ? PG_USER : PG_SUPERVISOR, 
                    PG_READ_WRITE, CACHE_WB);
        }

        break_address = addr;

        registers->rbx = break_address;
        registers->rax = break_address == addr ? 0 : ENOMEM;
        break;
    }

    case SYSCALL_ISATTY:    // * isatty | fd = $rbx | $rax = errno, $rbx = ret
    {
        int fd = (int)registers->rbx;
        if (!is_fd_valid(fd))
        {
            registers->rax = EBADF;
            registers->rbx = 0;
            break;
        }
        file_entry_t* entry = &file_table[__CURRENT_TASK.file_table[fd]];
        registers->rbx = vfs_isatty(entry);
        if (!registers->rbx)
            registers->rax = ENOTTY;
        break;
    }

    case SYSCALL_EXECVE:    // * execve | path = $rbx, argv = $rcx, envp = $rdx | $rax = errno
    {
        vfs_file_tnode_t* tnode = vfs_get_file_tnode((char*)registers->rbx, __CURRENT_TASK.cwd);
        if (!tnode)
        {
            registers->rax = ENOENT;
            break;
        }
        char rpath[PATH_MAX];
        vfs_realpath_from_file_tnode(tnode, rpath);
        if (vfs_access((char*)registers->rbx, __CURRENT_TASK.cwd, X_OK) != 0)
        {
            registers->rax = EACCES;
            break;
        }
        startup_data_struct_t data = startup_data_init_from_command((char**)registers->rcx, (char**)registers->rdx);
        lock_task_queue();
        if (!multitasking_add_task_from_vfs(rpath, rpath, 3, false, &data, __CURRENT_TASK.cwd))
        {
            unlock_task_queue();
            registers->rax = ENOENT;
            break;
        }
        else
        {
            const uint16_t new_task_index = task_count - 1;
            pid_t old_pid = __CURRENT_TASK.pid;
            
            __CURRENT_TASK.is_dead = __CURRENT_TASK.to_reap = true;
            __CURRENT_TASK.pid = task_generate_pid();

            tasks[new_task_index].pid = old_pid;
            tasks[new_task_index].ppid = __CURRENT_TASK.ppid;
            tasks[new_task_index].pgid = __CURRENT_TASK.pgid;

            task_copy_file_table(current_task_index, new_task_index, true);

            unlock_task_queue();
            switch_task();
            break;
        }
    }

    case SYSCALL_WAITPID: // * waitpid | pid = $rbx, options = $rdx | $rax = errno, $rbx = *wstatus, $rcx = return_value
    {
        pid_t pid = registers->rcx;
        lock_task_queue();
        __CURRENT_TASK.wait_pid = registers->rbx;
        unlock_task_queue();
        switch_task();
        registers->rcx = pid;
        registers->rbx = __CURRENT_TASK.wstatus;
        break;
    }

    case SYSCALL_GETPID:     // * getpid || $rax = pid
        lock_task_queue();
        registers->rax = __CURRENT_TASK.pid;
        unlock_task_queue();
        break;

    case SYSCALL_FORK:     // * fork
        if (task_count >= MAX_TASKS)
        {
            registers->rax = (uint64_t)-1;
        }
        else
        {
            lock_task_queue();
            __CURRENT_TASK.forked_pid = task_generate_pid();
            pid_t forked_pid = __CURRENT_TASK.forked_pid;
            unlock_task_queue();
            switch_task();
            if (__CURRENT_TASK.pid == forked_pid)
                registers->rax = 0;
            else
            {
                registers->rax = forked_pid;
            }
        } 
        break;

    case SYSCALL_TCGETATTR:     // * tcgetattr | fildes = $rbx, termios_p = $rcx | $rax = errno
    {
        struct termios* termios_p = (struct termios*)registers->rcx;
        if (!termios_p)
        {
            registers->rax = EINVAL;
            break;
        }
        int fd = (int)registers->rbx;
        if (!is_fd_valid(fd))
        {
            registers->rax = EBADF;
            break;
        }
        if (!vfs_isatty(&file_table[__CURRENT_TASK.file_table[fd]]))
        {
            registers->rax = ENOTTY;
            break;
        }
        *termios_p = tty_ts;
        registers->rax = 0;
        break;
    }

    case SYSCALL_TCSETATTR:     // * tcsetattr | fildes = $rbx, termios_p = $rcx, optional_actions = $rdx | $rax = errno
    {
        struct termios* termios_p = (struct termios*)registers->rcx;
        if (!termios_p)
        {
            registers->rax = EINVAL;
            break;
        }
        int fd = (int)registers->rbx;
        if (!is_fd_valid(fd))
        {
            registers->rax = EBADF;
            break;
        }
        if (!vfs_isatty(&file_table[__CURRENT_TASK.file_table[fd]]))
        {
            registers->rax = ENOTTY;
            break;
        }
        tty_ts = *termios_p;
        registers->rax = 0;
        break;
    }

    case SYSCALL_SET_KB_LAYOUT:
    // * Should probably apply some form of security (user system)
        if (registers->rbx >= 1 && registers->rbx <= NUM_KB_LAYOUTS)
        {
            current_keyboard_layout = keyboard_layouts[registers->rbx - 1];
            registers->rax = 1;
        }
        else
            registers->rax = 0;
        break;

    case SYSCALL_FLUSH_INPUT_BUFFER:
        utf32_buffer_clear(&(__CURRENT_TASK.input_buffer));
        break;

    case SYSCALL_STAT:  // * stat | path = $rbx, stat_buf = $rcx | $rax = ret   
    {
        struct stat* st = (struct stat*)registers->rcx;
        const char* path = (const char*)registers->rbx;
        registers->rax = vfs_stat(path, __CURRENT_TASK.cwd, st);
        break;
    }

    case SYSCALL_FSTAT:  // * fstat | fd = $rbx, stat_buf = $rcx | $rax = ret   
    {
        struct stat* st = (struct stat*)registers->rcx;
        int fd = (int)registers->rbx;
        registers->rax = vfs_fstat(fd, __CURRENT_TASK.cwd, st);
        break;
    }

    case SYSCALL_ACCESS:  // * access | path = $rbx, mode = $rcx | $rax = ret
    {
        const char* path = (const char*)registers->rbx;
        registers->rax = vfs_access(path, __CURRENT_TASK.cwd, registers->rcx);
        break;
    }

    case SYSCALL_READDIR:   // * readdir | &dirent_entry = $rbx, dirp = $rcx | $rax = errno, $rbx = return_address
    {
        struct dirent* dirent_entry = (struct dirent*)registers->rbx;
        DIR* dirp = (DIR*)registers->rcx;
        registers->rbx = (uintptr_t)vfs_readdir(dirent_entry, dirp);
        registers->rax = errno;
        break;
    }

    case SYSCALL_GETCWD:    // * getcwd | buffer = $rbx, size = $rcx | $rax = ret
    {
        if (!registers->rbx || (size_t)registers->rcx == 0)
        {
            registers->rax = (uint64_t)NULL;
            break;
        }
        char buf_cpy[PATH_MAX];
        vfs_realpath_from_folder_tnode(__CURRENT_TASK.cwd, buf_cpy);
        for (size_t i = 0; i < (size_t)registers->rcx - 1; i++)
            ((uint8_t*)registers->rbx)[i] = buf_cpy[i];
        ((uint8_t*)registers->rbx)[(size_t)registers->rcx - 1] = 0;
        registers->rax = registers->rbx;
        break;
    }

    case SYSCALL_CHDIR:     // * chdir | path = $rbx | $rax = errno
    {
        vfs_folder_tnode_t* tnode = vfs_get_folder_tnode((char*)registers->rbx, __CURRENT_TASK.cwd);
        if (!tnode)
        {
            registers->rax = vfs_get_file_tnode((char*)registers->rbx, __CURRENT_TASK.cwd) ? ENOTDIR : ENOENT;
            break;
        }
        if (vfs_access((char*)registers->rbx, __CURRENT_TASK.cwd, X_OK))
        {
            registers->rax = EACCES;
            break;
        }
        __CURRENT_TASK.cwd = tnode;
        registers->rax = 0;
        break;
    }

    case SYSCALL_REALPATH:  // * realpath | path = $rbx, resolved_path = $rcx | $rax = errno
    {
        vfs_folder_tnode_t* folder_tnode = vfs_get_folder_tnode((char*)registers->rbx, __CURRENT_TASK.cwd);
        if (!folder_tnode) 
        {
            // * Maybe it's a file
            vfs_file_tnode_t* file_tnode = vfs_get_file_tnode((char*)registers->rbx, __CURRENT_TASK.cwd);
            if (!file_tnode)
            {
                registers->rax = ENOENT;
                break;
            }
            vfs_realpath_from_file_tnode(file_tnode, (char*)registers->rcx);
            registers->rax = 0;
            break;
        }
        vfs_realpath_from_folder_tnode(folder_tnode, (char*)registers->rcx);
        registers->rax = 0;
        break;
    }

    case SYSCALL_LSEEK:     // * lseek | fd = $rbx, offset = (off_t)$rcx, whence = (int)$rdx | $rax = (uint64_t)-errno
    {
        int fd = (int)registers->rbx;
        if (!is_fd_valid(fd))
        {
            registers->rax = (uint64_t)(-EBADF);
            break;
        }
        file_entry_t* entry = &file_table[__CURRENT_TASK.file_table[fd]];
        if (entry->entry_type != ET_FILE)
        {
            registers->rax = (uint64_t)(-ESPIPE);
            break;
        }
        off_t offset = (off_t)registers->rcx;
        int whence = registers->rdx;
        switch (whence)
        {
        case SEEK_SET:
            goto do_seek;
        case SEEK_CUR:
            offset += entry->position;
            goto do_seek;
        case SEEK_END:
            offset += entry->tnode.file->inode->st.st_size;
            goto do_seek;
        default:
            registers->rax = (uint64_t)(-EINVAL);
        }
        if (false)
        {
        do_seek:
            if (offset < 0) // || offset >= entry->tnode.file->inode->st.st_size)
            {
                registers->rax = (uint64_t)(-EINVAL);
                break;
            }
            entry->position = offset;
            registers->rax = entry->position;
            break;
        }
        break;
    }

    case SYSCALL_TCGETPGRP: // * tcgetpgrp | fd = $rbx | $rax = (uint64_t)-errno/pid
    {
        int fd = registers->rbx;
        if (!is_fd_valid(fd))
        {
            registers->rax = (uint64_t)-EBADF;
            break;
        }
        file_entry_t* entry = &file_table[__CURRENT_TASK.file_table[fd]];
        if (!vfs_isatty(entry))
        {
            registers->rax = (uint64_t)-ENOTTY;
            break;
        }
        registers->rax = (uint64_t)tty_foreground_pgrp;
        break;
    }

    case SYSCALL_TCSETPGRP: // * tcsetpgrp | fd = $rbx, pgrp = $rcx | $rax = (uint64_t)errno
    {
        int fd = registers->rbx;
        pid_t pgrp = (pid_t)registers->rcx;
        if (!is_fd_valid(fd))
        {
            registers->rax = EBADF;
            break;
        }
        file_entry_t* entry = &file_table[__CURRENT_TASK.file_table[fd]];
        if (!vfs_isatty(entry))
        {
            registers->rax = ENOTTY;
            break;
        }
        if (pgrp <= 0)
        {
            registers->rax = EINVAL;
            break;
        }
        tty_foreground_pgrp = pgrp;
        registers->rax = 0;
        break;
    }
    
    case SYSCALL_GETPGID:   // * getpgid | pid = $rbx | $rax = (uint64_t)-errno/pid
    {
        pid_t pid = (pid_t)registers->rbx;
        if (pid < 0)
        {
            registers->rax = (uint64_t)-EINVAL;
            break;
        }
        thread_t* task;
        if (pid == 0)
            task = &__CURRENT_TASK;
        else
            task = find_task_by_pid(pid);
        if (!task)
        {
            registers->rax = (uint64_t)-ESRCH;
            break;
        }
        registers->rax = task->pgid;
        break;
    }

    case SYSCALL_SETPGID:   // * setpgid | pid = $rbx, pgid = $rcx | $rax = (uint64_t)errno
    {
        pid_t pid = (pid_t)registers->rbx;
        if (pid < 0)
        {
            registers->rax = (uint64_t)-EINVAL;
            break;
        }
        thread_t* task;
        lock_task_queue();
        if (pid == 0)
            task = &__CURRENT_TASK;
        else
            task = find_task_by_pid(pid);
        if (!task)
        {
            registers->rax = (uint64_t)-ESRCH;
            unlock_task_queue();
            break;
        }
        pid_t pgid = (pid_t)registers->rcx;
        lock_task_queue();
        if (!pgid)
            pgid = task->pid;
        task->pgid = pgid;
        unlock_task_queue();
        registers->rax = 0;
        break;
    }

    default:
        LOG(ERROR, "Undefined system call (%#llx)", registers->rax);
        
        __CURRENT_TASK.is_dead = true;
        __CURRENT_TASK.return_value = WSIGNALBIT | SIGILL;
        switch_task();
    }
}