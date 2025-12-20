#pragma once

#include "../io/keyboard.h"
#include "mutex.h"

#define THREAD_NAME_MAX 64

typedef struct thread
{
    char name[THREAD_NAME_MAX];

    uint64_t rsp, cr3;

    utf32_buffer_t input_buffer;
    bool reading_stdin, is_dead;
    uint16_t doing_io;

    uint32_t return_value;

    pid_t wait_pid;
    uint32_t wstatus;

    bool to_reap;

    pid_t forked_pid;

    uint8_t ring;
    pid_t pid, ppid, pgid;
    bool system_task;    // system_task: cause kernel panics

    vfs_folder_tnode_t* cwd;

    uint8_t* fpu_state;

    uint16_t stored_cpu_ticks, current_cpu_ticks;   // * In milliseconds

    file_table_index_t file_table[OPEN_MAX];
} thread_t;

mutex_t file_table_lock = MUTEX_INIT;

#define __CURRENT_TASK      tasks[current_task_index]

uint8_t global_cpu_ticks = 0;

const uint64_t task_rsp_offset = offsetof(thread_t, rsp);
const uint64_t task_cr3_offset = offsetof(thread_t, cr3);

#define TASK_STACK_PAGES            0x400       // * 4MB
#define TASK_KERNEL_STACK_PAGES     0x20        // * 128KB

#define TASK_STACK_TOP_ADDRESS              0x800000000000 // 0x7fffffffffff
#define TASK_STACK_BOTTOM_ADDRESS           (TASK_STACK_TOP_ADDRESS - 0x1000 * TASK_STACK_PAGES)
#define TASK_KERNEL_STACK_TOP_ADDRESS       TASK_STACK_BOTTOM_ADDRESS
#define TASK_KERNEL_STACK_BOTTOM_ADDRESS    (TASK_KERNEL_STACK_TOP_ADDRESS - 0x1000 * TASK_KERNEL_STACK_PAGES)

#define MAX_TASKS 256

thread_t tasks[MAX_TASKS];    // TODO : Implement a dynamic array
uint16_t task_count;

#define TASK_SWITCH_DELAY 40 // ms
#define TASK_SWITCHES_PER_SECOND (1000 / TASK_SWITCH_DELAY)

uint64_t multitasking_counter = TASK_SWITCH_DELAY;

uint16_t current_task_index = 0;
bool multitasking_enabled = false;
volatile bool first_task_switch = true;

extern void iretq_instruction();
void task_kill(uint16_t index);

void apic_enable();
void apic_disable();

void lock_task_queue()
{
    // lapic_disable();
    disable_interrupts();
    // acquire_spinlock(&task_queue_spinlock);
}

void unlock_task_queue()
{
    // lapic_enable();
    enable_interrupts();
    // release_spinlock(&task_queue_spinlock);
}

int vfs_allocate_thread_file(int index)
{
    for (int i = 3; i < OPEN_MAX; i++)
    {
        if (tasks[index].file_table[i] == invalid_fd)
            return i;
    }
    return -1;
}

extern void __attribute__((sysv_abi)) context_switch(thread_t* old_tcb, thread_t* next_tcb, uint64_t ds, uint8_t* old_fpu_state, uint8_t* next_fpu_state);
void full_context_switch(uint16_t next_task_index)
{
    int last_index = current_task_index;
    current_task_index = next_task_index;
    TSS.rsp0 = TASK_KERNEL_STACK_TOP_ADDRESS;
    
    context_switch(&tasks[last_index], &__CURRENT_TASK, __CURRENT_TASK.ring == 0 ? KERNEL_DATA_SEGMENT : USER_DATA_SEGMENT,
    tasks[last_index].fpu_state, __CURRENT_TASK.fpu_state);
}

bool task_can_be_killed(uint16_t i)
{
    return !tasks[i].doing_io;
}

bool task_is_blocked(uint16_t index)
{
    if (tasks[index].is_dead && task_can_be_killed(index)) return true;
    if (tasks[index].reading_stdin) return true;
    if (tasks[index].forked_pid) return true;
    if (tasks[index].wait_pid != -1) return true;
    return false;
}

uint16_t find_next_task_index()
{
    for (uint16_t off = 1; off < task_count; off++)
    {
        uint16_t idx = (current_task_index + off) % task_count;
        if (idx == 0) continue;
        if (!task_is_blocked(idx)) return idx;
    }

    if (!task_is_blocked(current_task_index)) return current_task_index;
    return 0;
}

bool is_fd_valid(int fd)
{
    if (fd < 0 || fd >= OPEN_MAX)
        return false;
    if (__CURRENT_TASK.file_table[fd] == invalid_fd)
        return false;
    return true;
}

pid_t task_generate_pid()
{
    static pid_t current = 0;
    return current++;
}

void task_write_at_address_1b(thread_t* task, uint64_t address, uint8_t value);
void task_write_at_aligned_address_8b(thread_t* task, uint64_t address, uint64_t value);
void task_write_at_address_8b(thread_t* task, uint64_t address, uint64_t value);

void task_setup_stack(thread_t* task, uint64_t entry_point, uint16_t code_seg, uint16_t data_seg);
void task_set_name(thread_t* task, const char* name);

void task_destroy(thread_t* task);
void switch_task();
void multitasking_init();
void multitasking_start();
void task_kill(uint16_t index);
void multitasking_add_idle_task();

thread_t* find_task_by_pid(pid_t pid);

void task_init_file_table(thread_t* task);
void task_copy_file_table(uint16_t from, uint16_t to, bool cloexec);

void task_stack_push(thread_t*, uint64_t);

void cleanup_tasks();

void idle_main();

void tasks_log();