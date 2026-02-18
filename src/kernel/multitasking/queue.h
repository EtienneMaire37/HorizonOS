#pragma once

#include "task.h"
#include "../util/linked_list.h"

#define thread_queue_item_t ll_item_t
#define thread_queue_t ll_t

#define TQ_INIT     LL_INIT

extern thread_queue_t   dead_tasks, 
                        reapable_tasks, 
                        waitpid_tasks, 
                        forked_tasks, 
                        stopped_tasks,
                        pending_signal_tasks;

void thread_queue_push_back(thread_queue_t* queue, thread_t* data);
void thread_queue_remove(thread_queue_t* queue, thread_queue_item_t* data);

void move_running_task_to_thread_queue(thread_queue_t* queue, thread_t* task);
void move_task_to_running_queue(thread_queue_t* queue, thread_queue_item_t* task);
void move_task_from_to_thread_queue(thread_queue_t* queue1, thread_queue_t* queue2, thread_queue_item_t* item);
void move_task_to_queue(void* queue, thread_t* task);