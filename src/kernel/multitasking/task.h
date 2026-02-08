#pragma once

#include "../io/keyboard.h"
#include "../vfs/vfs.h"
#include <limits.h>
#include "../gdt/gdt.h"
#include "../cpu/segbase.h"
#include <signal.h>

#include "mutex.h"

#define THREAD_NAME_MAX 64
#define NUM_SIGNALS     (sizeof(sigset_t) * 8)

typedef struct thread thread_t;

typedef struct thread
{
    char name[THREAD_NAME_MAX];

    uint64_t rsp, cr3;
    uint64_t fs_base, gs_base;

    sigset_t sig_pending, sig_mask;
    struct sigaction sig_act_array[NUM_SIGNALS];

    bool is_dead;
    uint16_t doing_io;  // makes thread unkillable while doing io

    uint32_t return_value;

    pid_t wait_pid;
    uint32_t wstatus;

    bool to_reap;

    pid_t forked_pid;

    uint8_t ring;
    pid_t pid, ppid, pgid;
    bool system_task;    // cause kernel panics

    vfs_folder_tnode_t* cwd;

    uint8_t* fpu_state;

    uint16_t stored_cpu_ticks, current_cpu_ticks;   // * In milliseconds

    file_table_index_t file_table[OPEN_MAX];
    mutex_t file_table_mutex;

    thread_t *prev, *next; 
} thread_t;

extern const uint64_t task_rsp_offset;
extern const uint64_t task_cr3_offset;

extern void syscall_handler();

#define TASK_STACK_PAGES            0x400       // * 4MB
#define TASK_KERNEL_STACK_PAGES     0x20        // * 128KB

#define TASK_STACK_TOP_ADDRESS              0x800000000000 // 0x7fffffffffff
#define TASK_STACK_BOTTOM_ADDRESS           (TASK_STACK_TOP_ADDRESS - 0x1000 * TASK_STACK_PAGES)
#define TASK_KERNEL_STACK_TOP_ADDRESS       TASK_STACK_BOTTOM_ADDRESS
#define TASK_KERNEL_STACK_BOTTOM_ADDRESS    (TASK_KERNEL_STACK_TOP_ADDRESS - 0x1000 * TASK_KERNEL_STACK_PAGES)

#define TASK_SWITCH_DELAY 40 // ms
#define TASK_SWITCHES_PER_SECOND (1000 / TASK_SWITCH_DELAY)

extern pid_t task_reading_stdin;
extern utf32_buffer_t keyboard_input_buffer;

extern mutex_t file_table_lock;
extern uint8_t global_cpu_ticks;

extern thread_t* running_tasks;
#define idle_task running_tasks
extern uint16_t task_count;

extern uint64_t multitasking_counter;

extern thread_t* current_task;
extern bool multitasking_enabled;
extern volatile bool first_task_switch;

extern int task_lock_depth;

extern void iretq_instruction();
void task_kill(thread_t* task);

void apic_enable();
void apic_disable();

static inline void lock_task_queue()
{
    disable_interrupts();
    task_lock_depth++;
}

static inline void unlock_task_queue()
{
    disable_interrupts();
    if (--task_lock_depth == 0)
        enable_interrupts();
}

static inline int vfs_allocate_thread_file(thread_t* task)
{
    acquire_mutex(&task->file_table_mutex);
    for (int i = 3; i < OPEN_MAX; i++)
    {
        if (task->file_table[i] == invalid_fd)
            return (release_mutex(&task->file_table_mutex), i);
    }
    release_mutex(&task->file_table_mutex);
    return -1;
}

static inline void end_context_switch();

// !!! Assumes task queue is locked
extern void context_switch(thread_t* old_tcb, thread_t* next_tcb, uint64_t ds, uint8_t* old_fpu_state, uint8_t* next_fpu_state);
static inline void full_context_switch(thread_t* next)
{
    thread_t* old_task = current_task;
    current_task = next;
    TSS.rsp0 = TASK_KERNEL_STACK_TOP_ADDRESS;

    if (old_task->ring != 0)
        swapgs();

    old_task->fs_base = rdfsbase();
    old_task->gs_base = rdgsbase();
    
    context_switch(old_task, current_task, current_task->ring == 0 ? KERNEL_DATA_SEGMENT : USER_DATA_SEGMENT,
    old_task->fpu_state, current_task->fpu_state);

    end_context_switch();
}

static inline void end_context_switch()
{
    wrfsbase(current_task->fs_base);
    wrgsbase(current_task->gs_base);

    if (current_task->ring != 0)
        swapgs();
}

static inline bool task_can_be_killed(thread_t* task)
{
    lock_task_queue();
    bool ret = !task->doing_io;
    unlock_task_queue();
    return ret;
}

static inline bool task_is_blocked(thread_t* task)
{
    lock_task_queue();
    if (task->is_dead && task_can_be_killed(task)) return (unlock_task_queue(), true);
    if (task_reading_stdin == task->pid) return (unlock_task_queue(), true);
    if (task->forked_pid) return (unlock_task_queue(), true);
    if (task->wait_pid != -1) return (unlock_task_queue(), true);
    unlock_task_queue();
    return false;
}

static inline thread_t* find_next_task()
{
    for (thread_t* task = current_task->next; task != current_task; task = task->next)
    {
        if (task == idle_task) continue;
        if (!task_is_blocked(task)) return task;
    }

    if (!task_is_blocked(current_task)) return current_task;
    return idle_task;
}

static inline bool is_fd_valid(int fd)
{
    if (fd < 0 || fd >= OPEN_MAX)
        return false;
    if (current_task->file_table[fd] == invalid_fd)
        return false;
    return true;
}

pid_t task_generate_pid();
void multitasking_add_task(thread_t* task);

void task_write_at_address_1b(thread_t* task, uint64_t address, uint8_t value);
void task_write_at_aligned_address_8b(thread_t* task, uint64_t address, uint64_t value);
void task_write_at_address_8b(thread_t* task, uint64_t address, uint64_t value);
uint8_t task_read_at_address_1b(thread_t* task, uint64_t address);
uint64_t task_read_at_aligned_address_8b(thread_t* task, uint64_t address);
uint64_t task_read_at_address_8b(thread_t* task, uint64_t address);

void task_setup_stack(thread_t* task, uint64_t entry_point, uint16_t code_seg, uint16_t data_seg);
void task_set_name(thread_t* task, const char* name);

thread_t* task_create_empty();
void task_destroy(thread_t* task);
void switch_task();
void multitasking_init();
void multitasking_start();
void multitasking_add_idle_task();

thread_t* find_task_by_pid(pid_t pid);

void task_init_file_table(thread_t* task);
void task_copy_file_table(thread_t* from, thread_t* to, bool cloexec);

void task_stack_push(thread_t*, uint64_t);
void task_stack_push_string(thread_t* task, const char* str);
void task_stack_push_data(thread_t* task, void* data, size_t bytes);

void cleanup_tasks();

void tasks_log();

void kill_current_task(int ret);
void task_mask_signal(thread_t* index, int sig);
void task_set_pending_signal(thread_t* index, int sig);
void task_queue_signal(thread_t* index, int sig);