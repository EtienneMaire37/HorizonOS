#pragma once

#include <limits.h>
#include <signal.h>
#include <elf.h>
#include <stdint.h>
#include "../vfs/vfs.h"
#include "../util/linked_list.h"

#define THREAD_NAME_MAX 64
#define NUM_SIGNALS     (SIGRTMAX + 1)

typedef struct thread thread_t;

typedef struct thread
{
    ll_t _poll_tqs;

    precise_time_t timeout_deadline;

    uint32_t return_value;

    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;

    mode_t umask;

    uint64_t pending_signal_handler;
    int pending_signal_number;

    sigset_t sig_pending, sig_mask;
    struct sigaction sig_act_array[NUM_SIGNALS];
    bool sig_pending_user_space;

    // * waitpid shenanigans
    pid_t wait_pid, waitpid_ret, pgid_on_waitpid;
    int waitpid_flags;
    uint32_t wstatus;

    pid_t forked_pid;

    void* queue;

    uint8_t ring;
    pid_t pid, ppid, pgid, sid;
    bool system_task;    // * Allow causing kernel panics

    vfs_folder_tnode_t* cwd;
    uint8_t* fpu_state;

    file_table_index_t file_table[OPEN_MAX];

    int lock_depth;

    uint16_t stored_cpu_ticks, current_cpu_ticks;   // * In milliseconds

    uint64_t rsp, cr3;
    uint64_t fs_base, gs_base;

    char name[THREAD_NAME_MAX];

    // * We still have to keep them here in the case
    // * where the current process is blocked before context switching
    thread_t *prev, *next;
} thread_t;

#include "../io/keyboard.h"
#include "../vfs/vfs.h"
#include "../gdt/gdt.h"
#include "../cpu/segbase.h"
#include "mutex.h"

extern const uint64_t task_rsp_offset;
extern const uint64_t task_cr3_offset;

extern void syscall_handler();

#define TASK_STACK_PAGES            0x400       // * 4MB
#define TASK_KERNEL_STACK_PAGES     0x20        // * 128KB

#define TASK_STACK_TOP_ADDRESS              0x800000000000 // 0x7fffffffffff
#define TASK_STACK_BOTTOM_ADDRESS           (TASK_STACK_TOP_ADDRESS - 0x1000 * TASK_STACK_PAGES)
#define TASK_KERNEL_STACK_TOP_ADDRESS       TASK_STACK_BOTTOM_ADDRESS
#define TASK_KERNEL_STACK_BOTTOM_ADDRESS    (TASK_KERNEL_STACK_TOP_ADDRESS - 0x1000 * TASK_KERNEL_STACK_PAGES)

#define TASK_SWITCH_DELAY 10 // ms
#define TASK_SWITCHES_PER_SECOND (1000 / TASK_SWITCH_DELAY)

// !!! Assumes task queue is locked
extern void context_switch(thread_t* old_tcb, thread_t* next_tcb, uint64_t ds);
void full_context_switch(thread_t* next);
void end_context_switch();
thread_t* find_next_task();
bool is_fd_valid(int fd);

pid_t task_generate_pid();
void task_set_pid(thread_t* task, pid_t pid);
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

void cleanup_tasks();

void tasks_log();

void kill_task(thread_t* task, int ret);

void task_mask_signal(thread_t* task, int sig);
void task_unmask_signal(thread_t* task, int sig);
void task_set_pending_signal(thread_t* task, int sig);
void task_unset_pending_signal(thread_t* task, int sig);
void task_queue_signal(thread_t* task, int sig);

void waitpid_check_dead();
