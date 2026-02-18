#include "vas.h"
#include "../fpu/fpu.h"
#include "../util/math.h"
#include "../cpu/memory.h"
#include "../paging/paging.h"
#include <fcntl.h>
#include "../vga/textio.h"
#include <string.h>
#include "queue.h"
#include "multitasking.h"
#include "hashmap.h"

const uint64_t task_rsp_offset = offsetof(thread_t, rsp);
const uint64_t task_cr3_offset = offsetof(thread_t, cr3);

void task_init_file_table(thread_t* task)
{
    for (int i = 0; i < OPEN_MAX; i++)
        task->file_table[i].index = invalid_fd;
    task->file_table[0] = (file_table_index_t){.index = 0, .flags = 0};    // * STDIN_FILENO
    task->file_table[1] = (file_table_index_t){.index = 1, .flags = 0};    // * STDOUT_FILENO
    task->file_table[2] = (file_table_index_t){.index = 2, .flags = 0};    // * STDERR_FILENO
}

thread_t* task_create_empty()
{
    thread_t* task = (thread_t*)malloc(sizeof(thread_t));
    if (!task) return NULL;

    memset(task, 0, sizeof(task));

    task->file_table_mutex = MUTEX_INIT;

    task->pid = task_generate_pid();
    task->ppid = -1;
    task->pgid = -1;
    task_set_pgid(task, task->pid);

    task->ruid = task->euid = task->suid = 0;
    task->rgid = task->egid = task->sgid = 0;

    task->system_task = true;

    task->fpu_state = fpu_state_create();

    task->forked_pid = 0;    // is_being_forked = false

    task->wait_pid = -1;

    task->cwd = vfs_root;

    task->queue = NULL;

    task_init_file_table(task);

    hashmap_set_item(pid_to_task_hashmap, task->pid, task);

    return task;
}

void task_set_pgid(thread_t* task, pid_t pgid)
{
    lock_scheduler();
    if (task->pgid != -1)
        tq_hashmap_remove(pgid_to_tq_hashmap, task->pgid, task);
    task->pgid = pgid;
    tq_hashmap_push_back(pgid_to_tq_hashmap, task->pgid, task);
    unlock_scheduler();
}

void task_destroy(thread_t* task)
{
    lock_scheduler();
    LOG(TRACE, "Destroying task %p: \"%s\" (pid = %d, ring = %u) (%d tasks left)", 
        task, task->name, task->pid, task->ring, task_count - 1);
    
    hashmap_remove_item(pid_to_task_hashmap, task->pid);
    tq_hashmap_remove(pgid_to_tq_hashmap, task->pgid, task);

    thread_t* parent = find_task_by_pid_anywhere(task->ppid);
    if (parent)
        tq_hashmap_remove(pid_to_children_tq_hashmap, parent->pid, task);

    for (int i = 0; i < OPEN_MAX; i++)
    {
    	if (task->file_table[i].index != invalid_fd)
            vfs_remove_global_file(task->file_table[i].index);
    }
    fpu_state_destroy(&task->fpu_state);
    task_free_vas((physical_address_t)task->cr3);
	LOG(TRACE, "Done.");
    unlock_scheduler();
}

void task_setup_stack(thread_t* task, uint64_t entry_point, uint16_t code_seg, uint16_t data_seg)
{
    task_stack_push(task, data_seg);
    task_stack_push(task, task->rsp + 8);

    task_stack_push(task, 0x200);  // get_rflags()
    task_stack_push(task, code_seg);
    task_stack_push(task, entry_point);

    task_stack_push(task, (uint64_t)iretq_instruction);

    task_stack_push(task, (uint64_t)unlock_scheduler);
    task_stack_push(task, (uint64_t)cleanup_tasks);
    task_stack_push(task, (uint64_t)end_context_switch);

    task_stack_push(task, 0);           // rax
    task_stack_push(task, 0);           // rbx
    task_stack_push(task, 0);           // rcx
    task_stack_push(task, code_seg);    // rdx
    task_stack_push(task, 0);           // r8
    task_stack_push(task, 0);           // r9
    task_stack_push(task, 0);           // r10
    task_stack_push(task, 0);           // r11
    task_stack_push(task, 0);           // r12
    task_stack_push(task, 0);           // r13
    task_stack_push(task, 0);           // r14
    task_stack_push(task, 0);           // r15
    task_stack_push(task, 0);           // rbp
}

