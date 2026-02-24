#pragma once

#include "task.h"
#include "../util/hashmap.h"
#include "queue.h"

extern hashmap_t *futex_tq_hashmap, 
                 *pid_to_task_hashmap, 
                 *pgid_to_tq_hashmap, 
                 *pid_to_children_tq_hashmap;

extern mutex_t file_table_lock;
extern uint8_t global_cpu_ticks;

extern thread_t* running_tasks;
#define idle_task running_tasks
extern uint16_t task_count;

extern uint64_t multitasking_counter;

extern thread_t* current_task;
extern thread_t* last_task;
extern bool multitasking_enabled;

extern pid_t task_reading_stdin;
extern utf32_buffer_t keyboard_input_buffer;

extern int task_lock_depth;
extern bool queued_ts;

extern void iretq_instruction();

static inline void lock_scheduler()
{
    disable_interrupts();
    task_lock_depth++;
}

static inline void unlock_scheduler()
{
    disable_interrupts();
    if (--task_lock_depth == 0)
    {
        if (queued_ts)
        {
            queued_ts = false;
            enable_interrupts();
            switch_task();
        }
        enable_interrupts();
    }
}

static inline file_entry_t* get_global_file_entry(int fd)
{
    return &file_table[current_task->file_table[fd].index];
}

static inline void vfs_close(int fd)
{
    lock_scheduler();
    if (is_fd_valid(fd))
    {
        vfs_remove_global_file(current_task->file_table[fd].index);
        current_task->file_table[fd].index = invalid_fd;
    }
    unlock_scheduler();
}

static inline bool multitasking_is_pgrp_empty(pid_t pgid)
{
    thread_queue_t* tq = hashmap_get_item(pgid_to_tq_hashmap, pgid);
    if (!tq) return true;
    return *tq == NULL;
}

static inline void log_tq(thread_queue_t* tq)
{
    LOG(DEBUG, "{");
    if (tq && *tq)
    {
        thread_queue_item_t* it = *tq;
        do
        {
            thread_t* cur = it->data;
            LOG(DEBUG, "\t\"%s\" (pid = %d)%s", cur->name, cur->pid, it->next == *tq ? "" : ",");
            it = it->next;
        } while (it != *tq);
    }
    LOG(DEBUG, "}");
}

static inline void log_context(thread_t* task)
{
    if (!task) return;
    LOG(DEBUG, "log_context (rsp = %#16" PRIx64 ")", task->rsp);
    for (int i = 0; i < 20; i++) 
    {
        if (task->rsp + (i + 1) * 8 >= TASK_STACK_TOP_ADDRESS)
            break;
        LOG(DEBUG, "%#16" PRIx64 " (rsp + %d) = %#16" PRIx64, task->rsp + 8 * i, i, task_read_at_address_8b(task, task->rsp + 8 * i));
    }
}

void multitasking_init();
void multitasking_start();
void multitasking_add_idle_task(char* name);
void multitasking_add_task(thread_t* task);
void multitasking_remove_task(thread_t* task);

thread_t* find_running_task_by_pid(pid_t pid);
thread_t* find_task_by_pid_in_queue(thread_queue_t* queue, pid_t pid);
thread_t* find_task_by_pid_anywhere(pid_t pid);

void task_send_signal_to_pgrp(int sig, pid_t pgrp);
void task_send_signal(thread_t* thread, int sig);
void task_handle_signal(thread_t* thread, int sig);
void task_try_handle_signals(thread_t* thread, sigset_t old, sigset_t new);

pid_t waitpid_find_child_in_tq(thread_queue_t* queue, pid_t pid, int* wstatus, int pgid_on_call);