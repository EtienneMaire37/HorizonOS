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
                        _waiting_for_stdin_tasks,
                        waiting_for_time_tasks;

void thread_queue_push_back(thread_queue_t* queue, thread_t* data);
void thread_queue_remove(thread_queue_t* queue, thread_queue_item_t* data);

void copy_task_to_thread_queue(thread_queue_t* queue, thread_t* task);
void remove_task_from_thread_queue(thread_queue_t* queue, thread_t* task);
void move_running_task_to_thread_queue(thread_queue_t* queue, thread_t* task);
void move_task_to_running_queue(thread_queue_t* queue, thread_t* task);
void move_task_to_running_queue_by_item(thread_queue_t* queue, thread_queue_item_t* task);
void move_task_from_to_thread_queue(thread_queue_t* queue1, thread_queue_t* queue2, thread_t* item);
void move_task_from_to_thread_queue_by_item(thread_queue_t* queue1, thread_queue_t* queue2, thread_queue_item_t* item);
void move_task_to_queue(void* queue, thread_t* task);
void remove_all_tasks_from_queue(thread_queue_t* tq);
void move_all_tasks_to_running_queue(thread_queue_t* tq);
void move_n_tasks_to_running_queue(thread_queue_t* tq, int n);
void filter_tasks_to_running_queue(thread_queue_t* tq, bool (*test)(thread_t* task));
void run_it_on_queue(thread_queue_t* tq, void (*func)(thread_t* task));
