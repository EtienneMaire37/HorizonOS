#include "idle.h"
#include "vas.h"
#include "../fpu/fpu.h"
#include "../util/math.h"
#include "../cpu/memory.h"
#include "../paging/paging.h"
#include <fcntl.h>
#include "../vga/textio.h"
#include <string.h>

mutex_t file_table_lock = MUTEX_INIT;
uint8_t global_cpu_ticks = 0;

thread_t* running_tasks;
uint16_t task_count = 0;

uint64_t multitasking_counter = TASK_SWITCH_DELAY;

thread_t* current_task = NULL;
bool multitasking_enabled = false;
volatile bool first_task_switch = true;

pid_t task_reading_stdin = -1;
utf32_buffer_t keyboard_input_buffer;

int task_lock_depth = 0;

const uint64_t task_rsp_offset = offsetof(thread_t, rsp);
const uint64_t task_cr3_offset = offsetof(thread_t, cr3);

void task_init_file_table(thread_t* task)
{
    for (int i = 0; i < OPEN_MAX; i++)
        task->file_table[i] = invalid_fd;
    task->file_table[0] = 0;    // * STDIN_FILENO
    task->file_table[1] = 1;    // * STDOUT_FILENO
    task->file_table[2] = 2;    // * STDERR_FILENO
}

thread_t* task_create_empty()
{
    thread_t* task = (thread_t*)malloc(sizeof(thread_t));
    if (!task) return NULL;

    memset(task->name, 0, THREAD_NAME_MAX);

    task->file_table_mutex = MUTEX_INIT;

    task->pid = task_generate_pid();
    task->ppid = -1;
    task->pgid = task->pid;

    task->system_task = true;

    task->fpu_state = fpu_state_create();

    task->is_dead = false;
    task->forked_pid = 0;    // is_being_forked = false

    task->wait_pid = -1;

    task->to_reap = false;
    task->doing_io = false;

    task->cwd = vfs_root;

    task_init_file_table(task);

    return task;
}

void task_destroy(thread_t* task)
{
    LOG(TRACE, "Destroying task \"%s\" (pid = %d, ring = %u)", task->name, task->pid, task->ring);
    task_free_vas((physical_address_t)task->cr3);
    for (int i = 0; i < OPEN_MAX; i++)
    {
        if (task->file_table[i] != invalid_fd)
            vfs_remove_global_file(task->file_table[i]);
    }
    fpu_state_destroy(&task->fpu_state);
    free(task->sig_act_array);
}

void task_setup_stack(thread_t* task, uint64_t entry_point, uint16_t code_seg, uint16_t data_seg)
{
    task_stack_push(task, data_seg);
    task_stack_push(task, task->rsp + 8);

    task_stack_push(task, 0x200);  // get_rflags()
    task_stack_push(task, code_seg);
    task_stack_push(task, entry_point);

    task_stack_push(task, (uint64_t)iretq_instruction);

    task_stack_push(task, (uint64_t)unlock_task_queue);
    task_stack_push(task, (uint64_t)cleanup_tasks);
    task_stack_push(task, (uint64_t)end_context_switch);

    task_stack_push(task, 0);           // rax
    task_stack_push(task, 0);           // rbx
    task_stack_push(task, code_seg);    // rdx
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

void multitasking_init()
{
    task_count = 0;
    current_task = NULL;

    utf32_buffer_init(&keyboard_input_buffer);
    task_reading_stdin = -1;

    vfs_init_file_table();

    multitasking_add_idle_task("idle");
}

void multitasking_start()
{
    fflush(stdout);
    multitasking_enabled = true;
    current_task = idle_task;

    idle_main();
}

void multitasking_add_idle_task(char* name)
{
    if (task_count != 0)
    {
        LOG(CRITICAL, "The kernel task must be the first one");
        abort();
    }

    thread_t* task = task_create_empty();
    task_set_name(task, name);
    task->cr3 = get_cr3();

    multitasking_add_task(task);
}

void multitasking_add_task(thread_t* task)
{
    lock_task_queue();
    
    if (!running_tasks)
    {
        task->prev = task->next = task;
        running_tasks = task;
    }
    else
    {
        task->prev = running_tasks->prev;
        task->next = running_tasks;
        running_tasks->prev->next = task;
        running_tasks->prev = task;
    }

    task_count++;

    unlock_task_queue();
}

void task_stack_push(thread_t* task, uint64_t value)
{
    task->rsp -= 8;

    // * not needed
    // if (!is_address_canonical(task->rsp))
    //     LOG(ERROR, "rsp: %#" PRIx64 " is not canonical!!", task->rsp);

    task_write_at_address_8b(task, (physical_address_t)task->rsp, value);
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
        LOG(CRITICAL, "No tasks!");
        abort();
    }

    // ! Should never log anything here

    lock_task_queue();

    bool was_first_task_switch = first_task_switch;
    first_task_switch = false;

    thread_t* next_task = find_next_task();
    if (current_task->pid != next_task->pid)
        full_context_switch(next_task);

    cleanup_tasks();

    unlock_task_queue();
}

thread_t* find_task_by_pid(pid_t pid)
{
    if (pid < 0) return NULL;
    thread_t* cur = running_tasks;
    do
    {
        if (cur->pid == pid)
            return cur;
        cur = cur->next;
    } while (cur != running_tasks);
    return NULL;
}

