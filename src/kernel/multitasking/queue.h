#pragma once

#include "task.h"

#include "queue_def.h"

extern thread_queue_t   dead_tasks,
                        reapable_tasks,
                        waitpid_tasks,
                        forked_tasks,
                        stopped_tasks,
                        _waiting_for_stdin_tasks,
                        waiting_for_time_tasks;

void __thread_queue_push_back(thread_queue_t* queue, thread_t* data);
void __thread_queue_remove(thread_queue_t* queue, thread_queue_item_t* data);

void __copy_task_to_thread_queue(thread_queue_t* queue, thread_t* task);
void __remove_task_from_thread_queue(thread_queue_t* queue, thread_t* task);
void __move_running_task_to_thread_queue(thread_queue_t* queue, thread_t* task);
void __move_task_to_running_queue(thread_queue_t* queue, thread_t* task);
void __move_task_to_running_queue_by_item(thread_queue_t* queue, thread_queue_item_t* task);
void __move_task_from_to_thread_queue(thread_queue_t* queue1, thread_queue_t* queue2, thread_t* item);
void __move_task_from_to_thread_queue_by_item(thread_queue_t* queue1, thread_queue_t* queue2, thread_queue_item_t* item);
void __move_task_to_queue(void* queue, thread_t* task);
void __remove_all_tasks_from_queue(thread_queue_t* tq);
void __move_all_tasks_to_running_queue(thread_queue_t* tq);
void __move_n_tasks_to_running_queue(thread_queue_t* tq, int n);
void __filter_tasks_to_running_queue(thread_queue_t* tq, bool (*test)(thread_t* task));
void __run_it_on_queue(thread_queue_t* tq, void (*func)(thread_t* task));
