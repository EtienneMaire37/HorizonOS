#pragma once

#include "task.h"
#include "../util/hashmap.h"

extern hashmap_t* futex_hashmap;

extern mutex_t file_table_lock;
extern uint8_t global_cpu_ticks;

extern thread_t* running_tasks;
#define idle_task running_tasks
extern uint16_t task_count;

extern uint64_t multitasking_counter;

extern thread_t* current_task;
extern bool multitasking_enabled;
extern volatile bool first_task_switch;

extern pid_t task_reading_stdin;
extern utf32_buffer_t keyboard_input_buffer;

extern int task_lock_depth;


extern void iretq_instruction();

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

void multitasking_init();
void multitasking_start();
void multitasking_add_idle_task(char* name);
void multitasking_add_task(thread_t* task);
void multitasking_remove_task(thread_t* task);