void task_set_name(thread_t* task, const char* name)
{
    int name_bytes = minint(strlen(name), THREAD_NAME_MAX - 1);
    memcpy(task->name, name, name_bytes);
    task->name[name_bytes] = 0;
}

void task_stack_push(thread_t* task, uint64_t value)
{
    task->rsp -= 8;

    // * not needed
    // if (!is_address_canonical(task->rsp))
    //     LOG(ERROR, "rsp: %#" PRIx64 " is not canonical!!", task->rsp);

    task_write_at_address_8b(task, (physical_address_t)task->rsp, value);
}

void task_stack_push_auxv(thread_t* task, Elf64_auxv_t val)
{
    task_stack_push(task, val.a_un.a_val);
    task_stack_push(task, val.a_type);
}

void task_stack_push_data(thread_t* task, void* data, size_t bytes)
{
    task->rsp -= bytes;

    // * Also align!
    task->rsp = task->rsp & ~7ULL;

    for (size_t i = 0; i < bytes; i++)
        task_write_at_address_1b(task, (physical_address_t)task->rsp + i, ((uint8_t*)data)[i]);
}

void task_stack_push_string(thread_t* task, const char* str)
{
    const int bytes = strlen(str) + 1;
    task_stack_push_data(task, (void*)str, bytes);
}

void task_write_at_address_1b(thread_t* task, uint64_t address, uint8_t value)
{
    if (task->cr3 == physical_null)
    {
        LOG(WARNING, "Kernel tried to write into a null vas");
        return;
    }
    
    uint8_t* ptr = (uint8_t*)virtual_to_physical((uint64_t*)(task->cr3 + PHYS_MAP_BASE), address);
    
    *ptr = value;
}

void task_write_at_aligned_address_8b(thread_t* task, uint64_t address, uint64_t value)
{
    if (task->cr3 == physical_null)
    {
        LOG(WARNING, "task_write_at_aligned_address_8b: Kernel tried to write into a null vas");
        return;
    }
    if (address & 7)    // ! Not aligned
    {
        LOG(CRITICAL, "task_write_at_aligned_address_8b: Address %#.16" PRIx64 " not aligned", address);
        abort();
    }

    uint64_t* ptr = (uint64_t*)virtual_to_physical((uint64_t*)(task->cr3 + PHYS_MAP_BASE), address);
    
    *ptr = value;
}

void task_write_at_address_8b(thread_t* task, uint64_t address, uint64_t value)
{
    if (task->cr3 == physical_null)
    {
        LOG(WARNING, "Kernel tried to write into a null vas");
        return;
    }

    task_write_at_address_1b(task, address + 0, (value >> 0)  & 0xff);
    task_write_at_address_1b(task, address + 1, (value >> 8)  & 0xff);
    task_write_at_address_1b(task, address + 2, (value >> 16) & 0xff);
    task_write_at_address_1b(task, address + 3, (value >> 24) & 0xff);

    task_write_at_address_1b(task, address + 4, (value >> 32) & 0xff);
    task_write_at_address_1b(task, address + 5, (value >> 40) & 0xff);
    task_write_at_address_1b(task, address + 6, (value >> 48) & 0xff);
    task_write_at_address_1b(task, address + 7, (value >> 56) & 0xff);
}

uint8_t task_read_at_address_1b(thread_t* task, uint64_t address)
{
    if (task->cr3 == physical_null)
    {
        LOG(WARNING, "Kernel tried to write into a null vas");
        return 0;
    }
    
    uint8_t* ptr = (uint8_t*)virtual_to_physical((uint64_t*)(task->cr3 + PHYS_MAP_BASE), address);
    
    return *ptr;
}

