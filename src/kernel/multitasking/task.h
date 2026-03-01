#pragma once

#include "../io/keyboard.h"
#include "../vfs/vfs.h"
#include <limits.h>
#include "../gdt/gdt.h"
#include "../cpu/segbase.h"
#include <signal.h>
#include <elf.h>

#include "mutex.h"

#define THREAD_NAME_MAX 64
#define NUM_SIGNALS     (SIGRTMAX + 1)

typedef struct thread thread_t;

typedef struct thread
{
    char name[THREAD_NAME_MAX];

    uint64_t rsp, cr3;
    uint64_t fs_base, gs_base;
    uint64_t context_sp;

    sigset_t sig_pending, sig_mask;
    struct sigaction sig_act_array[NUM_SIGNALS];
    bool sig_pending_user_space;

    uint32_t return_value;
    
    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;

    uint64_t pending_signal_handler;

    // * waitpid shenanigans
    pid_t wait_pid, waitpid_ret, pgid_on_waitpid;
    int waitpid_flags;
    uint32_t wstatus;

    pid_t forked_pid;

    void* queue;

    uint8_t ring;
    pid_t pid, ppid, pgid;
    bool system_task;    // * Allow causing kernel panics

    vfs_folder_tnode_t* cwd;

    uint8_t* fpu_state;

    uint16_t stored_cpu_ticks, current_cpu_ticks;   // * In milliseconds

    file_table_index_t file_table[OPEN_MAX];
    mutex_t file_table_mutex;

    // * We still have to keep them here to fix the case 
    // * where the current process is blocked before context switching
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

static inline int vfs_allocate_thread_file(thread_t* task)
{
    acquire_mutex(&task->file_table_mutex);
    for (int i = 3; i < OPEN_MAX; i++)
    {
        if (task->file_table[i].index == invalid_fd)
            return (release_mutex(&task->file_table_mutex), i);
    }
    release_mutex(&task->file_table_mutex);
    return -1;
}

// !!! Assumes task queue is locked
extern void context_switch(thread_t* old_tcb, thread_t* next_tcb, uint64_t ds);
void full_context_switch(thread_t* next);
void end_context_switch();
bool task_is_blocked(thread_t* task);
thread_t* find_next_task();
bool is_fd_valid(int fd);

pid_t task_generate_pid();
void task_set_pgid(thread_t* task, pid_t pgid);

void multitasking_add_task(thread_t* task);
void multitasking_remove_task(thread_t* task);

void task_write_at_address_1b(thread_t* task, uint64_t address, uint8_t value);
void task_write_at_aligned_address_8b(thread_t* task, uint64_t address, uint64_t value);
void task_write_at_address_8b(thread_t* task, uint64_t address, uint64_t value);
uint8_t task_read_at_address_1b(thread_t* task, uint64_t address);
uint64_t task_read_at_aligned_address_8b(thread_t* task, uint64_t address);
uint64_t task_read_at_address_8b(thread_t* task, uint64_t address);

void task_setup_stack(thread_t* task, uint64_t entry_point);
void task_unsetup_stack(thread_t* task);
void task_set_name(thread_t* task, const char* name);

thread_t* task_create_empty();
void task_destroy(thread_t* task);
void switch_task();
void multitasking_init();
void multitasking_start();
void multitasking_add_idle_task();

void task_init_file_table(thread_t* task);
void task_copy_file_table(thread_t* from, thread_t* to, bool cloexec);

void task_stack_push(thread_t*, uint64_t);
void task_stack_push_auxv(thread_t* task, Elf64_auxv_t val);
void task_stack_push_string(thread_t* task, const char* str);
void task_stack_push_data(thread_t* task, void* data, size_t bytes);

void task_context_stack_push(thread_t* task, uint64_t rsp);
uint64_t task_context_stack_pop(thread_t* task);

void cleanup_tasks();

void tasks_log();

void kill_task(thread_t* task, int ret);

void task_mask_signal(thread_t* task, int sig);
void task_unmask_signal(thread_t* task, int sig);
void task_set_pending_signal(thread_t* task, int sig);
void task_queue_signal(thread_t* task, int sig);

void waitpid_check_dead();