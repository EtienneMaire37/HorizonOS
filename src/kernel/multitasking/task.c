#include "idle.h"
#include "vas.h"
#include "../fpu/fpu.h"
#include "../../libc/src/math_utils.h"
#include "../cpu/memory.h"
#include "../paging/paging.h"
#include "../../libc/include/fcntl.h"
#include "../vga/textio.h"

mutex_t file_table_lock = MUTEX_INIT;
uint8_t global_cpu_ticks = 0;

thread_t tasks[MAX_TASKS];    // TODO : Implement a dynamic array
uint16_t task_count = 0;

uint64_t multitasking_counter = TASK_SWITCH_DELAY;

uint16_t current_task_index = 0;
bool multitasking_enabled = false;
volatile bool first_task_switch = true;

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

thread_t task_create_empty()
{
    thread_t task;

    memset(task.name, 0, THREAD_NAME_MAX);

    utf32_buffer_init(&task.input_buffer);
    task.reading_stdin = false;

    task.cr3 = physical_null;
    task.ring = 0;
    task.fs_base = 0;

    task.rsp = 0;

    task.pid = task_generate_pid();
    task.ppid = -1;
    task.pgid = task.pid;

    task.system_task = true;

    task.fpu_state = fpu_state_create();

    task.current_cpu_ticks = 0;
    task.stored_cpu_ticks = 0;

    task.is_dead = false;
    task.forked_pid = 0;    // is_being_forked = false

    task.wait_pid = -1;

    task.to_reap = false;
    task.doing_io = false;

    task.cwd = vfs_root;

    task_init_file_table(&task);

    return task;
}

void task_destroy(thread_t* task)
{
    LOG(TRACE, "Destroying task \"%s\" (pid = %d, ring = %u)", task->name, task->pid, task->ring);
    LOG(TRACE, "Freeing VAS...");
    task_free_vas((physical_address_t)task->cr3);
    LOG(TRACE, "Destroying the keyboard buffer...");
    utf32_buffer_destroy(&task->input_buffer);
    LOG(TRACE, "Freeing tnodes...");
    for (int i = 0; i < OPEN_MAX; i++)
    {
        if (task->file_table[i] != invalid_fd)
            vfs_remove_global_file(task->file_table[i]);
    }
    LOG(TRACE, "Destroying FPU state...");
    fpu_state_destroy(&task->fpu_state);
    LOG(TRACE, "Done.");
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
    current_task_index = 0;

    vfs_init_file_table();

    multitasking_add_idle_task("idle");
}

void multitasking_start()
{
    fflush(stdout);
    multitasking_enabled = true;
    current_task_index = 0;

    idle_main();
}

void multitasking_add_idle_task(char* name)
{
    if (task_count >= MAX_TASKS)
    {
        LOG(CRITICAL, "Too many tasks");
        abort();
    }

    if (task_count != 0)
    {
        LOG(CRITICAL, "The kernel task must be the first one");
        abort();
    }

    thread_t task = task_create_empty();
    task_set_name(&task, name);
    task.cr3 = get_cr3();

    tasks[task_count++] = task;
}

void task_stack_push(thread_t* task, uint64_t value)
{
    task->rsp -= 8;

    // * not needed
    // if (!is_address_canonical(task->rsp))
    //     LOG(ERROR, "rsp: %#llx is not canonical!!", task->rsp);

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
        LOG(CRITICAL, "task_write_at_aligned_address_8b: Address %#.16llx not aligned", address);
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
        LOG(CRITICAL, "task_write_at_aligned_address_8b: Address %#.16llx not aligned", address);
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

    uint16_t next_task_index = find_next_task_index();
    if (__CURRENT_TASK.pid != tasks[next_task_index].pid)
        full_context_switch(next_task_index);

    cleanup_tasks();

    unlock_task_queue();
}

thread_t* find_task_by_pid(pid_t pid)
{
    if (pid < 0) return NULL;
    for (uint16_t i = 0; i < task_count; i++)
        if (tasks[i].pid == pid)
            return &tasks[i];
    return NULL;
}

void task_kill(uint16_t index)
{
    if (index == current_task_index)
    {
        LOG(CRITICAL, "Kernel tried killing current task");
        abort();
    }
    if (index >= task_count || index == 0)
    {
        LOG(CRITICAL, "Invalid task index %u", index);
        abort();
    }
    if (task_count == 1)
    {
        LOG(CRITICAL, "Zero tasks remaining");
        abort();
    }

    task_destroy(&tasks[index]);

    for (uint16_t i = index; i < task_count - 1; i++)
        tasks[i] = tasks[i + 1];

    if (current_task_index > index)
        current_task_index--;
    task_count--;
}

