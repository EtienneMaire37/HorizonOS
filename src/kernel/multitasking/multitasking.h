#pragma once

#include "task.h"
#include "../util/hashmap.h"
#include "queue.h"

extern hashmap_t* futex_hashmap;

extern mutex_t file_table_lock;
extern uint8_t global_cpu_ticks;

extern thread_t* running_tasks;
#define idle_task running_tasks
extern uint16_t task_count;

extern uint64_t multitasking_counter;

extern thread_t* current_task;
extern bool multitasking_enabled;

extern pid_t task_reading_stdin;
extern utf32_buffer_t keyboard_input_buffer;

extern int task_lock_depth;


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
        enable_interrupts();
}

static inline file_entry_t* get_global_file_entry(int fd)
{
    return &file_table[current_task->file_table[fd].index];
}

void multitasking_init();
void multitasking_start();
void multitasking_add_idle_task(char* name);
void multitasking_add_task(thread_t* task);
void multitasking_remove_task(thread_t* task);

thread_t* find_running_task_by_pid(pid_t pid);
thread_t* find_task_by_pid_in_queue(thread_queue_t* queue, pid_t pid);
thread_t* find_task_by_pid_anywhere(pid_t pid);

pid_t waitpid_find_child_in_tq(thread_queue_t* queue, pid_t pid, int* wstatus, int pgid_on_call);