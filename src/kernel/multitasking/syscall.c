#define _GNU_SOURCE
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
#include <sys/resource.h>
#include <time.h>
#include "../time/ktime.h"
#include <fcntl.h>
#include <assert.h>
#include "signal.h"
#include "multitasking.h"

extern void sigret();

void c_syscall_handler(interrupt_registers_t* registers, void** return_address)
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
        // LOG(TRACE, "val: %c", ((char*)arg2)[0]);
        break;
    sc_case(SYS_WRITE, 3, int, const void*, size_t)
        SC_LOG("syscall SYS_WRITE(%d, %p, %zu)", arg1, arg2, arg3);
        if (!is_fd_valid(arg1))
        {
            sc_ret_errno = EBADF;
            sc_ret(1) = (uint64_t)-1;
            break;
        }
        // if (arg3 == 1)
        //     LOG(TRACE, "%c (%d)", ((char*)arg2)[0], ((char*)arg2)[0]);
        ssize_t ret;
        sc_ret_errno = vfs_write(arg1, arg2, arg3, &ret);
        sc_ret(1) = (uint64_t)ret;
        break;
    sc_case(SYS_EXIT, 1, int)
        SC_LOG("syscall SYS_EXIT(%d)", arg1);
        kill_task(current_task, ((uint16_t)arg1 & 0x7f) << 8);
        abort();
    sc_case(SYS_ISATTY, 1, int)
        SC_LOG("syscall SYS_ISATTY(%d)", arg1);
        if (!is_fd_valid(arg1))
        {
            sc_ret_errno = EBADF;
            break;
        }
        file_entry_t* entry = get_global_file_entry(arg1);
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
            unlock_scheduler();
            break;
        }

        allocate_range((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE), (uint64_t)addr, arg2 / 4096, PG_USER, (arg3 & PROT_WRITE) ? PG_READ_WRITE : PG_READ_ONLY, CACHE_WB);

        for (size_t i = 0; i < arg2 / 4096; i++)
            invlpg(i * 4096 + (uint64_t)addr);

        memset(addr, 0, arg2);

        sc_ret_errno = 0;
        sc_ret(1) = (uint64_t)addr;
        break;
    sc_case(SYS_VM_UNMAP, 2, void*, size_t)
        SC_LOG("syscall SYS_VM_UNMAP(%p, %zu)", arg1, arg2);
        if ((uint64_t)arg1 & 0xfff)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        free_range((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE), (virtual_address_t)arg1, (arg2 + 0xfff) >> 12);
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
        file_entry_t* entry = get_global_file_entry(arg1);
        if (entry->entry_type != ET_FILE)
        {
            sc_ret_errno = 0;
            sc_ret(1) = (uint64_t)((off_t)0);
            break;
        }
        if (vfs_isatty(entry))
        {
            sc_ret_errno = ESPIPE;
            sc_ret(1) = (uint64_t)((off_t)-1);
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
        SC_LOG("syscall SYS_OPEN(\"%s\", %d, %u)", arg1, arg2, arg3);

        // * Only supported flags for now
        if (arg2 & ~(O_CLOEXEC | O_ACCMODE))
        {
            sc_ret_errno = ENOSYS;
            sc_ret(1) = (uint64_t)(-1);
            break;
        }

        // acquire_mutex(&file_table_lock);

        int fd = vfs_allocate_global_file();

        file_table[fd].flags = arg2;
        file_table[fd].position = 0;

        struct stat st;
        int stat_ret = vfs_stat(arg1, current_task->cwd, &st);
        if (stat_ret != 0)
        {
            vfs_remove_global_file(fd);
            // release_mutex(&file_table_lock);
            sc_ret_errno = stat_ret;
            sc_ret(1) = (uint64_t)(-1);
            break;
        }

        if (false) // * Assume we're root
        {
            vfs_remove_global_file(fd);
            // release_mutex(&file_table_lock);
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
            // release_mutex(&file_table_lock);

            sc_ret_errno = EMFILE;
            sc_ret(1) = (uint64_t)(-1);
        }
        else
        {
            current_task->file_table[ret].index = fd;
            current_task->file_table[ret].flags = (arg2 & O_CLOEXEC) ? FD_CLOEXEC : 0;
            // release_mutex(&file_table_lock);
            sc_ret_errno = 0;
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
        vfs_close(arg1);
        break;
    sc_case(SYS_IOCTL, 3, int, unsigned long, void*)
        SC_LOG("syscall SYS_IOCTL(%d, %#lx, %p)", arg1, arg2, arg3);
        lock_scheduler();
        syscall_ioctl(registers, arg1, arg2, arg3);
        unlock_scheduler();
        break;
    sc_case(SYS_EXECVE, 3, const char*, char**, char**)
        SC_LOG("syscall SYS_EXECVE(\"%s\", %p, %p)", arg1, arg2, arg3);
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
        lock_scheduler();
        thread_t* new_task = multitasking_add_task_from_vfs(rpath, rpath, 3, false, &data, current_task->cwd);
        if (!new_task)
        {
            LOG(TRACE, "EXECVE: Couldn't load executable");
            unlock_scheduler();
            sc_ret_errno = ENOENT;
            break;
        }
        else
        {
            pid_t old_pid = current_task->pid;
            
            task_set_pid(current_task, task_generate_pid());
            task_set_pid(new_task, old_pid);
            
            new_task->ppid = current_task->ppid;
            task_set_pgid(new_task, current_task->pgid);

            task_copy_file_table(current_task, new_task, true);

            move_running_task_to_thread_queue(&reapable_tasks, current_task);

            unlock_scheduler();
            assert(task_lock_depth == 0);
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
        if (!vfs_isatty(get_global_file_entry(arg1)))
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
        if (!vfs_isatty(get_global_file_entry(arg1)))
        {
            sc_ret_errno = ENOTTY;
            break;
        }
        lock_scheduler();
        tty_ts = *arg3;
        if (arg2 == TCSAFLUSH)
        {
            keyboard_buffered_input_buffer.get_index = keyboard_buffered_input_buffer.put_index = 0;
            keyboard_input_buffer.get_index = keyboard_input_buffer.put_index = 0;
        }
        LOG(TRACE, "TCSETATTR: .c_iflag = %#o", arg3->c_iflag);
        LOG(TRACE, "TCSETATTR: .c_oflag = %#o", arg3->c_oflag);
        LOG(TRACE, "TCSETATTR: .c_cflag = %#o", arg3->c_cflag);
        LOG(TRACE, "TCSETATTR: .c_lflag = %#o", arg3->c_lflag);
        LOG(TRACE, "TCSETATTR: .c_line = `%c`", arg3->c_line);
        unlock_scheduler();
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
        size_t ret = vfs_realpath_from_folder_tnode(current_task->cwd, buf_cpy);
        if (ret > arg2)
        {
            sc_ret_errno = ERANGE;
            break;
        }
        memcpy(arg1, buf_cpy, arg2);
        sc_ret_errno = 0;
        break;
    sc_case(SYS_CHDIR, 1, const char*)
        SC_LOG("syscall SYS_CHDIR(\"%s\")", arg1);
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
        lock_scheduler();
        current_task->forked_pid = task_generate_pid();
        pid_t forked_pid = current_task->forked_pid;
        move_running_task_to_thread_queue(&forked_tasks, current_task);
        unlock_scheduler();
        assert(task_lock_depth == 0);
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
        lock_scheduler();
        if (arg3) *arg3 = current_task->sig_mask;
        sigset_t old_sigmask = current_task->sig_mask;
        switch (arg1)
        {
        case SIG_BLOCK:
        	current_task->sig_mask = sigset_bitwise_or(*arg2, current_task->sig_mask);
            task_unmask_signal(current_task, SIGKILL);
            task_unmask_signal(current_task, SIGSTOP);
        	sc_ret_errno = 0;
        	break;
        case SIG_UNBLOCK:
           	current_task->sig_mask = sigset_bitwise_and(sigset_bitwise_not(*arg2), current_task->sig_mask);
           	sc_ret_errno = 0;
           	break;
        case SIG_SETMASK:
           	current_task->sig_mask = *arg2;
            task_unmask_signal(current_task, SIGKILL);
            task_unmask_signal(current_task, SIGSTOP);
           	sc_ret_errno = 0;
           	break;
        default:
        	sc_ret_errno = EINVAL;
        }
        task_try_handle_signals(current_task, old_sigmask, current_task->sig_mask);
        unlock_scheduler();
    	break;
    sc_case(SYS_WAIT4, 4, pid_t, int*, int, struct rusage*)
        SC_LOG("syscall SYS_WAIT4(%d, %p, %d, %p)", arg1, arg2, arg3, arg4);
        if (arg3 & WNOWAIT)
        {
            sc_ret_errno = 0;
            break;
        }
        if (arg3 & ~(WNOHANG | WUNTRACED | WCONTINUED))
        {
            sc_ret_errno = EINVAL;
            break;
        }
        if (arg3 & WNOHANG)
        {
            lock_scheduler();
            sc_ret(1) = waitpid_find_child_in_tq(&dead_tasks, arg1, arg2, current_task->pgid);
            unlock_scheduler();
            sc_ret_errno = 0;
            break;
        }
        lock_scheduler();
        current_task->wait_pid = arg1;
        current_task->pgid_on_waitpid = current_task->pgid;
        current_task->waitpid_flags = arg3;
        current_task->waitpid_ret = -1;
        move_running_task_to_thread_queue(&waitpid_tasks, current_task);
        unlock_scheduler();
        waitpid_check_dead();
        assert(task_lock_depth == 0);
        switch_task();
        if (arg4)
            memset(arg4, 0, sizeof(struct rusage));
        if (arg2) *arg2 = current_task->wstatus;
        sc_ret(1) = current_task->waitpid_ret;
        sc_ret_errno = current_task->waitpid_ret == -1 ? EINTR : 0;
        break;
    sc_case(SYS_TTYNAME, 3, int, char*, size_t)
        SC_LOG("syscall SYS_TTYNAME(%d, %p, %zu)", arg1, arg2, arg3);
        lock_scheduler();
        if (!is_fd_valid(arg1))
        {
            sc_ret_errno = EBADF;
            unlock_scheduler();
            break;
        }
        file_entry_t* entry = get_global_file_entry(arg1);
        if (!vfs_isatty(entry))
        {
            sc_ret_errno = ENOTTY;
            unlock_scheduler();
            break;
        }
        vfs_file_tnode_t* tnode = entry->tnode.file;
        char path[PATH_MAX];
        ssize_t rp_ret = vfs_realpath_from_file_tnode(tnode, path);
        if (rp_ret == -1 || rp_ret > arg3)
        {
            sc_ret_errno = ERANGE;
            unlock_scheduler();
            break;
        }
        memcpy(arg2, path, rp_ret);
        unlock_scheduler();
        sc_ret_errno = 0;
        break;
    sc_case(SYS_GETRESUID, 3, uid_t*, uid_t*, uid_t*)
        SC_LOG("syscall SYS_GETRESUID(%p, %p, %p)", arg1, arg2, arg3);
        if (!arg1 || !arg2 || !arg3)
        {
            sc_ret_errno = EFAULT;
            break;
        }
        lock_scheduler();
        *arg1 = current_task->ruid;
        *arg2 = current_task->euid;
        *arg3 = current_task->suid;
        unlock_scheduler();
        sc_ret_errno = 0;
        break;
    sc_case(SYS_GETRESGID, 3, uid_t*, uid_t*, uid_t*)
        SC_LOG("syscall SYS_GETRESGID(%p, %p, %p)", arg1, arg2, arg3);
        if (!arg1 || !arg2 || !arg3)
        {
            sc_ret_errno = EFAULT;
            break;
        }
        lock_scheduler();
        *arg1 = current_task->rgid;
        *arg2 = current_task->egid;
        *arg3 = current_task->sgid;
        unlock_scheduler();
        sc_ret_errno = 0;
        break;
    sc_case(SYS_CLOCK_GET, 3, int, time_t*, long*)
        SC_LOG("syscall SYS_CLOCK_GET(%d, %p, %p)", arg1, arg2, arg3);
        if (!arg2 || !arg3)
        {
            sc_ret_errno = EFAULT;
            break;
        }
        switch (arg1)
        {
        case CLOCK_REALTIME:
            *arg2 = ktime(NULL);
            *arg3 = system_thousands * 1000000;
            sc_ret_errno = 0;
            break;
        default:
            sc_ret_errno = EINVAL;
        }
        break;
    sc_case(SYS_GETPID, 0)
        SC_LOG("syscall SYS_GETPID()");
        lock_scheduler();
        sc_ret(0) = (uint64_t)current_task->pid;
        unlock_scheduler();
        break;
    sc_case(SYS_GETPPID, 0)
        SC_LOG("syscall SYS_GETPPID()");
        lock_scheduler();
        sc_ret(0) = (uint64_t)current_task->ppid;
        unlock_scheduler();
        break;
    sc_case(SYS_GETHOSTNAME, 2, char*, size_t)
        SC_LOG("syscall SYS_GETHOSTNAME(%p, %zu)", arg1, arg2);
        if (!arg1)
        {
            sc_ret_errno = EFAULT;
            break;
        }
        const char* str = "horizonos-pc";
        const size_t len = strlen(str) + 1;
        if (arg2 <= 0)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        if (arg2 > len)
        {
            sc_ret_errno = ENAMETOOLONG;
            break;
        }
        memcpy(arg1, str, len);
        sc_ret_errno = 0;
        break;
    sc_case(SYS_FSTATAT, 4, int, const char*, int, struct stat*)
        SC_LOG("syscall SYS_FSTATAT(%d, \"%s\", %d, %p)", arg1, arg2, arg3, arg4);
        // TODO: Implement symbolic links
        if (arg3 & ~(AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW))
        {
            sc_ret_errno = EINVAL;
            break;
        }
        if (!arg4 || !arg2)
        {
            sc_ret_errno = EFAULT;
            break;
        }
        bool relative_path = *arg2 != '/';
        lock_scheduler();
        if (relative_path && !is_fd_valid(arg1))
        {
            unlock_scheduler();
            sc_ret_errno = EBADF;
            break;
        }
        vfs_folder_tnode_t* cwd = NULL;
        if (relative_path)
        {
            file_entry_t* entry = get_global_file_entry(arg1);
            if (entry->entry_type != ET_FOLDER)
            {
                sc_ret_errno = ENOTDIR;
                unlock_scheduler();
                break;
            }
            if (entry->entry_type == ET_FOLDER)
            {
                cwd = arg1 == AT_FDCWD ? current_task->cwd : entry->tnode.file->inode->parent;
                if (*arg2 == 0) // * empty path
                {
                    if (arg3 & AT_EMPTY_PATH)
                    {
                        *arg4 = entry->tnode.folder->inode->st;
                        sc_ret_errno = 0;
                        unlock_scheduler();
                        break;
                    }
                    else
                    {
                        sc_ret_errno = ENOENT;
                        unlock_scheduler();
                        break;
                    }
                }
                else
                    goto fstatat_get_st;
            }
            else
            {
                sc_ret_errno = ENOTDIR;
                unlock_scheduler();
                break;
            }
            sc_ret_errno = ENOSYS;
            break;
        }
        else
        {
        fstatat_get_st:
            vfs_folder_tnode_t* foldertnode = vfs_get_folder_tnode(arg2, cwd);
            if (!foldertnode)
            {
                vfs_file_tnode_t* filetnode = vfs_get_file_tnode(arg2, cwd);
                if (!filetnode)
                {
                    sc_ret_errno = ENOENT;
                    unlock_scheduler();
                    break;
                }
                *arg4 = filetnode->inode->st;
            }
            else
                *arg4 = foldertnode->inode->st;
            
            sc_ret_errno = 0;
            unlock_scheduler();
            break;
        }
        break;
    sc_case(SYS_GETPGID, 1, pid_t)
        SC_LOG("syscall SYS_GETPGID(%d)", arg1);
        if (arg1 < 0)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        lock_scheduler();
        if (arg1 == 0)
        {
            sc_ret_errno = 0;
            sc_ret(1) = current_task->pgid;
            unlock_scheduler();
            break;
        }
        thread_t* process = find_task_by_pid_anywhere(arg1);
        if (!process)
        {
            unlock_scheduler();
            sc_ret_errno = ESRCH;
            break;
        }
        sc_ret(1) = (uint64_t)process->pgid;
        sc_ret_errno = 0;
        unlock_scheduler();
        break;
    sc_case(SYS_SETPGID, 2, pid_t, pid_t)
        SC_LOG("syscall SYS_SETPGID(%d, %d)", arg1, arg2);
        lock_scheduler();
        thread_t* process = find_task_by_pid_anywhere(arg1);
        if (!process)
        {
            unlock_scheduler();
            sc_ret_errno = ESRCH;
            break;
        }
        task_set_pgid(process, arg2);
        sc_ret_errno = 0;
        unlock_scheduler();
        break;
    sc_case(SYS_DUP, 1, int)
        SC_LOG("syscall SYS_DUP(%d)", arg1);
        lock_scheduler();
        if (!is_fd_valid(arg1))
        {
            unlock_scheduler();
            sc_ret_errno = EBADF;
            break;
        }
        int newfd = vfs_allocate_thread_file(current_task);
        if (newfd == -1)
        {
            unlock_scheduler();
            sc_ret_errno = EMFILE;
            break;
        }
        current_task->file_table[newfd].index = current_task->file_table[arg1].index;
        current_task->file_table[newfd].flags = 0;
        file_table[current_task->file_table[newfd].index].used++;
        unlock_scheduler();
        sc_ret_errno = 0;
        sc_ret(1) = newfd;
        break;
    sc_case(SYS_DUP3, 3, int, int, int)
        SC_LOG("syscall SYS_DUP3(%d, %d, %d)", arg1, arg2, arg3);
        if (arg1 == arg3)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        if (arg2 & ~O_CLOEXEC)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        lock_scheduler();
        if (!is_fd_valid(arg1))
        {
            unlock_scheduler();
            sc_ret_errno = EBADF;
            break;
        }
        if (arg3 <= -1)
        {
            unlock_scheduler();
            sc_ret_errno = EINVAL;
            break;
        }
        if (is_fd_valid(arg3))
            vfs_close(arg3);
        current_task->file_table[arg3].index = current_task->file_table[arg1].index;
        current_task->file_table[arg3].flags = (arg2 & O_CLOEXEC) ? FD_CLOEXEC : 0;
        file_table[current_task->file_table[arg3].index].used++;
        unlock_scheduler();
        sc_ret_errno = 0;
        break;
    sc_case(SYS_FCNTL, 3, int, int, uint64_t)
        SC_LOG("syscall SYS_FCNTL(%d, %d, %" PRId64 ")", arg1, arg2, arg3);
        lock_scheduler();
        switch (arg2)
        {
        case F_GETFD:
            if (!is_fd_valid(arg1))
            {
                sc_ret_errno = EBADF;
                break;
            }
            sc_ret_errno = 0;
            sc_ret(1) = current_task->file_table[arg1].flags;
            break;
        case F_SETFD:
            if (!is_fd_valid(arg1))
            {
                sc_ret_errno = EBADF;
                break;
            }
            sc_ret_errno = 0;
            current_task->file_table[arg1].flags = arg3;
            break;
        case F_GETFL:
            if (!is_fd_valid(arg1))
            {
                sc_ret_errno = EBADF;
                break;
            }
            sc_ret_errno = 0;
            sc_ret(1) = file_table[current_task->file_table[arg1].index].flags;
            break;
        default:
            LOG(DEBUG, "Unknown fcntl request");
            task_send_signal(current_task, SIGILL);
            sc_ret_errno = ENOSYS;
        }
        unlock_scheduler();
        break;
    sc_case(SYS_SIGRET, 0)
        SC_LOG("syscall SYS_SIGRET()");
        *return_address = sigret;
        break;
    sc_case(SYS_PSELECT, 6, int, fd_set*, fd_set*, fd_set*, const struct timespec*, const sigset_t*)
        SC_LOG("syscall SYS_PSELECT(%d, %p, %p, %p, %p, %p)", arg1, arg2, arg3, arg4, arg5, arg6);
        lock_scheduler();
        // * sigmask
        sigset_t saved_sigmask = current_task->sig_mask;
        if (arg6)
            current_task->sig_mask = *arg6;

        sc_ret_errno = 0;

        // * Assume all fds are always ready for now
        // * Only clear exceptional conditions and check stdin
        if (arg2)
        {
            for (int i = 0; i < arg1; i++)
            {
                if (!(vfs_isatty(get_global_file_entry(i)))) continue;
                if (no_buffered_characters(keyboard_buffered_input_buffer))
                {
                    move_running_task_to_thread_queue(&waiting_for_stdin_tasks, current_task);
                    switch_task();
                    unlock_scheduler();
                    lock_scheduler();

                    if (no_buffered_characters(keyboard_buffered_input_buffer))
                    {
                        sc_ret_errno = EINTR;
                        for (int j = i; j < arg1; j++)
                            FD_CLR(j, arg2);
                        break;
                    }
                }
            }
        }
        if (arg4)
            FD_ZERO(arg4);

        sc_ret(1) = 0;
        for (int i = 0; i < arg1; i++)
        {
            if (arg2)
                sc_ret(1) += FD_ISSET(i, arg2) ? 1 : 0;
            if (arg3)
                sc_ret(1) += FD_ISSET(i, arg3) ? 1 : 0;
            if (arg4)
                sc_ret(1) += FD_ISSET(i, arg4) ? 1 : 0;
        }

        current_task->sig_mask = saved_sigmask;
        unlock_scheduler();
        break;
    sc_case(SYS_HOS_SET_KB_LAYOUT, 1, int)
        SC_LOG("syscall SYS_HOS_SET_KB_LAYOUT(%d)", arg1);
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
        task_send_signal(current_task, SIGILL);
        sc_ret_errno = ENOSYS;
    }

    lock_scheduler();
    if (current_task->sig_pending_user_space)
    {
        setup_user_signal_stack_frame__interrupt(registers);
        current_task->sig_pending_user_space = false;
        // *return_address = intret;
    }
    unlock_scheduler();
    
    // SC_LOG("returning from syscall to address %p", *return_address);
}