uint64_t task_read_at_aligned_address_8b(thread_t* task, uint64_t address)
{
    if (task->cr3 == physical_null)
    {
        LOG(WARNING, "task_write_at_aligned_address_8b: Kernel tried to write into a null vas");
        return 0;
    }
    if (address & 7)    // ! Not aligned
    {
        LOG(CRITICAL, "task_write_at_aligned_address_8b: Address %#.16" PRIx64 " not aligned", address);
        abort();
    }

    uint64_t* ptr = (uint64_t*)virtual_to_physical((uint64_t*)(task->cr3 + PHYS_MAP_BASE), address);
    
    return *ptr;
}

uint64_t task_read_at_address_8b(thread_t* task, uint64_t address)
{
    if (task->cr3 == physical_null)
    {
        LOG(WARNING, "Kernel tried to write into a null vas");
        return 0;
    }

    return (((uint64_t)task_read_at_address_1b(task, address + 0) <<  0) |
            ((uint64_t)task_read_at_address_1b(task, address + 1) <<  8) |
            ((uint64_t)task_read_at_address_1b(task, address + 2) << 16) |
            ((uint64_t)task_read_at_address_1b(task, address + 3) << 24) |
            ((uint64_t)task_read_at_address_1b(task, address + 4) << 32) |
            ((uint64_t)task_read_at_address_1b(task, address + 5) << 40) |
            ((uint64_t)task_read_at_address_1b(task, address + 6) << 48) |
            ((uint64_t)task_read_at_address_1b(task, address + 7) << 56));
}

void switch_task()
{
    if (task_count == 0)
    {
        LOG(DEBUG, "No processes left");
        abort();
    }

    if (task_lock_depth > 0)
    {
        queued_ts = true;
        return;
    }

    // ! Should never log anything here

    lock_scheduler();

    thread_t* next_task = find_next_task();
    if (current_task->pid != next_task->pid)
        full_context_switch(next_task);

    cleanup_tasks();

    unlock_scheduler();
}

thread_t* find_running_task_by_pid(pid_t pid)
{
    if (pid < 0) return NULL;
    thread_t* cur_running = running_tasks;
    do
    {
        if (cur_running->pid == pid)
            return cur_running;
        cur_running = cur_running->next;
    } while (cur_running != running_tasks);
    return NULL;
}

thread_t* find_task_by_pid_in_queue(thread_queue_t* queue, pid_t pid)
{
    if (pid < 0) return NULL;
    lock_scheduler();
    if (!queue) return (unlock_scheduler(), NULL);
    if (!*queue) return (unlock_scheduler(), NULL);
    thread_queue_item_t* it = *queue;
    do
    {
        thread_t* thread = (thread_t*)it->data;
        if (thread->pid == pid)
        {
            unlock_scheduler();
            return thread;
        }
        it = it->next;
    } while (it != *queue);
    unlock_scheduler();
    return NULL;
}

thread_t* find_task_by_pid_anywhere(pid_t pid)
{
    return hashmap_get_item(pid_to_task_hashmap, pid);
}

void task_copy_file_table(thread_t* from, thread_t* to, bool cloexec)
{
	lock_scheduler();
    acquire_mutex(&file_table_lock);
    acquire_mutex(&from->file_table_mutex);
    acquire_mutex(&to->file_table_mutex);
    for (int i = 3; i < OPEN_MAX; i++)
    {
        if (from->file_table[i].index == invalid_fd || (cloexec && (from->file_table[i].flags & FD_CLOEXEC)))
            to->file_table[i].index = invalid_fd;
        else
        {
            to->file_table[i] = from->file_table[i];
            if (to->file_table[i].index != invalid_fd)
                file_table[to->file_table[i].index].used++;
        }
    }
    release_mutex(&to->file_table_mutex);
    release_mutex(&from->file_table_mutex);
    release_mutex(&file_table_lock);
    unlock_scheduler();
}

