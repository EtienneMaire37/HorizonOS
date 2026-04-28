#define _GNU_SOURCE
#include "hashmap.h"
#include "task.h"
#include <sys/select.h>
#include <asm-generic/errno.h>
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
#include "../util/memory.h"
#include "../vfs/entries.h"
#include <dirent.h>
#include "../cpu/units.h"
#include "../vfs/table.h"
#include <sys/utsname.h>
#include "../system/info.h"
#include "../vfs/pipe.h"
#include <sys/poll.h>
#include <sched.h>

extern void sigret();

bool should_restart_syscall()
{
// * If a blocked call to one of the following interfaces is interrupted by a signal handler, then the call is automatically restarted
// * after the signal handler returns if the SA_RESTART flag was used; otherwise the call fails with the error EINTR
    if (current_task->sig_pending_user_space && (current_task->sig_act_array[current_task->pending_signal_number].sa_flags & SA_RESTART))
        return true;
    return false;
}

void task_handle_signal_to_userspace(interrupt_registers_t* registers)
{
    lock_scheduler();
    if (current_task->sig_pending_user_space)
    {
        if (current_task->pending_signal_handler)
        {
            setup_user_signal_stack_frame__interrupt(registers);
            current_task->sig_pending_user_space = false;
        }
        else
            task_handle_sig_dfl(current_task, current_task->pending_signal_number);
    }
    unlock_scheduler();
}