void task_copy_file_table(uint16_t from, uint16_t to, bool cloexec)
{
    acquire_mutex(&file_table_lock);
    for (int i = 3; i < OPEN_MAX; i++)
    {
        if (tasks[from].file_table[i] == invalid_fd || (cloexec && (file_table[tasks[from].file_table[i]].flags & O_CLOEXEC)))
            tasks[to].file_table[i] = invalid_fd;
        else
        {
            tasks[to].file_table[i] = tasks[from].file_table[i];
            if (tasks[to].file_table[i] != invalid_fd)
                file_table[tasks[to].file_table[i]].used++;
        }
    }
    release_mutex(&file_table_lock);
}

void copy_task(uint16_t index)
{
    if (index >= task_count || index == 0)
    {
        LOG(CRITICAL, "Invalid task index %u", index);
        abort();
        return;
    }

    task_count++;

    const uint16_t new_task_index = task_count - 1;

    tasks[new_task_index] = tasks[index];

    memcpy(tasks[new_task_index].name, tasks[index].name, THREAD_NAME_MAX);
    tasks[new_task_index].ring = tasks[index].ring;
    tasks[new_task_index].pid = tasks[index].forked_pid;
    tasks[new_task_index].system_task = tasks[index].system_task;
    tasks[new_task_index].reading_stdin = false;
    
    utf32_buffer_create_and_copy(&tasks[index].input_buffer, &tasks[new_task_index].input_buffer);

    tasks[new_task_index].cr3 = task_create_empty_vas(tasks[new_task_index].ring == 0 ? PG_SUPERVISOR : PG_USER);
    tasks[new_task_index].rsp = tasks[index].rsp;

    tasks[new_task_index].fpu_state = fpu_state_create_copy(tasks[index].fpu_state);

    task_copy_file_table(index, new_task_index, false);

    tasks[new_task_index].forked_pid = 0;
    tasks[new_task_index].is_dead = false;

    tasks[new_task_index].ppid = tasks[index].pid;
    tasks[new_task_index].wait_pid = -1;

    task_vas_copy((uint64_t*)(tasks[index].cr3 + PHYS_MAP_BASE), (uint64_t*)(tasks[new_task_index].cr3 + PHYS_MAP_BASE), 0, TASK_STACK_TOP_ADDRESS >> 12);
}

void cleanup_tasks()
{
    if (current_task_index != 0) return;

    for (uint16_t i = 0; i < task_count; i++)
    {
        if (i == current_task_index) continue;
        if (tasks[i].forked_pid)
        {
            copy_task(i);
            tasks[i].forked_pid = 0;
        }
    }

    for (uint16_t i = 0; i < task_count; i++)
    {
        if (!(tasks[i].is_dead && task_can_be_killed(i))) continue;
        thread_t* parent = find_task_by_pid(tasks[i].ppid);
        if (parent)
        {
            if (parent->wait_pid != -1)
            {
                if (parent->wait_pid == 0 || absint(parent->wait_pid) == tasks[i].pid)
                {
                    tasks[i].to_reap = true;
                    parent->wait_pid = -1;
                    parent->wstatus = tasks[i].return_value;
                }
            }
        }
        else
            tasks[i].to_reap = true;
    }

    for (uint16_t i = 0; i < task_count; i++)
    {
        if (i == current_task_index) continue;
        if (tasks[i].to_reap)
        {
            task_kill(i);
            i--;
            continue;
        }
    }
}

void tasks_log()
{
    LOG(DEBUG, "%u tasks: (Total CPU usage: %u.%u)", task_count, (1000 - tasks[0].stored_cpu_ticks) / 10,  (1000 - tasks[0].stored_cpu_ticks) % 10);
    lock_task_queue();
    for (uint16_t i = 0; i < task_count; i++)
    {
        thread_t* task = &tasks[i];
        LOG(DEBUG, "%s── task \"%s\" [pid %d, ppid %d, pgid %d] | CPU Usage: %u.%u%s%s%s", i == task_count - 1 ? "└" : "├",
            task->name, task->pid, task->ppid, task->pgid, task->stored_cpu_ticks / 10, task->stored_cpu_ticks % 10,
            task_is_blocked(i) ? " (blocked)" : "", 
            task->pgid == tty_foreground_pgrp ? " (foreground)" : " (background)",
            task->doing_io ? " (waiting for io)" : "",
            task->is_dead ? " (dead)" : "",
            task->to_reap ? " (waiting to be reaped)" : "");
    }
    unlock_task_queue();
}

pid_t task_generate_pid()
{
    static pid_t current = 0;
    return current++;
}