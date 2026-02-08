#include "queue.h"

thread_queue_t dead_tasks = TQ_INIT;
thread_queue_t reapable_tasks = TQ_INIT;

void thread_queue_push_back(thread_queue_t* queue, thread_t* data)
{
    // !! Should probably have a different lock per queue
    // TODO: Improve this
    lock_task_queue();

    ll_push_back(queue, data);

    unlock_task_queue();
}

void thread_queue_remove(thread_queue_t* queue, thread_queue_item_t* item)
{
    // !! Same here
    lock_task_queue();

    ll_remove(queue, item);

    unlock_task_queue();
}

void move_running_task_to_thread_queue(thread_queue_t* queue, thread_t* task)
{
    lock_task_queue();
    multitasking_remove_task(task);
    thread_queue_push_back(queue, task);
    unlock_task_queue();
}

void move_task_to_running_queue(thread_queue_t* queue, thread_queue_item_t* item)
{
    lock_task_queue();
    multitasking_add_task(item->data);
    thread_queue_remove(queue, item);
    unlock_task_queue();
}

void move_task_from_to_thread_queue(thread_queue_t* queue1, thread_queue_t* queue2, thread_queue_item_t* item)
{
    lock_task_queue();
    thread_t* task = item->data;
    thread_queue_remove(queue1, item);
    thread_queue_push_back(queue2, task);
    unlock_task_queue();
}