uint64_t c_syscall_handler(interrupt_registers_t* registers, void** return_address)
{
    // SC_LOG("syscall %" PRIu64, registers->rax);
    uint64_t syscall_num = registers->rax;
    sc_ret_errno = -1;
    bool sc_no_errno = syscall_num == SYS_GETPID || syscall_num == SYS_GETPPID;
    if (current_task->sig_pending_user_space)
        sc_ret_errno = ERESTART;
    else
    {
    switch (syscall_num)
    {
    {
    sc_case(SYS_SETFS, 1, uint64_t)
        SC_LOG("syscall SYS_SETFS(%#" PRIx64 ")", arg1);
        wrfsbase(arg1);
        sc_ret_errno = 0;
        break;
    sc_case(SYS_READ, 3, int, void*, size_t)
        SC_LOG("syscall SYS_READ(%d, %p, %zu)", arg1, arg2, arg3);
        sc_validate_pointer(arg2);
        lock_scheduler();
        if (!is_fd_valid(arg1))
        {
            unlock_scheduler();
            sc_ret_errno = EBADF;
            sc_ret(1) = (uint64_t)-1;
            break;
        }
        unlock_scheduler();
        ssize_t ret;
        sc_ret_errno = (uint64_t)vfs_read(arg1, arg2, arg3, &ret);
        sc_ret(1) = (uint64_t)ret;
        break;
    sc_case(SYS_WRITE, 3, int, const void*, size_t)
        SC_LOG("syscall SYS_WRITE(%d, %p, %zu)", arg1, arg2, arg3);
        sc_validate_pointer(arg2);
        lock_scheduler();
        if (!is_fd_valid(arg1))
        {
            unlock_scheduler();
            sc_ret_errno = EBADF;
            sc_ret(1) = (uint64_t)-1;
            break;
        }
        unlock_scheduler();
        ssize_t ret;
        sc_ret_errno = vfs_write(arg1, arg2, arg3, &ret);
        sc_ret(1) = (uint64_t)ret;
        break;
    sc_case(SYS_EXIT, 1, int)
        SC_LOG("syscall SYS_EXIT(%d)", arg1);
        kill_task(current_task, ((uint16_t)arg1 & 0x7f) << 8);
        assert(!"exit fatal error");
        break;
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
            LOG(WARNING, "SYS_VM_MAP: Invalid or unsupported argument %#o", arg3);
            sc_ret_errno = EINVAL;
            sc_ret(1) = (uint64_t)MAP_FAILED;
            break;
        }

        // * Don't support file mapping/sharing shenanigans for now
        if (arg4 & ~(MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED))
        {
            LOG(WARNING, "SYS_VM_MAP: Invalid or unsupported argument %#o", arg4);
            sc_ret_errno = EINVAL;
            sc_ret(1) = (uint64_t)MAP_FAILED;
            break;
        }

        void* addr = (arg4 & MAP_FIXED) ? arg1 : vmm_find_free_user_space_pages(arg1, arg2 / 4096);

        if (!addr)
        {
            sc_ret_errno = EINVAL;
            sc_ret(1) = (uint64_t)MAP_FAILED;
            break;
        }

        SC_LOG("MMAP: Found %zu free pages at %p", arg2 / 4096, addr);

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
        sc_ret_errno = 0;
        free_range((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE), (virtual_address_t)arg1, (arg2 + 0xfff) >> 12);
        break;
    sc_case(SYS_VM_PROTECT, 3, void*, size_t, int)
        SC_LOG("syscall SYS_VM_PROTECT(%p, %zu, %#o)", arg1, arg2, arg3);
        sc_ret_errno = 0;
        arg2 = (arg2 + 0xfff) & ~0xfffULL;
        if (arg2 == 0 || (uint64_t)arg1 & 0xfff)
        {
            sc_ret_errno = EINVAL;
            sc_ret(1) = (uint64_t)MAP_FAILED;
            break;
        }
        if (arg3 & ~(PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC))
        {
            LOG(WARNING, "SYS_VM_PROTECT: Invalid or unsupported argument %#o", arg3);
            sc_ret_errno = EINVAL;
            sc_ret(1) = (uint64_t)MAP_FAILED;
            break;
        }
        // * For now completely ignore
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
        if (entry->entry_type != VFS_ET_FILE)
        {
            sc_ret_errno = 0;
            sc_ret(1) = (uint64_t)((off_t)0);
            break;
        }
        if (!S_ISREG(entry->st.st_mode))
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
            offset += entry->st.st_size;
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
        sc_ret_errno = 0;
        break;
    sc_case(SYS_OPENAT, 4, int, const char*, int, unsigned int)
        SC_LOG("syscall SYS_OPENAT(%d, \"%s\", %#o, %u)", arg1, arg2, arg3, arg4);
        sc_validate_pointer(arg2);

        // * Only supported flags for now
        // NOTE: A few of these aren't actually handled in the code but are supported as a byproduct of missing features (eg. symbolic links, writeable fs, ...)
        if (arg3 & ~(O_CLOEXEC | O_ACCMODE | O_NOCTTY | O_DIRECTORY | O_NONBLOCK | O_NOFOLLOW | O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC))
        {
            LOG(WARNING, "SYS_OPENAT: Invalid or unsupported argument %#o", arg3);
            sc_ret_errno = EINVAL;
            break;
        }

        lock_scheduler();
        bool relative = *arg2 != '/';
        if (relative && arg1 != AT_FDCWD && !is_fd_valid(arg1))
        {
            unlock_scheduler();
            sc_ret_errno = EBADF;
            break;
        }
        int fd = vfs_allocate_global_file();

        if (fd == -1)
        {
            unlock_scheduler();
            sc_ret_errno = ENFILE;
            break;
        }

        file_table[fd].flags = arg3;
        file_table[fd].position = 0;

        file_entry_t* entry = get_global_file_entry(arg1);
        vfs_folder_tnode_t* cwd = (arg1 == AT_FDCWD) ? current_task->cwd : ((entry && (entry->entry_type == VFS_ET_FOLDER)) ? entry->tnode.folder : NULL);

        struct stat st;
        int stat_ret = vfs_stat(arg2, cwd, &st);
        if (stat_ret != 0)
        {
            vfs_remove_global_file(fd);
            unlock_scheduler();
            if (stat_ret == ENOENT && (arg3 & O_CREAT))
                sc_ret_errno = EROFS;
            else
                sc_ret_errno = stat_ret;
            break;
        }

        if ((arg3 & O_DIRECTORY) && !S_ISDIR(st.st_mode))
        {
            vfs_remove_global_file(fd);
            unlock_scheduler();
            sc_ret_errno = ENOTDIR;
            break;
        }

        file_table[fd].entry_type = S_ISDIR(st.st_mode) ? VFS_ET_FOLDER : VFS_ET_FILE;

        file_table[fd].tnode.file = NULL;
        file_table[fd].tnode.folder = NULL;

        bool wr = ((arg3 & O_ACCMODE) == O_WRONLY || (arg3 & O_ACCMODE) == O_RDWR);

        if (file_table[fd].entry_type == VFS_ET_FILE)
        {
            file_table[fd].tnode.file = vfs_get_file_tnode(arg2, cwd);
            assert(file_table[fd].tnode.file);

            if (wr && file_table[fd].tnode.file->inode->drive.type == DT_INITRD)
            {
                vfs_remove_global_file(fd);
                unlock_scheduler();
                sc_ret_errno = EROFS;
                break;
            }

            file_table[fd].iofunc = file_table[fd].tnode.file->inode->io_func;

            file_table[fd].st = file_table[fd].tnode.file->inode->st;
        }
        else if (file_table[fd].entry_type == VFS_ET_FOLDER)
        {
            if (wr)
            {
                vfs_remove_global_file(fd);
                unlock_scheduler();
                sc_ret_errno = EISDIR;
                break;
            }

            file_table[fd].tnode.folder = vfs_get_folder_tnode(arg2, cwd);
            assert(file_table[fd].tnode.folder);

            file_table[fd].iofunc = NULL;

            file_table[fd].st = file_table[fd].tnode.folder->inode->st;
        }
        else
            assert(!"openat fatal error");

        file_table[fd].on_destroy = NULL;

        file_table[fd].file_data.folder_child.str = NULL;
        file_table[fd].file_data.folder_child.done_reading = false;

        int ret = vfs_allocate_thread_file(current_task);
        if (ret == -1)
        {
            vfs_remove_global_file(fd);
            unlock_scheduler();

            sc_ret_errno = EMFILE;
        }
        else
        {
            current_task->file_table[ret].index = fd;
            current_task->file_table[ret].flags = (arg3 & O_CLOEXEC) ? FD_CLOEXEC : 0;
            unlock_scheduler();
            sc_ret_errno = 0;
            sc_ret(1) = (uint64_t)ret;
        }
        break;
    sc_case(SYS_CLOSE, 1, int)
        SC_LOG("syscall SYS_CLOSE(%d)", arg1);
        lock_scheduler();
        if (!is_fd_valid(arg1))
        {
            unlock_scheduler();
            sc_ret_errno = EBADF;
            break;
        }
        vfs_close(arg1);
        unlock_scheduler();
        sc_ret_errno = 0;
        break;
    sc_case(SYS_OPEN_DIR, 1, const char*)
        SC_LOG("syscall SYS_OPEN_DIR(\"%s\")", arg1);
        sc_validate_pointer(arg1);
        if (!strcmp(arg1, ""))
        {
            sc_ret_errno = ENOENT;
            sc_ret(1) = -1;
            break;
        }

        lock_scheduler();

        vfs_folder_tnode_t* tnode = vfs_get_folder_tnode(arg1, current_task->cwd);
        if (!tnode)
        {
            unlock_scheduler();
            sc_ret_errno = ENOENT;
            sc_ret(1) = -1;
            break;
        }

        int fd = vfs_allocate_global_file();

        file_table[fd].entry_type = VFS_ET_FOLDER;

        file_table[fd].tnode.file = NULL;
        file_table[fd].tnode.folder = tnode;
        file_table[fd].st = file_table[fd].tnode.folder->inode->st;

        file_table[fd].file_data.folder_child.str = NULL;
        file_table[fd].file_data.folder_child.done_reading = false;

        file_table[fd].on_destroy = NULL;

        int ret = vfs_allocate_thread_file(current_task);
        if (ret == -1)
        {
            vfs_remove_global_file(fd);
            unlock_scheduler();

            sc_ret_errno = EMFILE;
            sc_ret(1) = (uint64_t)(-1);
        }
        else
        {
            current_task->file_table[ret].index = fd;
            current_task->file_table[ret].flags = 0;
            unlock_scheduler();
            sc_ret_errno = 0;
            sc_ret(1) = (uint64_t)ret;
        }
        break;
    sc_case(SYS_READ_ENTRIES, 3, int, void*, size_t)
        SC_LOG("syscall SYS_READ_ENTRIES(%d, %p, %zu)", arg1, arg2, arg3);
        sc_validate_pointer(arg2);
        lock_scheduler();
        if (!is_fd_valid(arg1))
        {
            unlock_scheduler();
            sc_ret_errno = EBADF;
            sc_ret(1) = 0;
            break;
        }
        file_entry_t* entry = get_global_file_entry(arg1);
        assert(entry);
        if (entry->entry_type != VFS_ET_FOLDER)
        {
            unlock_scheduler();
            sc_ret_errno = ENOTDIR;
            sc_ret(1) = 0;
            break;
        }
        sc_ret_errno = 0;
        if (entry->file_data.folder_child.done_reading)
        {
            sc_ret(1) = 0;
            entry->file_data.folder_child.done_reading = false;
            unlock_scheduler();
            break;
        }
        struct dirent64 dir_entry;
        size_t bytes_read = 0;
        while (true)
        {
            if (bytes_read + sizeof(dir_entry) > arg3)
            {
                if (bytes_read == 0)
                    sc_ret_errno = EINVAL;
                break;
            }
            dir_entry = vfs_find_new_child_entry(entry);
            if (dir_entry.d_name[0] == 0)
            {
                entry->file_data.folder_child.done_reading = true;
                break;
            }
            memcpy((void*)((char*)arg2 + bytes_read), &dir_entry, sizeof(dir_entry));
            bytes_read += sizeof(dir_entry);
        }
        sc_ret(1) = bytes_read;
        unlock_scheduler();
        break;
    sc_case(SYS_IOCTL, 3, int, unsigned long, void*)
        SC_LOG("syscall SYS_IOCTL(%d, %#lx, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg3);
        lock_scheduler();
        syscall_ioctl(registers, arg1, arg2, arg3);
        unlock_scheduler();
        break;
    sc_case(SYS_EXECVE, 3, const char*, char**, char**)
        SC_LOG("syscall SYS_EXECVE(\"%s\", %p, %p)", arg1, arg2, arg3);
    // TODO: Validate each pointer individually
        sc_validate_pointer(arg1);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
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

            thread_t* parent = find_task_by_pid_anywhere(new_task->ppid);
            if (parent)
                tq_hashmap_push_back(pid_to_children_tq_hashmap, parent->pid, new_task);

            unlock_scheduler();
            assert(task_lock_depth == 0);
            switch_task();
            break;
        }
        break;
    sc_case(SYS_TCGETATTR, 2, int, struct termios*)
        SC_LOG("syscall SYS_TCGETATTR(%d, %p)", arg1, arg2);
        sc_validate_pointer(arg2);
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
        sc_validate_pointer(arg3);
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
        // LOG(TRACE, "TCSETATTR: .c_iflag = %#o", arg3->c_iflag);
        // LOG(TRACE, "TCSETATTR: .c_oflag = %#o", arg3->c_oflag);
        // LOG(TRACE, "TCSETATTR: .c_cflag = %#o", arg3->c_cflag);
        // LOG(TRACE, "TCSETATTR: .c_lflag = %#o", arg3->c_lflag);
        // LOG(TRACE, "TCSETATTR: .c_line = `%c`", arg3->c_line);
        unlock_scheduler();
        sc_ret_errno = 0;
        break;
    sc_case(SYS_GETCWD, 2, char*, size_t)
        SC_LOG("syscall SYS_GETCWD(%p, %zu)", arg1, arg2);
        sc_validate_pointer(arg1);
        if (arg2 == 0)
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
        sc_validate_pointer(arg1);
        vfs_folder_tnode_t* tnode = vfs_get_folder_tnode(arg1, current_task->cwd);
        if (!tnode)
        {
            sc_ret_errno = vfs_get_file_tnode(arg1, current_task->cwd) ? ENOTDIR : ENOENT;
            break;
        }
        if (vfs_access(arg1, current_task->cwd, X_OK) != 0)
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
        lock_scheduler();
        if (current_task->pid == forked_pid)
            sc_ret(1) = 0;
        else
            sc_ret(1) = forked_pid;
        unlock_scheduler();
        break;
    sc_case(SYS_SIGACTION, 3, int, const struct sigaction*, struct sigaction*)
        SC_LOG("syscall SYS_SIGACTION(%d, %p, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
        if (arg1 < 0 || arg1 >= NUM_SIGNALS || arg1 == SIGKILL || arg1 == SIGSTOP)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        if (arg3) *arg3 = current_task->sig_act_array[arg1];
        if (arg2) current_task->sig_act_array[arg1] = *arg2;
        sc_ret_errno = 0;
        break;
    sc_case(SYS_SIGPROCMASK, 3, int, const sigset_t*, sigset_t*)
        SC_LOG("syscall SYS_SIGPROCMASK(%d, %p, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
        lock_scheduler();
        if (arg3) *arg3 = current_task->sig_mask;
        if (!arg1)
        {
            unlock_scheduler();
            sc_ret_errno = 0;
            break;
        }
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
        SC_LOG("syscall SYS_WAIT4(%d, %p, %#o, %p)", arg1, arg2, arg3, arg4);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg4);
        if (arg3 & WNOWAIT)
        {
            sc_ret_errno = 0;
            break;
        }
        if (arg3 & ~(WNOHANG | WUNTRACED | WCONTINUED))
        {
            LOG(WARNING, "SYS_WAIT4: Invalid or unsupported argument %#o", arg3);
            sc_ret_errno = EINVAL;
            break;
        }
        lock_scheduler();
        pid_t first_pid = waitpid_find_child_in_tq(&dead_tasks, arg1, arg2, current_task->pgid);
        if (!first_pid && !hashmap_get_item(pid_to_children_tq_hashmap, current_task->pid))
        {
            unlock_scheduler();
            sc_ret_errno = ECHILD;
            break;
        }
        if (arg3 & WNOHANG || first_pid)
        {
            sc_ret(1) = first_pid;
            sc_ret_errno = 0;
            if (first_pid)
            {
                thread_t* task = find_task_by_pid_anywhere(first_pid);
                assert(task);
                if (arg2) *arg2 = task->return_value;
                move_task_to_queue(&reapable_tasks, task);
            }
            unlock_scheduler();
            break;
        }
        current_task->wait_pid = arg1;
        current_task->pgid_on_waitpid = current_task->pgid;
        current_task->waitpid_flags = arg3;
        current_task->waitpid_ret = -1;
        move_running_task_to_thread_queue(&waitpid_tasks, current_task);
        unlock_scheduler();
        waitpid_check_dead();
        assert(task_lock_depth == 0);
        switch_task();
        lock_scheduler();
        if (arg4)
            memset(arg4, 0, sizeof(struct rusage));
        if (arg2) *arg2 = current_task->wstatus;
        sc_ret(1) = current_task->waitpid_ret;
        // * Interrupted
        if (current_task->waitpid_ret == -1)
            sc_ret_errno = should_restart_syscall() ? ERESTART : EINTR;
        else
            sc_ret_errno = 0;
        unlock_scheduler();
        break;
    sc_case(SYS_TTYNAME, 3, int, char*, size_t)
        SC_LOG("syscall SYS_TTYNAME(%d, %p, %zu)", arg1, arg2, arg3);
        sc_validate_pointer(arg2);
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
        sc_validate_pointer(arg1);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
        lock_scheduler();
        *arg1 = current_task->ruid;
        *arg2 = current_task->euid;
        *arg3 = current_task->suid;
        unlock_scheduler();
        sc_ret_errno = 0;
        break;
    sc_case(SYS_GETRESGID, 3, uid_t*, uid_t*, uid_t*)
        SC_LOG("syscall SYS_GETRESGID(%p, %p, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg1);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
        lock_scheduler();
        *arg1 = current_task->rgid;
        *arg2 = current_task->egid;
        *arg3 = current_task->sgid;
        unlock_scheduler();
        sc_ret_errno = 0;
        break;
    sc_case(SYS_CLOCK_GET, 3, int, time_t*, long*)
        SC_LOG("syscall SYS_CLOCK_GET(%d, %p, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
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
        sc_validate_pointer(arg1);
        const char* str = sys_nodename;
        const size_t len = strlen(str) + 1;
        if (arg2 <= 0)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        if (len > arg2)
        {
            sc_ret_errno = ENAMETOOLONG;
            break;
        }
        memcpy(arg1, str, len);
        sc_ret_errno = 0;
        break;
    sc_case(SYS_FSTATAT, 4, int, const char*, int, struct stat*)
        SC_LOG("syscall SYS_FSTATAT(%d, \"%s\", %#x, %p)", arg1, arg2, arg3, arg4);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg4);
        // TODO: Implement symbolic links
        if (arg3 & ~(AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW))
        {
            LOG(WARNING, "SYS_FSTATAT: Invalid or unsupported argument %#x", arg3);
            sc_ret_errno = EINVAL;
            break;
        }
        lock_scheduler();
        file_entry_t* entry = get_global_file_entry(arg1);
        if (arg1 != AT_FDCWD && !entry)
        {
            unlock_scheduler();
            sc_ret_errno = EBADF;
            break;
        }
        if ((strcmp(arg2, "") == 0) && (arg3 & AT_EMPTY_PATH))
        {
            *arg4 = entry->st;
            unlock_scheduler();
            sc_ret_errno = 0;
            break;
        }
        bool relative_path = *arg2 != '/';
        vfs_folder_tnode_t* cwd = NULL;
        if (relative_path)
        {
            if (arg1 == AT_FDCWD)
            {
                cwd = current_task->cwd;
                goto fstatat_get_st;
            }
            if (entry->entry_type != VFS_ET_FOLDER)
            {
                sc_ret_errno = ENOTDIR;
                unlock_scheduler();
                break;
            }
            if (entry->entry_type == VFS_ET_FOLDER)
            {
                cwd = entry->tnode.folder;
                goto fstatat_get_st;
            }
            assert(!"fstatat fatal error");
        }
        else
        {
        fstatat_get_st:
            ;
            struct stat st;
            int stat_ret = vfs_stat(arg2, cwd, &st);
            unlock_scheduler();
            if (stat_ret != 0)
            {
                sc_ret_errno = stat_ret;
                break;
            }

            *arg4 = st;
            sc_ret_errno = 0;
            break;
        }
        break;
    sc_case(SYS_ACCESS, 2, const char*, int)
        SC_LOG("syscall SYS_ACCESS(\"%s\", %#o)", arg1, arg2);
        sc_validate_pointer(arg1);
        if (arg2 & ~(R_OK | W_OK | X_OK)) // * F_OK is 0
        {
            LOG(WARNING, "SYS_ACCESS: Invalid or unsupported argument %#o", arg2);
            sc_ret_errno = EINVAL;
            break;
        }
        sc_ret_errno = vfs_access(arg1, current_task->cwd, arg2);
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
        if (!process || process == idle_task)
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
        int ret = vfs_dup(arg1);
        if (ret >= 0)
        {
            sc_ret_errno = 0;
            sc_ret(1) = ret;
        }
        else
            sc_ret_errno = -ret;
        break;
    sc_case(SYS_DUP3, 3, int, int, int)
        SC_LOG("syscall SYS_DUP3(%d, %#o, %d)", arg1, arg2, arg3);
        if (arg1 == arg3)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        if (arg2 & ~O_CLOEXEC)
        {
            LOG(WARNING, "SYS_DUP3: Invalid or unsupported argument %#o", arg2);
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
        SC_LOG("syscall SYS_FCNTL(%d, %d, %" PRIu64 ")", arg1, arg2, arg3);
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
        case F_DUPFD_CLOEXEC:
        case F_DUPFD:
            ;
            int ret = vfs_dup(arg1);
            if (ret >= 0)
            {
                sc_ret_errno = 0;
                sc_ret(1) = ret;
                if (arg2 == F_DUPFD_CLOEXEC)
                    current_task->file_table[ret].flags |= FD_CLOEXEC;
            }
            else
                sc_ret_errno = -ret;
            break;
        default:
            LOG(WARNING, "Unknown fcntl request (%d, %d, %" PRIu64 ")", arg1, arg2, arg3);
            // task_send_signal(current_task, SIGILL);
            sc_ret_errno = ENOSYS;
        }
        unlock_scheduler();
        break;
    sc_case(SYS_SIGRET, 0)
        SC_LOG("syscall SYS_SIGRET()");
        *return_address = sigret;
        sc_ret_errno = 0;
        // LOG(TRACE, "poped ret rsp:  %#16" PRIx64, registers->rsp);
        // hexdump((void*)registers->rsp, sizeof(interrupt_registers_t));
        break;
    sc_case(SYS_PSELECT, 6, int, fd_set*, fd_set*, fd_set*, const struct timespec*, const sigset_t*)
        SC_LOG("syscall SYS_PSELECT(%d, %p, %p, %p, %p, %p)", arg1, arg2, arg3, arg4, arg5, arg6);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
        sc_validate_pointer(arg4);
        sc_validate_pointer(arg5);
        sc_validate_pointer(arg6);
        lock_scheduler();
        // * sigmask
        sigset_t saved_sigmask = current_task->sig_mask;
        if (arg6)
            current_task->sig_mask = *arg6;

        if (arg1 == 0 || (!arg2 && !arg3 && !arg4))
        {
            sc_ret_errno = 0;
            sc_ret(1) = 0;
            if (arg5)
            {
                current_task->timeout_deadline = global_timer + arg5->tv_nsec * PRECISE_NANOSECONDS + arg5->tv_sec * PRECISE_SECONDS;
                move_running_task_to_thread_queue(&waiting_for_time_tasks, current_task);
                switch_task();
                unlock_scheduler();
                lock_scheduler();
            }
            else
                current_task->timeout_deadline = 0;
            if (current_task->timeout_deadline >= global_timer)
                sc_ret_errno = should_restart_syscall() ? ERESTART : EINTR;
            current_task->sig_mask = saved_sigmask;
            unlock_scheduler();
            break;
        }

        bool shouldblock = !(arg5 && arg5->tv_nsec == 0 && arg5->tv_sec == 0);

        fd_set read_ret, write_ret;

        do
        {
            sc_ret_errno = 0;
            sc_ret(1) = 0;

            if (arg2) read_ret = *arg2;
            if (arg3) write_ret = *arg3;

            if (arg2)
            {
                for (int i = 0; i < arg1; i++)
                {
                    if (!FD_ISSET(i, &read_ret)) continue;
                    file_entry_t* entry = get_global_file_entry(i);
                    if (!entry)
                    {
                        FD_CLR(i, &read_ret);
                        continue;
                    }
                    if (vfs_willblock(entry, POLLIN))
                        FD_CLR(i, &read_ret);
                    else
                        shouldblock = false;
                }
            }
            if (arg3)
            {
                for (int i = 0; i < arg1; i++)
                {
                    if (!FD_ISSET(i, &write_ret)) continue;
                    file_entry_t* entry = get_global_file_entry(i);
                    if (!entry)
                    {
                        FD_CLR(i, &write_ret);
                        continue;
                    }
                    if (vfs_willblock(entry, POLLOUT))
                        FD_CLR(i, &write_ret);
                    else
                        shouldblock = false;
                }
            }
            // * Ignore exceptional conditions
            if (arg4)
                FD_ZERO(arg4);
            if (shouldblock)
            {
                if (arg2 || arg3)
                {
                    for (int i = 0; i < arg1; i++)
                    {
                        if (!((arg2 ? FD_ISSET(i, arg2) : false) || (arg3 ? FD_ISSET(i, arg3) : false))) continue;
                        file_entry_t* entry = get_global_file_entry(i);
                        if (!entry)
                            continue;
                        if (arg2 ? FD_ISSET(i, arg2) : false)
                            if (vfs_willblock(entry, POLLIN))
                                goto pselect_monitor;
                        if (arg3 ? FD_ISSET(i, arg3) : false)
                            if (vfs_willblock(entry, POLLOUT))
                                goto pselect_monitor;

                        continue;

                    pselect_monitor:
                        task_monitor_entry(current_task, entry);
                    }
                }
                task_start_polling(current_task, arg5 ? arg5->tv_nsec * PRECISE_NANOSECONDS + arg5->tv_sec * PRECISE_SECONDS : NO_TIMEOUT);
                switch_task();
                unlock_scheduler();
                lock_scheduler();
                if (current_task->sig_pending_user_space)
                {
                    sc_ret_errno = should_restart_syscall() ? ERESTART : EINTR;
                    shouldblock = false;
                }
            }
        } while (shouldblock);

        if (sc_ret_errno != ERESTART)
        {
            if (arg2) *arg2 = read_ret;
            if (arg3) *arg3 = write_ret;

            for (int i = 0; i < arg1; i++)
            {
                if (arg2)
                    sc_ret(1) += FD_ISSET(i, arg2) ? 1 : 0;
                if (arg3)
                    sc_ret(1) += FD_ISSET(i, arg3) ? 1 : 0;
                if (arg4)
                    sc_ret(1) += FD_ISSET(i, arg4) ? 1 : 0;
            }
        }

        current_task->sig_mask = saved_sigmask;
        unlock_scheduler();
        break;
    sc_case(SYS_FADVISE, 4, int, off_t, off_t, int)
        SC_LOG("syscall SYS_FADVISE(%d, %ld, %ld, %d)", arg1, arg2, arg3, arg4);
        sc_ret_errno = 0;
        break;
    sc_case(SYS_PIPE2, 2, int*, int)
        SC_LOG("syscall SYS_PIPE2(%p, %#o)", arg1, arg2);
        sc_validate_pointer(arg1);
        // * Don't support packet mode for now
        if (arg2 & ~(O_CLOEXEC | O_NONBLOCK))
        {
            LOG(WARNING, "SYS_PIPE2: Invalid or unsupported argument %#o", arg2);
            sc_ret_errno = EINVAL;
            break;
        }
        sc_ret_errno = vfs_setup_pipe(arg1, arg2);
        break;

    sc_case(SYS_KILL, 2, int, int)
        SC_LOG("syscall SYS_KILL(%d, %d)", arg1, arg2);
        if (arg2 < 0 || arg2 >= NUM_SIGNALS)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        lock_scheduler();
        if (arg1 > 0)
        {
            thread_t* task = find_task_by_pid_anywhere(arg1);
            if (!task)
            {
                sc_ret_errno = ESRCH;
                unlock_scheduler();
                break;
            }
            if (task->pid == 0)
            {
                sc_ret_errno = EPERM;
                unlock_scheduler();
                break;
            }
            task_send_signal(task, arg2);
        }
        else if (arg1 == 0 || arg1 < -1)
        {
            pid_t pgrp = arg1 ? -arg1 : current_task->pgid;
            task_send_signal_to_pgrp(arg2, pgrp);
        }
        else    // arg1 == -1
        {
        // * then  sig is sent to every process for which the calling process has permission to send signals, except for process 1
            assert(!"Not implemented");
        }
        unlock_scheduler();
        sc_ret_errno = 0;
        break;

    sc_case(SYS_UNAME, 1, struct utsname*)
        SC_LOG("syscall SYS_UNAME(%p)", arg1);
        sc_validate_pointer(arg1);
        sc_ret_errno = 0;
        strncpy(arg1->sysname, sys_sysname, sizeof(arg1->sysname));
        strncpy(arg1->nodename, sys_nodename, sizeof(arg1->nodename));
        strncpy(arg1->release, sys_release, sizeof(arg1->release));
        strncpy(arg1->version, sys_version, sizeof(arg1->version));
        strncpy(arg1->machine, sys_machine, sizeof(arg1->machine));
        strncpy(arg1->domainname, sys_domainname, sizeof(arg1->domainname));
        break;

    sc_case(SYS_FSYNC, 1, int)
        SC_LOG("syscall SYS_FSYNC(%d)", arg1);
        sc_ret_errno = 0;
        break;

    sc_case(SYS_SLEEP, 2, time_t*, long*)
        SC_LOG("syscall SYS_SLEEP(%lld, %ld)", (long long)*arg1, (long)*arg2);
        sc_validate_pointer(arg1);
        sc_validate_pointer(arg2);
        if (*arg1 < 0 || *arg2 >= 1000000000)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        precise_time_t deadline = global_timer + *arg2 * PRECISE_NANOSECONDS + *arg1 * PRECISE_SECONDS;
        current_task->timeout_deadline = deadline;
        move_running_task_to_thread_queue(&waiting_for_time_tasks, current_task);
        switch_task();
        const precise_time_t timer = global_timer;
        if (timer <= deadline)
        {
            sc_ret_errno = EINTR;
            *arg1 = (deadline - timer) / PRECISE_SECONDS;
            *arg2 = ((deadline - timer) / PRECISE_NANOSECONDS) % 1000000000;
            break;
        }
        *arg1 = 0;
        *arg2 = 0;
        sc_ret_errno = 0;
        break;

    sc_case(SYS_POLL, 3, struct pollfd*, nfds_t, int)
        SC_LOG("syscall SYS_POLL(%p, %lu, %d)", arg1, arg2, arg3);
        // TODO: Validate the full array AND THE KERNEL STACK SIZE
        sc_validate_pointer(arg1);
        const size_t array_bytes = sizeof(struct pollfd) * arg2;
        struct pollfd* ret = alloca(array_bytes);
        lock_scheduler();
        bool shouldblock = arg3 != 0;
        do
        {
            sc_ret_errno = 0;
            sc_ret(1) = 0;
            for (nfds_t i = 0; i < arg2; i++)
            {
                ret[i] = arg1[i];
                struct pollfd* fds = &ret[i];
                file_entry_t* entry = get_global_file_entry(fds->fd);
                int set = 0;
                if (entry)
                {
                    fds->revents = 0;
                    if ((fds->events & POLLIN) || (fds->events & POLLRDNORM))
                    {
                        if (!vfs_willblock(entry, POLLIN))
                            fds->revents |= (set = 1, shouldblock = false, POLLIN | vfs_hup(entry));
                    }
                    if ((fds->events & POLLOUT) || (fds->events & POLLWRNORM))
                    {
                        if (!vfs_willblock(entry, POLLOUT))
                            fds->revents |= (set = 1, shouldblock = false, POLLOUT | vfs_hup(entry));
                    }
                }
                else
                    fds->revents = fds->fd < 0 ? 0 : (set = 1, POLLNVAL);
                sc_ret(1) += set;
            }
            if (shouldblock)
            {
                for (nfds_t i = 0; i < arg2; i++)
                {
                    struct pollfd* fds = &arg1[i];
                    file_entry_t* entry = get_global_file_entry(fds->fd);
                    if (!entry)
                        continue;
                    if ((fds->events & POLLIN) || (fds->events & POLLRDNORM))
                        if (vfs_willblock(entry, POLLIN))
                            goto poll_monitor;
                    if ((fds->events & POLLOUT) || (fds->events & POLLWRNORM))
                        if (vfs_willblock(entry, POLLOUT))
                            goto poll_monitor;

                    continue;

                poll_monitor:
                    // SC_LOG("Starting poll on fd %d", fds->fd);
                    task_monitor_entry(current_task, entry);
                }
                task_start_polling(current_task, PRECISE_MILLISECONDS * arg3);
                switch_task();
                unlock_scheduler();
                lock_scheduler();
                if (current_task->sig_pending_user_space)
                {
                    sc_ret_errno = should_restart_syscall() ? ERESTART : EINTR;
                    shouldblock = false;
                }
            }
        } while (shouldblock);
        if (sc_ret_errno != ERESTART)
            memcpy(arg1, ret, array_bytes);
        unlock_scheduler();
        break;
    sc_case(SYS_GETAFFINITY, 3, pid_t, size_t, cpu_set_t*)
        SC_LOG("syscall SYS_GETAFFINITY(%d, %zu, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg3);
        if (arg2 < sizeof(cpu_set_t))
        {
            sc_ret_errno = EINVAL;
            break;
        }
        thread_t* task = arg1 == 0 ? current_task : find_task_by_pid_anywhere(arg1);
        if (!task)
        {
            sc_ret_errno = ESRCH;
            break;
        }
        sc_ret_errno = 0;
        CPU_ZERO(arg3);
        CPU_SET(0, arg3);   // * No SMP for now
        break;

    sc_case(SYS_UMASK, 2, mode_t, mode_t*)
        SC_LOG("syscall SYS_UMASK(%#o, %p)", arg1, arg2);
        sc_validate_pointer(arg2);
        if (arg2)
            *arg2 = current_task->umask;
        current_task->umask = arg1 & 0777;
        sc_ret_errno = 0;
        break;

    sc_case(SYS_LOG, 1, const char*)
        sc_validate_pointer(arg1);
    #ifdef PRINT_MLIBC_LOGS
        puts(arg1);
    #endif
        LOG(DEBUG, "%s", arg1);
        sc_ret_errno = 0;
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
        LOG(WARNING, "syscall %" PRIu64 " not implemented", registers->rax);
        // task_send_signal(current_task, SIGILL);
        sc_ret_errno = ENOSYS;
    }
    }

    if (sc_ret_errno != 0 && !sc_no_errno)
        SC_LOG("errno: %" PRId64 ": %s", sc_ret_errno, strerror(sc_ret_errno));

    if (registers->rax != SYS_SIGRET)
        task_handle_signal_to_userspace(registers);

    return registers->rsp;
}
