#define _GNU_SOURCE
#include "abi-bits/fcntl.h"
#include "sched_lock.h"
#include <limits.h>
#include <stdio.h>
#include "loader.h"
#include "../terminal/textio.h"
#include "../vfs/vnode.h"
#include "hashmap.h"
#include "startup_data.h"
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
#include <fcntl.h>
#include <assert.h>
#include "signal.h"
#include "multitasking.h"
#include <dirent.h>
#include <sys/utsname.h>
#include "../system/info.h"
#include <sys/poll.h>
#include <sched.h>
#include "../cpu/tsc.h"
#include "../vfs/open_file_descriptor_vector.h"

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
    uint32_t flags = lock_scheduler();
    if (current_task->sig_pending_user_space && !current_task->waiting_for_kill)
    {
        if (current_task->pending_signal_handler)
        {
            setup_user_signal_stack_frame__interrupt(registers);
            current_task->sig_pending_user_space = false;
        }
        else
            __task_handle_sig_dfl(current_task, current_task->pending_signal_number);
    }
    unlock_scheduler(flags);
}

uint64_t c_syscall_handler(interrupt_registers_t* registers, void** return_address)
{
    ASSERT(get_rflags() & (1 << 9));
    task_enter_critical_section(current_task);
    uint64_t syscall_num = registers->rax;
    sc_ret_errno = -1;
    bool sc_no_errno = syscall_num == SYS_GETPID || syscall_num == SYS_GETPPID || syscall_num == SYS_LOG;
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
        open_file_descriptor_ref_t open_file = ofdv_get(&current_task->open_file_table, arg1).fd;
        if (!open_file.ptr)
        {
            sc_ret_errno = EBADF;
            sc_ret(1) = (uint64_t)-1;
            break;
        }
        ASSERT(open_file.ptr->read);
        ssize_t ret = open_file.ptr->read(open_file.ptr, arg2, arg3);
        if (ret < 0)
        {
            sc_ret_errno = (uint64_t)-ret;
            ret = -1;
        }
        open_file_descriptor_delete_ref(&open_file);
        sc_ret(1) = (uint64_t)ret;
        break;
    sc_case(SYS_WRITE, 3, int, void*, size_t)
        SC_LOG("syscall SYS_WRITE(%d, %p, %zu)", arg1, arg2, arg3);
        sc_validate_pointer(arg2);
        open_file_descriptor_ref_t open_file = ofdv_get(&current_task->open_file_table, arg1).fd;
        if (!open_file.ptr)
        {
            sc_ret_errno = EBADF;
            sc_ret(1) = (uint64_t)-1;
            break;
        }
        ASSERT(open_file.ptr->write);
        ssize_t ret = open_file.ptr->write(open_file.ptr, arg2, arg3);
        if (ret < 0)
        {
            sc_ret_errno = (uint64_t)-ret;
            ret = -1;
        }
        open_file_descriptor_delete_ref(&open_file);
        sc_ret(1) = (uint64_t)ret;
        break;
    sc_case(SYS_EXIT, 1, int)
        SC_LOG("syscall SYS_EXIT(%d)", arg1);
        kill_task(current_task, ((uint16_t)arg1 & 0x7f) << 8);
        break;
    sc_case(SYS_ISATTY, 1, int)
        SC_LOG("syscall SYS_ISATTY(%d)", arg1);
        sc_ret(0) = vfs_isatty(arg1);
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
        open_file_descriptor_ref_t open_file = ofdv_get(&current_task->open_file_table, arg1).fd;
        if (!open_file.ptr)
        {
            sc_ret_errno = EBADF;
            sc_ret(1) = (uint64_t)((off_t)-1);
            break;
        }
        if (arg3 != SEEK_SET && arg3 != SEEK_CUR && arg3 != SEEK_END)
        {
            open_file_descriptor_delete_ref(&open_file);
            sc_ret_errno = EINVAL;
            sc_ret(1) = (uint64_t)((off_t)-1);
            break;
        }
        struct stat st = open_file.ptr->st;
        // ESPIPE fd is associated with a pipe, socket, or FIFO.
        if (S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode))
        {
            open_file_descriptor_delete_ref(&open_file);
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
            offset += open_file.ptr->offset;
            goto do_seek;
        case SEEK_END:
            offset += st.st_size;
            goto do_seek;
        }
        if (false)
        {
        do_seek:
            if (offset < 0) // || offset >= entry->tnode.file->inode->st.st_size)
            {
                open_file_descriptor_delete_ref(&open_file);
                sc_ret_errno = EINVAL;
                sc_ret(1) = (uint64_t)((off_t)-1);
                break;
            }
            open_file.ptr->offset = offset;
            open_file_descriptor_delete_ref(&open_file);
            sc_ret_errno = 0;
            sc_ret(1) = (uint64_t)offset;
            break;
        }
        open_file_descriptor_delete_ref(&open_file);
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

        open_file_descriptor_ref_t dir_open_file;
        if (arg1 != AT_FDCWD)
        {
            dir_open_file = ofdv_get(&current_task->open_file_table, arg1).fd;
            if (!dir_open_file.ptr)
            {
                sc_ret_errno = EBADF;
                break;
            }
            if (!S_ISDIR(dir_open_file.ptr->st.st_mode))
            {
                open_file_descriptor_delete_ref(&dir_open_file);
                sc_ret_errno = ENOTDIR;
                break;
            }
        }
        vnode_ref_t cwd = (arg1 == AT_FDCWD) ? task_get_cwd(current_task) : dir_open_file.ptr ? vnode_copy_ref(dir_open_file.ptr->vnode) : (vnode_ref_t){ NULL };
        open_file_descriptor_delete_ref(&dir_open_file);

        int _errno;
        vnode_ref_t file_node = vfs_get_vnode_from_path(&_errno, 0, 0, arg2, vfs_root_node, cwd, (arg3 & O_NOFOLLOW) ? false : true);
        vnode_delete_ref(&cwd);
        if (!file_node.ptr)
        {
            if (_errno == ENOENT && (arg3 & O_CREAT))
                sc_ret_errno = EROFS; // TODO: Add file creation support
            else
                sc_ret_errno = _errno;
            break;
        }

        struct stat st = vnode_stat(file_node);

        if ((arg3 & O_DIRECTORY) && !S_ISDIR(st.st_mode))
        {
            sc_ret_errno = ENOTDIR;
            break;
        }

        open_file_descriptor_ref_t open_file = vfs_create_new_file_descriptor_from_vnode(&_errno, arg3, file_node, &st);
        vnode_delete_ref(&file_node);
        if (!open_file.ptr)
        {
            sc_ret_errno = _errno;
            break;
        }

        int fd = ofdv_open(&current_task->open_file_table, (open_file_descriptor_entry_t){ .flags = arg3 & O_CLOEXEC ? FD_CLOEXEC : 0, .fd = open_file });
        open_file_descriptor_delete_ref(&open_file);
        if (fd < 0)
        {
            sc_ret_errno = -fd;
            break;
        }
        sc_ret_errno = 0;
        sc_ret(1) = fd;
        break;
    sc_case(SYS_CLOSE, 1, int)
        SC_LOG("syscall SYS_CLOSE(%d)", arg1);
        ofdv_close(&current_task->open_file_table, arg1);
        sc_ret_errno = 0;
        break;
    sc_case(SYS_READ_ENTRIES, 3, int, void*, size_t)
        SC_LOG("syscall SYS_READ_ENTRIES(%d, %p, %zu)", arg1, arg2, arg3);
        sc_validate_pointer(arg2);
        open_file_descriptor_ref_t open_file = ofdv_get(&current_task->open_file_table, arg1).fd;
        if (!open_file.ptr)
        {
            sc_ret_errno = ENOMEM;
            break;
        }
        struct dirent64 dir_entry;
        size_t bytes_read = 0;
        sc_ret_errno = 0;
        while (true)
        {
            if (bytes_read + sizeof(dir_entry) > arg3)
            {
                if (bytes_read == 0)
                    sc_ret_errno = EINVAL;
                break;
            }
            dir_entry = vfs_find_new_child_entry(open_file);
            if (dir_entry.d_name[0] == 0)
                break;
            memcpy((void*)((char*)arg2 + bytes_read), &dir_entry, sizeof(dir_entry));
            bytes_read += sizeof(dir_entry);
        }
        open_file_descriptor_delete_ref(&open_file);
        sc_ret(1) = bytes_read;
        break;
    sc_case(SYS_IOCTL, 3, int, unsigned long, void*)
        SC_LOG("syscall SYS_IOCTL(%d, %#lx, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg3);
        syscall_ioctl(registers, arg1, arg2, arg3);
        break;
    sc_case(SYS_EXECVE, 3, const char*, char**, char**)
        SC_LOG("syscall SYS_EXECVE(\"%s\", %p, %p)", arg1, arg2, arg3);
    // TODO: Validate each pointer individually
        sc_validate_pointer(arg1);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
        startup_data_struct_t data = startup_data_init_from_command(arg2, arg3);
        uint32_t flags = lock_scheduler();
        vnode_ref_t cwd = __task_get_cwd(current_task);
        thread_t* new_task = __multitasking_add_task_from_vfs(arg1, arg1, 3, false, &data, cwd);
        vnode_delete_ref(&cwd);
        if (!new_task)
        {
            LOG(TRACE, "EXECVE: Couldn't load executable");
            unlock_scheduler(flags);
            sc_ret_errno = ENOENT;
            break;
        }
        else
        {
            pid_t old_pid = current_task->pid;

            __task_set_pid(current_task, task_generate_pid());
            __task_set_pid(new_task, old_pid);

            new_task->ppid = current_task->ppid;
            __task_set_pgid(new_task, current_task->pgid);

            new_task->sid = current_task->sid;

            task_copy_file_table(current_task, new_task, true);

            __move_running_task_to_thread_queue(&reapable_tasks, current_task);

            thread_t* parent = __find_task_by_pid_anywhere(new_task->ppid);
            if (parent)
                __tq_hashmap_push_back(pid_to_children_tq_hashmap, parent->pid, new_task);

            switch_task();
            unlock_scheduler(flags);
            break;
        }
        break;
    sc_case(SYS_TCGETATTR, 2, int, struct termios*)
        SC_LOG("syscall SYS_TCGETATTR(%d, %p)", arg1, arg2);
        sc_validate_pointer(arg2);
        int _errno = vfs_isatty(arg1);
        if (_errno)
        {
            sc_ret_errno = _errno;
            break;
        }
        *arg2 = tty_ts;
        sc_ret_errno = 0;
        break;
    sc_case(SYS_TCSETATTR, 3, int, int, const struct termios*)
        SC_LOG("syscall SYS_TCSETATTR(%d, %d, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg3);
        int _errno = vfs_isatty(arg1);
        if (_errno)
        {
            sc_ret_errno = _errno;
            break;
        }
        tty_ts = *arg3;
        if (arg2 == TCSAFLUSH)
        {
            uint32_t flags = acquire_spinlock_noint(&keyboard_input_lock);
            keyboard_buffered_input_buffer.get_index = keyboard_buffered_input_buffer.put_index = 0;
            keyboard_input_buffer.get_index = keyboard_input_buffer.put_index = 0;
            release_spinlock_noint(&keyboard_input_lock, flags);
        }
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
        vnode_ref_t cwd = task_get_cwd(current_task);
        size_t ret = vfs_get_absolute_path_to_node(cwd, buf_cpy, sizeof(buf_cpy));
        vnode_delete_ref(&cwd);
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
        uint32_t flags = lock_scheduler();
        vnode_ref_t old_cwd = __task_get_cwd(current_task);
        vnode_delete_ref(&current_task->cwd);
        int _errno;
        vnode_ref_t new_cwd = vfs_get_vnode_from_path(&_errno, 0, 0, arg1, vfs_root_node, old_cwd, true);
        if (!new_cwd.ptr)
        {
            unlock_scheduler(flags);
            sc_ret_errno = _errno;
            break;
        }
        if (!S_ISDIR(vnode_stat(new_cwd).st_mode))
        {
            unlock_scheduler(flags);
            sc_ret_errno = ENOTDIR;
            break;
        }
        current_task->cwd = new_cwd;
        unlock_scheduler(flags);
        sc_ret_errno = 0;
        break;
    sc_case(SYS_FCHDIR, 1, int)
        SC_LOG("syscall SYS_FCHDIR(%d)", arg1);
        open_file_descriptor_ref_t open_file = ofdv_get(&current_task->open_file_table, arg1).fd;
        if (!open_file.ptr)
        {
            sc_ret_errno = EBADF;
            break;
        }
        if (!S_ISDIR(open_file.ptr->st.st_mode))
        {
            sc_ret_errno = ENOTDIR;
            break;
        }
        // TODO: Search permissions
        uint32_t flags = lock_scheduler();
        vnode_delete_ref(&current_task->cwd);
        current_task->cwd = vnode_copy_ref(open_file.ptr->vnode);
        unlock_scheduler(flags);
        open_file_descriptor_delete_ref(&open_file);
        break;
    sc_case(SYS_FORK, 0)
        SC_LOG("syscall SYS_FORK()");
        sc_ret_errno = 0;
        uint32_t flags = lock_scheduler();
        current_task->forked_pid = task_generate_pid();
        pid_t forked_pid = current_task->forked_pid;
        __move_running_task_to_thread_queue(&forked_tasks, current_task);
        unlock_scheduler(flags);
        switch_task();
        if (current_task->pid == forked_pid)
            sc_ret(1) = 0;
        else
            sc_ret(1) = forked_pid;
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
        if (arg2 || arg3)
        {
            uint32_t flags = lock_scheduler();
            if (arg3) *arg3 = current_task->sig_act_array[arg1];
            if (arg2) current_task->sig_act_array[arg1] = *arg2;
            unlock_scheduler(flags);
        }
        sc_ret_errno = 0;
        break;
    sc_case(SYS_SIGPROCMASK, 3, int, const sigset_t*, sigset_t*)
        SC_LOG("syscall SYS_SIGPROCMASK(%d, %p, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
        if (arg3) *arg3 = current_task->sig_mask;
        if (!arg1)
        {
            sc_ret_errno = 0;
            break;
        }
        sigset_t old_sigmask = current_task->sig_mask;
        uint32_t flags = lock_scheduler();
        switch (arg1)
        {
        case SIG_BLOCK:
        	current_task->sig_mask = sigset_bitwise_or(*arg2, current_task->sig_mask);
            __task_unmask_signal(current_task, SIGKILL);
            __task_unmask_signal(current_task, SIGSTOP);
        	sc_ret_errno = 0;
        	break;
        case SIG_UNBLOCK:
           	current_task->sig_mask = sigset_bitwise_and(sigset_bitwise_not(*arg2), current_task->sig_mask);
           	sc_ret_errno = 0;
           	break;
        case SIG_SETMASK:
           	current_task->sig_mask = *arg2;
            __task_unmask_signal(current_task, SIGKILL);
            __task_unmask_signal(current_task, SIGSTOP);
           	sc_ret_errno = 0;
           	break;
        default:
        	sc_ret_errno = EINVAL;
        }
        __task_try_handle_signals(current_task, old_sigmask, current_task->sig_mask);
        unlock_scheduler(flags);
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
        uint32_t flags = lock_scheduler();
        pid_t first_pid = __waitpid_find_child_in_tq(&dead_tasks, arg1, arg2, current_task->pgid);
        if (!first_pid && !hashmap_get_item(pid_to_children_tq_hashmap, current_task->pid))
        {
            unlock_scheduler(flags);
            sc_ret_errno = ECHILD;
            break;
        }
        if (arg3 & WNOHANG || first_pid)
        {
            sc_ret(1) = first_pid;
            sc_ret_errno = 0;
            if (first_pid)
            {
                thread_t* task = __find_task_by_pid_anywhere(first_pid);
                ASSERT(task);
                if (arg2) *arg2 = task->return_value;
                __move_task_to_queue(&reapable_tasks, task);
            }
            unlock_scheduler(flags);
            break;
        }
        current_task->wait_pid = arg1;
        current_task->pgid_on_waitpid = current_task->pgid;
        current_task->waitpid_flags = arg3;
        current_task->waitpid_ret = -1;
        __move_running_task_to_thread_queue(&waitpid_tasks, current_task);
        unlock_scheduler(flags);
        __waitpid_check_dead();
        switch_task();
        flags = lock_scheduler();
        if (arg4)
            memset(arg4, 0, sizeof(struct rusage));
        if (arg2) *arg2 = current_task->wstatus;
        sc_ret(1) = current_task->waitpid_ret;
        // * Interrupted
        if (current_task->waitpid_ret == -1)
            sc_ret_errno = should_restart_syscall() ? ERESTART : EINTR;
        else
            sc_ret_errno = 0;
        unlock_scheduler(flags);
        break;
    sc_case(SYS_GETRESUID, 3, uid_t*, uid_t*, uid_t*)
        SC_LOG("syscall SYS_GETRESUID(%p, %p, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg1);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
        uint32_t flags = lock_scheduler();
        *arg1 = current_task->ruid;
        *arg2 = current_task->euid;
        *arg3 = current_task->suid;
        unlock_scheduler(flags);
        sc_ret_errno = 0;
        break;
    sc_case(SYS_GETRESGID, 3, uid_t*, uid_t*, uid_t*)
        SC_LOG("syscall SYS_GETRESGID(%p, %p, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg1);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
        uint32_t flags = lock_scheduler();
        *arg1 = current_task->rgid;
        *arg2 = current_task->egid;
        *arg3 = current_task->sgid;
        unlock_scheduler(flags);
        sc_ret_errno = 0;
        break;
    sc_case(SYS_CLOCK_GET, 3, int, time_t*, long*)
        SC_LOG("syscall SYS_CLOCK_GET(%d, %p, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
        switch (arg1)
        {
        case CLOCK_REALTIME:
            *arg2 = current_time.tv_sec;
            *arg3 = current_time.tv_nsec;
            sc_ret_errno = 0;
            break;
        default:
            sc_ret_errno = EINVAL;
        }
        break;
    sc_case(SYS_GETPID, 0)
        SC_LOG("syscall SYS_GETPID()");
        sc_ret(0) = (uint64_t)current_task->pid;
        break;
    sc_case(SYS_GETPPID, 0)
        SC_LOG("syscall SYS_GETPPID()");
        sc_ret(0) = (uint64_t)current_task->ppid;
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
        if (arg3 & ~(AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW))
        {
            LOG(WARNING, "SYS_FSTATAT: Invalid or unsupported argument %#x", arg3);
            sc_ret_errno = EINVAL;
            break;
        }
        open_file_descriptor_ref_t dir_open_file;
        if (arg1 != AT_FDCWD)
        {
            dir_open_file = ofdv_get(&current_task->open_file_table, arg1).fd;
            if (!dir_open_file.ptr)
            {
                sc_ret_errno = EBADF;
                break;
            }
            if (!S_ISDIR(dir_open_file.ptr->st.st_mode))
            {
                open_file_descriptor_delete_ref(&dir_open_file);
                sc_ret_errno = ENOTDIR;
                break;
            }
        }
        vnode_ref_t cwd = (arg1 == AT_FDCWD) ? task_get_cwd(current_task) : dir_open_file.ptr ? vnode_copy_ref(dir_open_file.ptr->vnode) : (vnode_ref_t){ NULL };
        open_file_descriptor_delete_ref(&dir_open_file);
        if (!arg2[0])
        {
            if (arg3 & AT_EMPTY_PATH)
            {
                *arg4 = vnode_stat(cwd);
                break;
            }
            else
            {
                sc_ret_errno = ENOENT;
                break;
            }
        }
        int _errno;
        vnode_ref_t vnode = vfs_get_vnode_from_path(&_errno, 0, 0, arg2, vfs_root_node, cwd, (arg3 & AT_SYMLINK_NOFOLLOW) ? false : true);
        vnode_delete_ref(&cwd);
        if (!vnode.ptr)
        {
            sc_ret_errno = _errno;
            break;
        }
        *arg4 = vnode_stat(vnode);
        vnode_delete_ref(&vnode);
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
        vnode_ref_t cwd = task_get_cwd(current_task);
        int _errno;
        vnode_ref_t vnode = vfs_get_vnode_from_path(&_errno, 0, 0, arg1, vfs_root_node, cwd, true);
        vnode_delete_ref(&cwd);
        if (!vnode.ptr)
        {
            sc_ret_errno = _errno;
            break;
        }
        mode_t mode = vnode_stat(vnode).st_mode;
        // TODO: Implement proper permission checks
        // The check is done using the calling process's real UID and GID, rather than the effective IDs as is done when actually attempting an operation (e.g., open(2)) on the file.
        vnode_delete_ref(&vnode);
        sc_ret_errno = 0;
        break;
    sc_case(SYS_GETPGID, 1, pid_t)
        SC_LOG("syscall SYS_GETPGID(%d)", arg1);
        if (arg1 < 0)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        uint32_t flags = lock_scheduler();
        if (arg1 == 0)
        {
            sc_ret_errno = 0;
            sc_ret(1) = current_task->pgid;
            unlock_scheduler(flags);
            break;
        }
        thread_t* process = __find_task_by_pid_anywhere(arg1);
        if (!process)
        {
            unlock_scheduler(flags);
            sc_ret_errno = ESRCH;
            break;
        }
        sc_ret(1) = (uint64_t)process->pgid;
        unlock_scheduler(flags);
        sc_ret_errno = 0;
        break;
    sc_case(SYS_SETPGID, 2, pid_t, pid_t)
        SC_LOG("syscall SYS_SETPGID(%d, %d)", arg1, arg2);
        uint32_t flags = lock_scheduler();
        thread_t* process = __find_task_by_pid_anywhere(arg1);
        if (!process || process == idle_task)
        {
            unlock_scheduler(flags);
            sc_ret_errno = ESRCH;
            break;
        }
        __task_set_pgid(process, arg2);
        sc_ret_errno = 0;
        unlock_scheduler(flags);
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
        sc_ret_errno = vfs_dup3(arg1, arg3, arg2);
        break;
    sc_case(SYS_FCNTL, 3, int, int, uint64_t)
        SC_LOG("syscall SYS_FCNTL(%d, %d, %" PRIu64 ")", arg1, arg2, arg3);
        switch (arg2)
        {
        case F_GETFD:
        {
            open_file_descriptor_entry_t entry = ofdv_get(&current_task->open_file_table, arg1);
            if (!entry.fd.ptr)
            {
                sc_ret_errno = EBADF;
                break;
            }
            sc_ret(1) = entry.flags;
            open_file_descriptor_delete_ref(&entry.fd);
            sc_ret_errno = 0;
            break;
        }
        case F_SETFD:
        {
            open_file_descriptor_entry_t entry = ofdv_get(&current_task->open_file_table, arg1);
            if (!entry.fd.ptr)
            {
                sc_ret_errno = EBADF;
                break;
            }
            entry.flags = arg3;
            open_file_descriptor_delete_ref(&entry.fd);
            sc_ret_errno = 0;
            break;
        }
        case F_GETFL:
        {
            open_file_descriptor_entry_t entry = ofdv_get(&current_task->open_file_table, arg1);
            if (!entry.fd.ptr)
            {
                sc_ret_errno = EBADF;
                break;
            }
            sc_ret(1) = entry.fd.ptr->flags;
            open_file_descriptor_delete_ref(&entry.fd);
            sc_ret_errno = 0;
            break;
        }
        case F_DUPFD_CLOEXEC:
        case F_DUPFD:
            ;
            int ret = vfs_dup(arg1);
            if (ret >= 0)
            {
                sc_ret_errno = 0;
                sc_ret(1) = ret;
                if (arg2 == F_DUPFD_CLOEXEC)
                    ofdv_setflags(&current_task->open_file_table, ret, FD_CLOEXEC);
            }
            else
                sc_ret_errno = -ret;
            break;
        default:
            LOG(WARNING, "Unknown fcntl request (%d, %d, %" PRIu64 ")", arg1, arg2, arg3);
            // task_send_signal(current_task, SIGILL);
            sc_ret_errno = ENOSYS;
        }
        break;
    sc_case(SYS_SIGRET, 0)
        SC_LOG("syscall SYS_SIGRET()");
        *return_address = sigret;
        sc_ret_errno = 0;
        break;
    sc_case(SYS_PSELECT, 6, int, fd_set*, fd_set*, fd_set*, const struct timespec*, const sigset_t*)
        SC_LOG("syscall SYS_PSELECT(%d, %p, %p, %p, %p, %p)", arg1, arg2, arg3, arg4, arg5, arg6);
        sc_validate_pointer(arg2);
        sc_validate_pointer(arg3);
        sc_validate_pointer(arg4);
        sc_validate_pointer(arg5);
        sc_validate_pointer(arg6);
        uint32_t sd_flags = lock_scheduler();
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
                current_task->timeout_deadline = rdtsc() + arg5->tv_nsec * tsc_cycles_per_second / 1000000000 + arg5->tv_sec * tsc_cycles_per_second;
                __move_running_task_to_thread_queue(&waiting_for_time_tasks, current_task);
                switch_task();
                unlock_scheduler(sd_flags);
                sd_flags = lock_scheduler();
            }
            else
                current_task->timeout_deadline = 0;
            if (current_task->timeout_deadline >= rdtsc())
                sc_ret_errno = should_restart_syscall() ? ERESTART : EINTR;
            current_task->sig_mask = saved_sigmask;
            unlock_scheduler(sd_flags);
            break;
        }

        bool shouldblock = !(arg5 && arg5->tv_nsec == 0 && arg5->tv_sec == 0);

        fd_set read_ret, write_ret;

        uint32_t ft_flags = acquire_spinlock_noint(&current_task->open_file_table.lock);
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
                    open_file_descriptor_t* entry = (size_t)i < current_task->open_file_table.size ? current_task->open_file_table.data[i].fd.ptr : NULL;
                    if (!entry)
                    {
                        FD_CLR(i, &read_ret);
                        continue;
                    }
                    if (__vfs_willblock(entry, POLLIN))
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
                    open_file_descriptor_t* entry = (size_t)i < current_task->open_file_table.size ? current_task->open_file_table.data[i].fd.ptr : NULL;
                    if (!entry)
                    {
                        FD_CLR(i, &write_ret);
                        continue;
                    }
                    if (__vfs_willblock(entry, POLLOUT))
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
                        open_file_descriptor_t* entry = (size_t)i < current_task->open_file_table.size ? current_task->open_file_table.data[i].fd.ptr : NULL;
                        if (!entry)
                            continue;
                        if (arg2 ? FD_ISSET(i, arg2) : false)
                            if (__vfs_willblock(entry, POLLIN))
                                goto pselect_monitor;
                        if (arg3 ? FD_ISSET(i, arg3) : false)
                            if (__vfs_willblock(entry, POLLOUT))
                                goto pselect_monitor;

                        continue;

                    pselect_monitor:
                        __task_monitor_entry(current_task, entry);
                    }
                }
                __task_start_polling(current_task, arg5 ? arg5->tv_nsec * tsc_cycles_per_second / 1000000000 + arg5->tv_sec * tsc_cycles_per_second : NO_TIMEOUT);
                switch_task(); // Postpone switch to after unlock
                release_spinlock_noint(&current_task->open_file_table.lock, ft_flags);
                unlock_scheduler(sd_flags);
                sd_flags = lock_scheduler();
                ft_flags = acquire_spinlock_noint(&current_task->open_file_table.lock);
                if (current_task->sig_pending_user_space)
                {
                    sc_ret_errno = should_restart_syscall() ? ERESTART : EINTR;
                    shouldblock = false;
                }
            }
        } while (shouldblock);
        release_spinlock_noint(&current_task->open_file_table.lock, ft_flags);
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
        unlock_scheduler(sd_flags);
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
        // TODO: Reimplement pipes
        sc_ret_errno = ENOSYS; // vfs_setup_pipe(arg1, arg2);
        break;

    sc_case(SYS_KILL, 2, int, int)
        SC_LOG("syscall SYS_KILL(%d, %d)", arg1, arg2);
        if (arg2 < 0 || arg2 >= NUM_SIGNALS)
        {
            sc_ret_errno = EINVAL;
            break;
        }
        uint32_t flags = lock_scheduler();
        if (arg1 > 0)
        {
            thread_t* task = __find_task_by_pid_anywhere(arg1);
            if (!task)
            {
                sc_ret_errno = ESRCH;
                unlock_scheduler(flags);
                break;
            }
            if (task->pid == 0)
            {
                sc_ret_errno = EPERM;
                unlock_scheduler(flags);
                break;
            }
            __task_send_signal(task, arg2);
        }
        else if (arg1 == 0 || arg1 < -1)
        {
            pid_t pgrp = arg1 ? -arg1 : current_task->pgid;
            __task_send_signal_to_pgrp(arg2, pgrp);
        }
        else    // arg1 == -1
        {
        // * then  sig is sent to every process for which the calling process has permission to send signals, except for process 1
            FATAL("Not implemented");
        }
        unlock_scheduler(flags);
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
        FATAL("Not implemented");
        // precise_time_t deadline = global_timer + *arg2 * PRECISE_NANOSECONDS + *arg1 * PRECISE_SECONDS;
        // current_task->timeout_deadline = deadline;
        // move_running_task_to_thread_queue(&waiting_for_time_tasks, current_task);
        // switch_task();
        // const precise_time_t timer = global_timer;
        // if (timer <= deadline)
        // {
        //     sc_ret_errno = EINTR;
        //     *arg1 = (deadline - timer) / PRECISE_SECONDS;
        //     *arg2 = ((deadline - timer) / PRECISE_NANOSECONDS) % 1000000000;
        //     break;
        // }
        // *arg1 = 0;
        // *arg2 = 0;
        // sc_ret_errno = 0;
        // break;

    sc_case(SYS_POLL, 3, struct pollfd*, nfds_t, int)
        SC_LOG("syscall SYS_POLL(%p, %lu, %d)", arg1, arg2, arg3);
        // TODO: Validate the full array AND THE KERNEL STACK SIZE
        sc_validate_pointer(arg1);
        const size_t array_bytes = sizeof(struct pollfd) * arg2;
        struct pollfd* ret = alloca(array_bytes);
        uint32_t sd_flags = lock_scheduler();
        uint32_t ft_flags = acquire_spinlock_noint(&current_task->open_file_table.lock);
        bool shouldblock = arg3 != 0;
        do
        {
            sc_ret_errno = 0;
            sc_ret(1) = 0;
            for (nfds_t i = 0; i < arg2; i++)
            {
                ret[i] = arg1[i];
                struct pollfd* fds = &ret[i];
                open_file_descriptor_t* entry = fds->fd >= 0 && (size_t)fds->fd < current_task->open_file_table.size ? current_task->open_file_table.data[fds->fd].fd.ptr : NULL;
                int set = 0;
                if (entry)
                {
                    fds->revents = 0;
                    if ((fds->events & POLLIN) || (fds->events & POLLRDNORM))
                    {
                        if (!__vfs_willblock(entry, POLLIN))
                            fds->revents |= (set = 1, shouldblock = false, POLLIN | __vfs_hup(entry));
                    }
                    if ((fds->events & POLLOUT) || (fds->events & POLLWRNORM))
                    {
                        if (!__vfs_willblock(entry, POLLOUT))
                            fds->revents |= (set = 1, shouldblock = false, POLLOUT | __vfs_hup(entry));
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
                    open_file_descriptor_t* entry = fds->fd >= 0 && (size_t)fds->fd < current_task->open_file_table.size ? current_task->open_file_table.data[fds->fd].fd.ptr : NULL;
                    if (!entry)
                        continue;
                    if ((fds->events & POLLIN) || (fds->events & POLLRDNORM))
                        if (__vfs_willblock(entry, POLLIN))
                            goto poll_monitor;
                    if ((fds->events & POLLOUT) || (fds->events & POLLWRNORM))
                        if (__vfs_willblock(entry, POLLOUT))
                            goto poll_monitor;

                    continue;

                poll_monitor:
                    // SC_LOG("Starting poll on fd %d", fds->fd);
                    __task_monitor_entry(current_task, entry);
                }
                __task_start_polling(current_task, PRECISE_MILLISECONDS * arg3);
                switch_task();
                release_spinlock_noint(&current_task->open_file_table.lock, ft_flags);
                unlock_scheduler(sd_flags);
                sd_flags = lock_scheduler();
                ft_flags = acquire_spinlock_noint(&current_task->open_file_table.lock);
                if (current_task->sig_pending_user_space)
                {
                    sc_ret_errno = should_restart_syscall() ? ERESTART : EINTR;
                    shouldblock = false;
                }
            }
        } while (shouldblock);
        release_spinlock_noint(&current_task->open_file_table.lock, ft_flags);
        unlock_scheduler(sd_flags);
        if (sc_ret_errno != ERESTART)
            memcpy(arg1, ret, array_bytes);
        break;
    sc_case(SYS_GETAFFINITY, 3, pid_t, size_t, cpu_set_t*)
        SC_LOG("syscall SYS_GETAFFINITY(%d, %zu, %p)", arg1, arg2, arg3);
        sc_validate_pointer(arg3);
        if (arg2 < sizeof(cpu_set_t))
        {
            sc_ret_errno = EINVAL;
            break;
        }
        uint32_t flags = lock_scheduler();
        thread_t* task = arg1 == 0 ? current_task : __find_task_by_pid_anywhere(arg1);
        if (!task)
        {
            unlock_scheduler(flags);
            sc_ret_errno = ESRCH;
            break;
        }
        unlock_scheduler(flags);
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

    sc_case(SYS_SETSID, 0)
        SC_LOG("syscall SYS_SETSID()");
        uint32_t flags = lock_scheduler();
        thread_queue_t* tq = hashmap_get_item(pgid_to_tq_hashmap, current_task->pid);
        if (tq && *tq)
        {
            unlock_scheduler(flags);
            sc_ret_errno = EPERM;
            break;
        }
        current_task->sid = current_task->pid;
        __task_set_pgid(current_task, current_task->pid);
        sc_ret(1) = current_task->sid;
        unlock_scheduler(flags);
        sc_ret_errno = 0;
        break;

    sc_case(SYS_LOG, 1, const char*)
        sc_validate_pointer(arg1);
    #ifdef PRINT_MLIBC_LOGS
        puts(arg1);
    #endif
        LOG(DEBUG, "%s", arg1);
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
        LOG(WARNING, "syscall %" PRIu64 " not implemented", syscall_num);
        // task_send_signal(current_task, SIGILL);
        sc_ret_errno = ENOSYS;
    }
    }

    ASSERT(get_rflags() & (1 << 9));

    if (sc_ret_errno != 0 && !sc_no_errno)
        SC_LOG("errno: %" PRId64 ": %s", sc_ret_errno, strerror(sc_ret_errno));

    if (syscall_num != SYS_SIGRET)
        task_handle_signal_to_userspace(registers);

    task_exit_critical_section(current_task);
    return registers->rsp;
}
