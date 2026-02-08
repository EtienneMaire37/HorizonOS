#pragma once

#include "task.h"

typedef struct thread_queue_item thread_queue_item_t;

typedef struct thread_queue_item
{
    thread_t* data;
    thread_queue_item_t *prev, *next;
} thread_queue_item_t;

typedef thread_queue_item_t* thread_queue_t;

#define TQ_INIT     NULL

extern thread_queue_t dead_tasks, reapable_tasks;

void thread_queue_push_back(thread_queue_t* queue, thread_t* data);
void thread_queue_remove(thread_queue_t* queue, thread_queue_item_t* data);

void move_running_task_to_thread_queue(thread_queue_t* queue, thread_t* task);
void move_task_to_running_queue(thread_queue_t* queue, thread_queue_item_t* task);
void move_task_from_to_thread_queue(thread_queue_t* queue1, thread_queue_t* queue2, thread_queue_item_t* item);