void duplicate_task(thread_t* task)
{
    if (!task)
        return;

    lock_scheduler();

    thread_t* new_task = (thread_t*)malloc(sizeof(thread_t));
    if (!new_task) 
    {
        unlock_scheduler();
        return;
    }

    *new_task = *task;

    new_task->pid = task->forked_pid;
    new_task->forked_pid = 0;
    new_task->system_task = task->system_task;

    new_task->cr3 = task_create_empty_vas(new_task->ring == 0 ? PG_SUPERVISOR : PG_USER);
    new_task->rsp = task->rsp;

    new_task->fpu_state = fpu_state_create_copy(task->fpu_state);

    task_copy_file_table(task, new_task, false);

    new_task->ppid = task->pid;
    new_task->wait_pid = -1;

    task_vas_copy((uint64_t*)(task->cr3 + PHYS_MAP_BASE), (uint64_t*)(new_task->cr3 + PHYS_MAP_BASE), 0, TASK_STACK_TOP_ADDRESS >> 12);

    new_task->pgid = -1;
    task_set_pgid(new_task, task->pgid);
    hashmap_set_item(pid_to_task_hashmap, new_task->pid, new_task);

    tq_hashmap_push_back(pid_to_children_tq_hashmap, task->pid, new_task);

    // LOG(DEBUG, "Pid to children tq hashmap after fork:");
    // tq_hashmap_log(pid_to_children_tq_hashmap);

    multitasking_add_task(new_task);
    task_count++;

    unlock_scheduler();
}

void cleanup_tasks()
{
    lock_scheduler();

    if (forked_tasks)
    {
        thread_queue_item_t* cur_forked_task = forked_tasks;
        do
        {
            thread_t* task_to_fork = cur_forked_task->data;
            cur_forked_task = cur_forked_task->next;
            if (task_to_fork != current_task)
            {
                move_task_to_running_queue(&forked_tasks, cur_forked_task);
                duplicate_task(task_to_fork);
            }
        }
        while (forked_tasks && cur_forked_task != forked_tasks);
    }

    if (reapable_tasks)
    {
        thread_queue_item_t* cur_reapable_task = reapable_tasks;
        do
        {
            thread_t* task_to_kill = cur_reapable_task->data;
            if (cur_reapable_task->data != current_task)
            {
                thread_queue_item_t* removed_item = cur_reapable_task;
                cur_reapable_task = cur_reapable_task->next;
                task_destroy(task_to_kill);
                free(task_to_kill);
                task_count--;
                thread_queue_remove(&reapable_tasks, removed_item);
            }
            else
                cur_reapable_task = cur_reapable_task->next;
        }
        while (reapable_tasks != NULL && cur_reapable_task->prev != cur_reapable_task);
    }

    unlock_scheduler();
}

void waitpid_check_dead()
{
    lock_scheduler();
    thread_queue_item_t* cur_dead_task = dead_tasks;
    if (waitpid_tasks && cur_dead_task)
    {
        do
        {
            thread_t* thread = (thread_t*)cur_dead_task->data;
            thread_t* parent = find_task_by_pid_in_queue(&waitpid_tasks, thread->ppid);
            pid_t pid = parent->wait_pid;
            if (parent)
            {
                if (pid > 0)
                {
                    if (thread->pid == pid)
                    {
                        goto found_task;
                    }
                }
                else if (pid == 0)
                {
                    if (thread->pgid == parent->pgid_on_waitpid)
                    {
                        goto found_task;
                    }
                }
                else if (pid == -1)
                {
                    goto found_task;
                }
                else // * pid < -1
                {
                    if (thread->pgid == -pid)
                    {
                        goto found_task;
                    }
                }

            found_task:
                move_task_from_to_thread_queue(&dead_tasks, &reapable_tasks, cur_dead_task);
                
                parent->wstatus = thread->return_value;
                parent->waitpid_ret = thread->pid;
                move_task_to_running_queue(&waitpid_tasks, ll_find_item_by_data(&waitpid_tasks, parent));
            }
            else
            {
                if (!find_running_task_by_pid(thread->ppid))
                    move_task_from_to_thread_queue(&dead_tasks, &reapable_tasks, cur_dead_task);
            }
            cur_dead_task = cur_dead_task->next;
        }
        while (dead_tasks != TQ_INIT && cur_dead_task != dead_tasks);
    }
    unlock_scheduler();
}

