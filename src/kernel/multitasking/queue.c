#include "queue.h"
#include "multitasking.h"
#include "task.h"

#include <stdlib.h>

thread_queue_t dead_tasks = TQ_INIT;
thread_queue_t reapable_tasks = TQ_INIT;
thread_queue_t waitpid_tasks = TQ_INIT;
thread_queue_t forked_tasks = TQ_INIT;
thread_queue_t stopped_tasks = TQ_INIT;
thread_queue_t waiting_for_stdin_tasks = TQ_INIT;
thread_queue_t waiting_for_time_tasks = TQ_INIT;

void thread_queue_push_back(thread_queue_t* queue, thread_t* data)
{
    if (!queue || !data) return;
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

void copy_task_to_thread_queue(thread_queue_t* queue, thread_t* task)
{
    lock_scheduler();
    thread_queue_push_back(queue, task);
    unlock_scheduler();
}
void remove_task_from_thread_queue(thread_queue_t* queue, thread_t* task)
{
    lock_scheduler();
    thread_queue_remove(queue, ll_find_item_by_data(queue, task));
    unlock_scheduler();
}

void move_task_to_running_queue(thread_queue_t* queue, thread_t* item)
{
    lock_scheduler();
    thread_queue_item_t* it = ll_find_item_by_data(queue, item);
    assert(it);
    multitasking_add_task(item);
    thread_queue_remove(queue, it);
    item->queue = &running_tasks;
    if (current_task == idle_task)
        switch_task();
    unlock_scheduler();
}

void move_task_to_running_queue_by_item(thread_queue_t* queue, thread_queue_item_t* item)
{
    lock_scheduler();
    multitasking_add_task(item->data);
    thread_queue_remove(queue, item);
    ((thread_t*)item->data)->queue = &running_tasks;
    if (current_task == idle_task)
        switch_task();
    unlock_scheduler();
}

void move_task_from_to_thread_queue(thread_queue_t* queue1, thread_queue_t* queue2, thread_t* item)
{
    lock_scheduler();
    thread_queue_item_t* it = ll_find_item_by_data(queue1, item);
    assert(it);
    thread_queue_remove(queue1, it);
    thread_queue_push_back(queue2, item);
    item->queue = queue2;
    unlock_scheduler();
}
void move_task_from_to_thread_queue_by_item(thread_queue_t* queue1, thread_queue_t* queue2, thread_queue_item_t* item)
{
    lock_scheduler();
    thread_t* task = item->data;
    thread_queue_remove(queue1, item);
    thread_queue_push_back(queue2, task);
    task->queue = queue2;
    unlock_scheduler();
}

void move_task_to_queue(void* queue, thread_t* task)
{
    lock_scheduler();
    if (task->queue == queue)
    {
        unlock_scheduler();
        return;
    }
    assert(task->queue != &dead_tasks && task->queue != &reapable_tasks);
    if (task->queue == &running_tasks)
    {
        if (queue != &running_tasks)
            move_running_task_to_thread_queue(queue, task);
    }
    else
    {
        if (queue == &running_tasks)
            move_task_to_running_queue(task->queue, task);
        else
            move_task_from_to_thread_queue(task->queue, queue, task);
    }
    unlock_scheduler();
}

void move_all_tasks_to_running_queue(thread_queue_t* tq)
{
    assert(tq);
    lock_scheduler();
    if (!*tq) { unlock_scheduler(); return; }
    thread_queue_item_t* it = *tq;
    if (it)
    {
        do
        {
            thread_queue_item_t* cur = it;
            it = it->next;
            move_task_to_running_queue_by_item(tq, cur);
        } while (*tq);
    }
    unlock_scheduler();
}

void remove_all_tasks_from_queue(thread_queue_t* tq)
{
    assert(tq);
    lock_scheduler();
    if (!*tq) { unlock_scheduler(); return; }
    thread_queue_item_t* it = *tq;
    if (it)
    {
        do
        {
            thread_queue_item_t* cur = it;
            it = it->next;
            ll_remove(tq, it);
        } while (*tq);
    }
    unlock_scheduler();
}

void move_n_tasks_to_running_queue(thread_queue_t* tq, int n)
{
    assert(tq);
    lock_scheduler();
    if (!*tq) { unlock_scheduler(); return; }
    thread_queue_item_t* it = *tq;
    int i = 0;
    if (it)
    {
        do
        {
            thread_queue_item_t* cur = it;
            it = it->next;
            move_task_to_running_queue_by_item(tq, cur);
            i++;
        } while (*tq && i <= n);
    }
    unlock_scheduler();
}

void filter_tasks_to_running_queue(thread_queue_t* tq, bool (*test)(thread_t* task))
{
    assert(tq);
    lock_scheduler();
    if (!*tq) { unlock_scheduler(); return; }
    bool first_item = true;
    thread_queue_item_t* it = *tq;
    if (it)
    {
        do
        {
            thread_queue_item_t* cur = it;
            it = it->next;
            if (test(cur->data))
                move_task_to_running_queue_by_item(tq, cur);
            else
                first_item = false;
        } while (*tq && (it != *tq || first_item));
    }
    unlock_scheduler();
}
