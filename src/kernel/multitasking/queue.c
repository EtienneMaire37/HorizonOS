#include "queue.h"
#include "multitasking.h"

thread_queue_t dead_tasks = TQ_INIT;
thread_queue_t reapable_tasks = TQ_INIT;
thread_queue_t waitpid_tasks = TQ_INIT;
thread_queue_t forked_tasks = TQ_INIT;

void thread_queue_push_back(thread_queue_t* queue, thread_t* data)
{
    // !! Should probably have a different lock per queue (or pair of queues)
    // TODO: Improve this
    lock_scheduler();

    ll_push_back(queue, data);

    unlock_scheduler();
}

void thread_queue_remove(thread_queue_t* queue, thread_queue_item_t* item)
{
    // !! Same here
    lock_scheduler();

    ll_remove(queue, item);

    unlock_scheduler();
}

void move_running_task_to_thread_queue(thread_queue_t* queue, thread_t* task)
{
    lock_scheduler();
    multitasking_remove_task(task);
    thread_queue_push_back(queue, task);
    task->queue = queue;
    unlock_scheduler();
}

void move_task_to_running_queue(thread_queue_t* queue, thread_queue_item_t* item)
{
    lock_scheduler();
    multitasking_add_task(item->data);
    thread_queue_remove(queue, item);
    ((thread_t*)item->data)->queue = running_tasks;
    unlock_scheduler();
}

void move_task_from_to_thread_queue(thread_queue_t* queue1, thread_queue_t* queue2, thread_queue_item_t* item)
{
    lock_scheduler();
    thread_t* task = item->data;
    thread_queue_remove(queue1, item);
    thread_queue_push_back(queue2, task);
    task->queue = queue2;
    unlock_scheduler();
}