void tasks_log()
{
    LOG(DEBUG, "%u tasks: (Total CPU usage: %u.%u)", task_count, (1000 - idle_task->stored_cpu_ticks) / 10,  (1000 - idle_task->stored_cpu_ticks) % 10);
    lock_scheduler();
    thread_t* task = running_tasks;
    do
    {
        LOG(DEBUG, "%s── task \"%s\" [pid %d, ppid %d, pgid %d] | CPU Usage: %u.%u%s", task->next == running_tasks ? "└" : "├",
            task->name, task->pid, task->ppid, task->pgid, task->stored_cpu_ticks / 10, task->stored_cpu_ticks % 10,
            task->pgid == tty_foreground_pgrp ? " (foreground)" : " (background)");

        task = task->next;
    }
    while (task != running_tasks);
    LOG(DEBUG, "pid to thread hashmap:");
    hashmap_log(pid_to_task_hashmap);
    LOG(DEBUG, "pgid to thread queue hashmap:");
    tq_hashmap_log(pgid_to_tq_hashmap);
    LOG(DEBUG, "pid to children hashmap:");
    tq_hashmap_log(pid_to_children_tq_hashmap);
    unlock_scheduler();
}

pid_t task_generate_pid()
{
    static pid_t current = 0;
    return current++;
}

void kill_task(thread_t* task, int ret)
{
    lock_scheduler();
    LOG(TRACE, "kill_task(%p: {.name = \"%s\", .pid = %d, .ppid = %d, .pgid = %d}, %d)", 
        task, task->name, task->pid, task->ppid, task->pgid, ret);
    task->return_value = ret;

    thread_t* global_parent = find_task_by_pid_anywhere(task->ppid);

    if (!global_parent || (global_parent->sig_act_array[SIGCHLD].sa_flags & SA_NOCLDWAIT) || global_parent->queue == &reapable_tasks || global_parent->queue == &dead_tasks)
//  * If the SA_NOCLDWAIT flag is set when establishing a handler for SIGCHLD, POSIX.1 leaves it unspecified whether a SIGCHLD  signal  is
//  *         generated  when a child process terminates.  On Linux, a SIGCHLD signal is generated in this case; on some other implementations, it
//  *         is not.
        move_task_to_queue(&reapable_tasks, task);
    else
    {
        move_task_to_queue(&dead_tasks, task);
        
        thread_t* parent = find_task_by_pid_in_queue(&waitpid_tasks, task->ppid);
        if (parent)
        {
            parent->waitpid_ret = task->pid;
            parent->wstatus = ret;
            move_task_to_running_queue(&waitpid_tasks, ll_find_item_by_data(&waitpid_tasks, parent));
            move_task_from_to_thread_queue(&dead_tasks, &reapable_tasks, ll_find_item_by_data(&dead_tasks, task));
        }
    }

    thread_queue_t* children = hashmap_get_item(pid_to_children_tq_hashmap, task->pid);
    ll_destroy(children);

    if (global_parent)
        tq_hashmap_remove(pid_to_children_tq_hashmap, global_parent->pid, task);

    if (global_parent)
        task_send_signal(global_parent, SIGCHLD);

    if (task == current_task)
        switch_task();

    unlock_scheduler();
}

void task_mask_signal(thread_t* task, int sig)
{
    int idx = sig / sizeof(unsigned long);
    task->sig_mask.__sig[idx] |= (1ULL << (sig - idx * sizeof(unsigned long)));
}

void task_set_pending_signal(thread_t* task, int sig)
{
    int idx = sig / sizeof(unsigned long);
    task->sig_pending.__sig[idx] |= (1ULL << (sig - idx * sizeof(unsigned long)));
}