void task_kill(thread_t* task)
{
    if (!task) return;
    if (task_count == 1)
    {
        LOG(CRITICAL, "Zero tasks remaining");
        abort();
    }

    task->prev->next = task->next;
    task->next->prev = task->prev;

    task_destroy(task);
    free(task);
}

void task_copy_file_table(thread_t* from, thread_t* to, bool cloexec)
{
    acquire_mutex(&file_table_lock);
    acquire_mutex(&from->file_table_mutex);
    acquire_mutex(&to->file_table_mutex);
    for (int i = 3; i < OPEN_MAX; i++)
    {
        if (from->file_table[i] == invalid_fd || (cloexec && (file_table[from->file_table[i]].flags & O_CLOEXEC)))
            to->file_table[i] = invalid_fd;
        else
        {
            to->file_table[i] = from->file_table[i];
            if (to->file_table[i] != invalid_fd)
                file_table[to->file_table[i]].used++;
        }
    }
    release_mutex(&to->file_table_mutex);
    release_mutex(&from->file_table_mutex);
    release_mutex(&file_table_lock);
}

void copy_task(thread_t* task)
{
    if (!task)
        return;

    thread_t* new_task = (thread_t*)malloc(sizeof(thread_t));
    if (!new_task) return;

    *new_task = *task;

    memcpy(new_task->name, task->name, THREAD_NAME_MAX);
    new_task->ring = task->ring;
    new_task->pid = task->forked_pid;
    new_task->system_task = task->system_task;

    new_task->cr3 = task_create_empty_vas(new_task->ring == 0 ? PG_SUPERVISOR : PG_USER);
    new_task->rsp = task->rsp;

    new_task->fpu_state = fpu_state_create_copy(task->fpu_state);

    task_copy_file_table(task, new_task, false);

    new_task->forked_pid = 0;
    new_task->is_dead = false;

    new_task->ppid = task->pid;
    new_task->wait_pid = -1;

    task_vas_copy((uint64_t*)(task->cr3 + PHYS_MAP_BASE), (uint64_t*)(new_task->cr3 + PHYS_MAP_BASE), 0, TASK_STACK_TOP_ADDRESS >> 12);

    multitasking_add_task(new_task);
}

void cleanup_tasks()
{
    if (current_task != idle_task) return;

    lock_task_queue();

    thread_t* cur_task = running_tasks;
    do
    {
        if (cur_task != current_task && cur_task->forked_pid)
        {
            copy_task(cur_task);
            cur_task->forked_pid = 0;
        }
        cur_task = cur_task->next;
    }
    while (cur_task != running_tasks);

    do
    {
        if (!(cur_task->is_dead && task_can_be_killed(cur_task))) goto continue1;
        thread_t* parent = find_task_by_pid(cur_task->ppid);
        if (parent)
        {
            if (parent->wait_pid != -1)
            {
                if (parent->wait_pid == 0 || absint(parent->wait_pid) == cur_task->pid)
                {
                    cur_task->to_reap = true;
                    parent->wait_pid = -1;
                    parent->wstatus = cur_task->return_value;
                }
            }
        }
        else
            cur_task->to_reap = true;
    continue1:
        cur_task = cur_task->next;
    }
    while (cur_task != running_tasks);

    do
    {
        if (cur_task != current_task && cur_task->to_reap)
        {
            thread_t* task_to_kill = cur_task;
            cur_task = cur_task->next;
            task_kill(task_to_kill);
        }
        else
            cur_task = cur_task->next;
    }
    while (cur_task != running_tasks);

    unlock_task_queue();
}

void tasks_log()
{
    LOG(DEBUG, "%u tasks: (Total CPU usage: %u.%u)", task_count, (1000 - idle_task->stored_cpu_ticks) / 10,  (1000 - idle_task->stored_cpu_ticks) % 10);
    lock_task_queue();
    thread_t* task = running_tasks;
    do
    {
        LOG(DEBUG, "%s── task \"%s\" [pid %d, ppid %d, pgid %d] | CPU Usage: %u.%u%s%s%s%s%s", task->next == running_tasks ? "└" : "├",
            task->name, task->pid, task->ppid, task->pgid, task->stored_cpu_ticks / 10, task->stored_cpu_ticks % 10,
            task_is_blocked(task) ? " (blocked)" : "", 
            task->pgid == tty_foreground_pgrp ? " (foreground)" : " (background)",
            task->doing_io ? " (waiting for io)" : "",
            task->is_dead ? " (dead)" : "",
            task->to_reap ? " (waiting to be reaped)" : "");

        task = task->next;
    }
    while (task != running_tasks);
    unlock_task_queue();
}

pid_t task_generate_pid()
{
    static pid_t current = 0;
    return current++;
}

void kill_current_task(int ret)
{
    lock_task_queue();
    current_task->return_value = ret;
    current_task->is_dead = true;
    unlock_task_queue();
    switch_task();
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

void task_queue_signal(thread_t* task, int sig)
{
    if (sig >= 32)
    {
        LOG(ERROR, "RT signals are not implemented yet!!");
        while (true);
    }

    task_set_pending_signal(task, sig);
}