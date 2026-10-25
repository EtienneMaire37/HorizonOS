#pragma once

#include "../util/hashmap.h"
#include "thread_def.h"
#include "../io/keyboard.h"
#include <assert.h>

extern hashmap_t *pid_to_task_hashmap,
                 *pgid_to_tq_hashmap,
                 *pid_to_children_tq_hashmap;

extern uint8_t global_cpu_ticks;

extern thread_t* running_tasks;
#define idle_task running_tasks
extern _Atomic uint16_t task_count;

extern uint64_t multitasking_counter;

extern thread_t* current_task;
extern bool multitasking_enabled;

extern utf32_buffer_t keyboard_input_buffer, keyboard_buffered_input_buffer;
extern spinlock_noint_t keyboard_input_lock;

extern bool queued_ts;

extern void iretq_instruction();

static inline bool __multitasking_is_pgrp_empty(pid_t pgid)
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

void multitasking_init();
void multitasking_start();
void multitasking_add_idle_task(char* name);
void __multitasking_add_task(thread_t* task);
void __multitasking_remove_task(thread_t* task);

void __task_stop(thread_t* thread, int sig);
void __task_continue(thread_t* thread);

thread_t* __find_running_task_by_pid(pid_t pid);
thread_t* __find_task_by_pid_in_queue(void* queue, pid_t pid);
thread_t* __find_task_by_pid_anywhere(pid_t pid);

void __task_handle_sig_dfl(thread_t* task, int sig);
void __task_send_signal_to_pgrp(int sig, pid_t pgrp);
void task_send_signal_to_pgrp(int sig, pid_t pgrp);
void task_send_signal(thread_t* thread, int sig);
void __task_send_signal(thread_t* thread, int sig);
void __task_handle_signal(thread_t* thread, int sig);
void __task_try_handle_signals(thread_t* thread, sigset_t old, sigset_t new);

pid_t __waitpid_find_child_in_tq(thread_queue_t* queue, pid_t pid, int* wstatus, int pgid_on_call);
