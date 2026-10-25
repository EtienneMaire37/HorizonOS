#pragma once

#include <elf.h>
#include "sched_lock.h"
#include "thread_def.h"

// !!! Assumes task queue is locked
extern void context_switch(thread_t* old_tcb, thread_t* next_tcb, uint64_t ds);
void end_context_switch();
thread_t* __find_next_task();

pid_t task_generate_pid();
void __task_set_pid(thread_t* task, pid_t pid);
void __task_set_pgid(thread_t* task, pid_t pgid);

void task_write_at_address_1b(thread_t* task, uint64_t address, uint8_t value);
void task_write_at_aligned_address_8b(thread_t* task, uint64_t address, uint64_t value);
void task_write_at_address_8b(thread_t* task, uint64_t address, uint64_t value);
uint8_t task_read_at_address_1b(thread_t* task, uint64_t address);
uint64_t task_read_at_aligned_address_8b(thread_t* task, uint64_t address);
uint64_t task_read_at_address_8b(thread_t* task, uint64_t address);

void task_setup_stack(thread_t* task, uint64_t entry_point);
void task_set_name(thread_t* task, const char* name);

thread_t* task_create_empty();
thread_t* __task_create_empty();
void task_destroy(thread_t* task);
void __task_destroy(thread_t* task);
void switch_task();
void multitasking_init();
void multitasking_start();

void __task_copy_file_table(thread_t* from, thread_t* to, bool cloexec);
void task_copy_file_table(thread_t* from, thread_t* to, bool cloexec);

void task_stack_push(thread_t*, uint64_t);
void task_stack_push_auxv(thread_t* task, Elf64_auxv_t val);
void task_stack_push_string(thread_t* task, const char* str);
void task_stack_push_data(thread_t* task, void* data, size_t bytes);

vnode_ref_t task_get_cwd(thread_t* task);

void cleanup_tasks();

void tasks_log();

void kill_task(thread_t* task, int ret);
void __kill_task(thread_t* task, int ret);

void __task_unmask_signal(thread_t* task, int sig);
void __task_set_pending_signal(thread_t* task, int sig);
void __task_unset_pending_signal(thread_t* task, int sig);

void __waitpid_check_dead();

static inline void task_enter_critical_section(thread_t* task)
{
    if (!task) return;
    uint32_t flags = lock_scheduler();
    task->can_be_killed = false;
    unlock_scheduler(flags);
}
static inline void task_exit_critical_section(thread_t* task)
{
    if (!task) return;
    uint32_t flags = lock_scheduler();
    task->can_be_killed = true;
    if (task->waiting_for_kill)
        __kill_task(task, task->waiting_for_kill_ret);
    unlock_scheduler(flags);
}
