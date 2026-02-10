#include "syscall.h"
#include "../cpu/segbase.h"
#include "../../../mlibc/src/syscall.hpp"
#include <sys/mman.h>
#include "../cpu/memory.h"
#include "../memalloc/virtual_memory_allocator.h"
#include "ioctl.h"
#include <string.h>
#include "queue.h"
#include "multitasking.h"
#include "sigset.h"

void c_syscall_handler(syscall_registers_t* registers)
{
    // SC_LOG("syscall %" PRIu64, registers->rax);
    switch (registers->rax)
    {
    {
    sc_case(SYS_SETFS, 1, uint64_t)
        SC_LOG("syscall SYS_SETFS(%#" PRIx64 ")", arg1);
        wrfsbase(arg1);
        sc_ret_errno = 0;
        break;
    sc_case(SYS_READ, 3, int, void*, size_t)
        SC_LOG("syscall SYS_READ(%d, %p, %zu)", arg1, arg2, arg3);
        if (!is_fd_valid(arg1))
        {
            sc_ret_errno = EBADF;
            sc_ret(1) = (uint64_t)-1;
            break;
        }
        ssize_t ret;
        sc_ret_errno = (uint64_t)vfs_read(arg1, arg2, arg3, &ret);
        sc_ret(1) = (uint64_t)ret;
        break;
    sc_case(SYS_WRITE, 3, int, const void*, size_t)
        SC_LOG("syscall SYS_WRITE(%d, %p, %zu)", arg1, arg2, arg3);
        if (!is_fd_valid(arg1))
        {
            sc_ret_errno = EBADF;
            sc_ret(1) = (uint64_t)-1;
            break;
        }
        ssize_t ret;
        sc_ret_errno = vfs_write(arg1, arg2, arg3, &ret);
        sc_ret(1) = (uint64_t)ret;
        break;
    sc_case(SYS_EXIT, 1, int)
        SC_LOG("syscall SYS_EXIT(%d)", arg1);
        kill_current_task(((uint16_t)arg1 & 0x7f) << 8);        
    sc_case(SYS_ISATTY, 1, int)
        SC_LOG("syscall SYS_ISATTY(%d)", arg1);
        if (!is_fd_valid(arg1))
        {
            sc_ret_errno = EBADF;
            break;
        }
        file_entry_t* entry = &file_table[current_task->file_table[arg1]];
        if (vfs_isatty(entry))
            sc_ret_errno = 0;
        else
            sc_ret_errno = ENOTTY;
        break;
    sc_case(SYS_VM_MAP, 6, void*, size_t, int, int, int, off_t)
        SC_LOG("syscall SYS_VM_MAP(%p, %zu, %#x, %#x, %d, %" PRId64 ")", arg1, arg2, arg3, arg4, arg5, arg6);
        
        if (arg2 & 0xfff || arg2 == 0 || (uint64_t)arg1 & 0xfff)
        {
            sc_ret_errno = EINVAL;
            sc_ret(1) = (uint64_t)MAP_FAILED;
            break;
        }

        // * Only support READ/WRITE for now
        if (arg3 & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
        {
            sc_ret_errno = EINVAL;
            sc_ret(1) = (uint64_t)MAP_FAILED;
            break;
        }

        // * Don't support file mapping/sharing shenanigans for now
        if (arg4 != (MAP_PRIVATE | MAP_ANONYMOUS))
        {
            sc_ret_errno = EINVAL;
            sc_ret(1) = (uint64_t)MAP_FAILED;
            break;
        }

        void* addr = vmm_find_free_user_space_pages(arg1, arg2 / 4096);
        SC_LOG("MMAP: Found %zu free pages at %p", arg2 / 4096, addr);

        if (!addr)
        {
            sc_ret_errno = EINVAL;
            sc_ret(1) = (uint64_t)MAP_FAILED;
            break;
        }

        allocate_range((uint64_t*)(get_cr3() + PHYS_MAP_OFFSET), (uint64_t)addr, arg2 / 4096, PG_USER, (arg3 & PROT_WRITE) ? PG_READ_WRITE : PG_READ_ONLY, CACHE_WB);
        for (size_t i = 0; i < arg2 / 4096; i++)
            invlpg(i * 4096 + (uint64_t)addr);

        memset(addr, 0, arg2);

        sc_ret_errno = 0;
        sc_ret(1) = (uint64_t)addr;
        break;
    sc_case(SYS_VM_UNMAP, 2, void*, size_t)
        SC_LOG("syscall SYS_VM_UNMAP(%p, %zu)", arg1, arg2);
        while (true);
        break;
    sc_case(SYS_SEEK, 3, int, off_t, int)
        SC_LOG("syscall SYS_SEEK(%d, %" PRId64 ", %d)", arg1, arg2, arg3);
        if (!is_fd_valid(arg1))
        {
            sc_ret_errno = EBADF;
            sc_ret(1) = (uint64_t)((off_t)-1);
            break;
        }
        if (arg3 != SEEK_SET && arg3 != SEEK_CUR && arg3 != SEEK_END)
        {
            sc_ret_errno = EINVAL;
            sc_ret(1) = (uint64_t)((off_t)-1);
            break;
        }
        file_entry_t* entry = &file_table[current_task->file_table[arg1]];
        if (entry->entry_type != ET_FILE)
        {
            sc_ret_errno = 0;
            sc_ret(1) = (uint64_t)((off_t)0);
            break;
        }
        off_t offset = arg2;
        switch (arg3)
        {
        case SEEK_SET:
            goto do_seek;
        case SEEK_CUR:
            offset += entry->position;
            goto do_seek;
        case SEEK_END:
            offset += entry->tnode.file->inode->st.st_size;
            goto do_seek;
        }
        if (false)
        {
        do_seek:
            if (offset < 0) // || offset >= entry->tnode.file->inode->st.st_size)
            {
                sc_ret_errno = EINVAL;
                sc_ret(1) = (uint64_t)((off_t)-1);
                break;
            }
            entry->position = offset;
            sc_ret_errno = 0;
            sc_ret(1) = (uint64_t)entry->position;
            break;
        }
        break;
    sc_case(SYS_OPEN, 3, const char*, int, unsigned int)
        SC_LOG("syscall SYS_OPEN(%s, %d, %u)", arg1, arg2, arg3);
        int fd = vfs_allocate_global_file();

        // * Only supported flags for now
        if (arg2 & ~(O_CLOEXEC | O_ACCMODE))
        {
            sc_ret_errno = EINVAL;
            sc_ret(1) = (uint64_t)(-1);
            break;
        }

        file_table[fd].flags = arg2;
        file_table[fd].position = 0;

        struct stat st;
        int stat_ret = vfs_stat(arg1, current_task->cwd, &st);
        if (stat_ret != 0)
        {
            vfs_remove_global_file(fd);
            sc_ret_errno = stat_ret;
            sc_ret(1) = (uint64_t)(-1);
            break;
        }

        if (!(st.st_mode & S_IRUSR) && (file_table[fd].flags & O_ACCMODE) != O_WRONLY) // * Assume we're the owner of every file
        {
            vfs_remove_global_file(fd);
            sc_ret_errno = EACCES;
            sc_ret(1) = (uint64_t)(-1);
            break;
        }

        file_table[fd].entry_type = S_ISDIR(stat_ret) ? ET_FOLDER : ET_FILE;

        file_table[fd].tnode.file = NULL;
        file_table[fd].tnode.folder = NULL;

        if (file_table[fd].entry_type == ET_FILE)
            file_table[fd].tnode.file = vfs_get_file_tnode(arg1, current_task->cwd);
        if (file_table[fd].entry_type == ET_FOLDER)
            file_table[fd].tnode.folder = vfs_get_folder_tnode(arg1, current_task->cwd);

        int ret = vfs_allocate_thread_file(current_task);
        if (ret == -1)
        {
            vfs_remove_global_file(fd);

            sc_ret_errno = ENOMEM;
            sc_ret(1) = (uint64_t)(-1);
        }
        else
        {
            current_task->file_table[ret] = fd;
            sc_ret_errno = EACCES;
            sc_ret(1) = (uint64_t)ret;
        }
        break;
    sc_case(SYS_CLOSE, 1, int)
        SC_LOG("syscall SYS_CLOSE(%d)", arg1);
        if (!is_fd_valid(arg1))
        {
            sc_ret_errno = EBADF;
            sc_ret(1) = (uint64_t)(-1);
            break;
        }
        vfs_remove_global_file(current_task->file_table[arg1]);
        current_task->file_table[arg1] = invalid_fd;
        break;
    sc_case(SYS_IOCTL, 3, int, unsigned long, void*)
        SC_LOG("syscall SYS_IOCTL(%d, %lu, %p)", arg1, arg2, arg3);
        syscall_ioctl(registers, arg1, arg2, arg3);
        break;
    sc_case(SYS_EXECVE, 3, const char*, char**, char**)
        SC_LOG("syscall SYS_EXECVE(%s, %p, %p)", arg1, arg2, arg3);
        vfs_file_tnode_t* tnode = vfs_get_file_tnode(arg1, current_task->cwd);
        if (!tnode)
        {
            sc_ret_errno = ENOENT;
            break;
        }
        char rpath[PATH_MAX];
        vfs_realpath_from_file_tnode(tnode, rpath);
        if (vfs_access(arg1, current_task->cwd, X_OK) != 0)
        {
            sc_ret_errno = EACCES;
            break;
        }
        startup_data_struct_t data = startup_data_init_from_command(arg2, arg3);
        lock_task_queue();
        thread_t* new_task = multitasking_add_task_from_vfs(rpath, rpath, 3, false, &data, current_task->cwd);
        if (!new_task)
        {
            unlock_task_queue();
            sc_ret_errno = ENOENT;
            break;
        }
        else
        {
            pid_t old_pid = current_task->pid;
            
            current_task->pid = task_generate_pid();

            new_task->pid = old_pid;
            new_task->ppid = current_task->ppid;
            new_task->pgid = current_task->pgid;

            task_copy_file_table(current_task, new_task, true);

            move_running_task_to_thread_queue(&reapable_tasks, current_task);

            unlock_task_queue();
            switch_task();
            break;
        }
        break;
    sc_case(SYS_TCGETATTR, 2, int, struct termios*)
        SC_LOG("syscall SYS_TCGETATTR(%d, %p)", arg1, arg2);
        if (!arg2)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        if (!is_fd_valid(arg1))
        {
            sc_ret_errno = EBADF;
            break;
        }
        if (!vfs_isatty(&file_table[current_task->file_table[arg1]]))
        {
            sc_ret_errno = ENOTTY;
            break;
        }
        *arg2 = tty_ts;
        sc_ret_errno = 0;
        break;
    sc_case(SYS_TCSETATTR, 3, int, int, const struct termios*)
        SC_LOG("syscall SYS_TCSETATTR(%d, %d, %p)", arg1, arg2, arg3);
        if (!arg3)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        if (!is_fd_valid(arg1))
        {
            sc_ret_errno = EBADF;
            break;
        }
        if (!vfs_isatty(&file_table[current_task->file_table[arg1]]))
        {
            sc_ret_errno = ENOTTY;
            break;
        }
        tty_ts = *arg3;
        sc_ret_errno = 0;
        break;
    sc_case(SYS_GETCWD, 2, char*, size_t)
        SC_LOG("syscall SYS_GETCWD(%p, %zu)", arg1, arg2);
        if (!arg1 || arg2 == 0)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        char buf_cpy[PATH_MAX];
        vfs_realpath_from_folder_tnode(current_task->cwd, buf_cpy);
        for (size_t i = 0; i < arg2 - 1; i++)
            arg1[i] = buf_cpy[i];
        arg1[arg2 - 1] = 0;
        sc_ret_errno = 0;
        break;
    sc_case(SYS_CHDIR, 1, const char*)
        SC_LOG("syscall SYS_CHDIR(%s)", arg1);
        vfs_folder_tnode_t* tnode = vfs_get_folder_tnode(arg1, current_task->cwd);
        if (!tnode)
        {
            sc_ret_errno = vfs_get_file_tnode(arg1, current_task->cwd) ? ENOTDIR : ENOENT;
            break;
        }
        if (vfs_access(arg1, current_task->cwd, X_OK))
        {
            sc_ret_errno = EACCES;
            break;
        }
        current_task->cwd = tnode;
        sc_ret_errno = 0;
        break;
    sc_case(SYS_FORK, 0)
        SC_LOG("syscall SYS_FORK()");
        sc_ret_errno = 0;
        lock_task_queue();
        current_task->forked_pid = task_generate_pid();
        pid_t forked_pid = current_task->forked_pid;
        unlock_task_queue();
        switch_task();
        if (current_task->pid == forked_pid)
            sc_ret(1) = 0;
        else
            sc_ret(1) = forked_pid;
        break;
    sc_case(SYS_SIGACTION, 3, int, const struct sigaction*, struct sigaction*)
        SC_LOG("syscall SYS_SIGACTION(%d, %p, %p)", arg1, arg2, arg3);
        if (arg1 >= NUM_SIGNALS || arg1 == SIGKILL || arg1 == SIGSTOP)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        if (arg3) *arg3 = current_task->sig_act_array[arg1];
        if (arg2) current_task->sig_act_array[arg1] = *arg2;
        sc_ret_errno = 0;
        break;
    sc_case(SYS_SIGPROCMASK, 3, int, const sigset_t* __restrict, sigset_t* __restrict)
        SC_LOG("syscall SYS_SIGPROCMASK(%d, %p, %p)", arg1, arg2, arg3);
        if (!arg2)
        {
        	sc_ret_errno = EFAULT;
        	break;
        }
        if (arg3) *arg3 = current_task->sig_mask;
        switch (arg1)
        {
        case SIG_BLOCK:
        	current_task->sig_mask = sigset_bitwise_or(*arg2, current_task->sig_mask);
        	sc_ret_errno = 0;
        	break;
        case SIG_UNBLOCK:
           	current_task->sig_mask = sigset_bitwise_and(sigset_bitwise_not(*arg2), current_task->sig_mask);
           	sc_ret_errno = 0;
           	break;
        case SIG_SETMASK:
           	current_task->sig_mask = *arg2;
           	sc_ret_errno = 0;
           	break;
        default:
        	sc_ret_errno = EINVAL;
        }
    	break;
    sc_case(SYS_HOS_SET_KB_LAYOUT, 1, int)
        SC_LOG("syscall SYS_HOS_SET_KB_LAYOUT(%d)", arg1);
        // * Should probably apply some form of security (root only operation)
        if (arg1 >= 1 && arg1 <= NUM_KB_LAYOUTS)
        {
            current_keyboard_layout = keyboard_layouts[arg1 - 1];
            sc_ret_errno = 0;
        }
        else
            sc_ret_errno = EINVAL;
        break;
    }
    default:
        LOG(DEBUG, "syscall %" PRIu64 " not implemented", registers->rax);
        kill_current_task(SIGILL);
    }
}
