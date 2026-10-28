#include "queue.h"
#include "multitasking.h"
#include "task.h"

#include <stdlib.h>

thread_queue_t dead_tasks = TQ_INIT;
thread_queue_t reapable_tasks = TQ_INIT;
thread_queue_t waitpid_tasks = TQ_INIT;
thread_queue_t forked_tasks = TQ_INIT;
thread_queue_t stopped_tasks = TQ_INIT;
thread_queue_t _waiting_for_stdin_tasks = TQ_INIT;
thread_queue_t waiting_for_time_tasks = TQ_INIT;

void __thread_queue_push_back(thread_queue_t* queue, thread_t* data)
{
    ASSERT(queue && data);
    ll_push_back(queue, data);
}

void __thread_queue_remove(thread_queue_t* queue, thread_queue_item_t* item)
{
    ASSERT(queue);
    if (!item)
    {
        LOG(DEBUG, "__thread_queue_remove: item is NULL");
        return;
    }
    ll_remove(queue, item);
}

void __move_running_task_to_thread_queue(thread_queue_t* queue, thread_t* task)
{
    ASSERT(queue && task);
    __multitasking_remove_task(task);
    __thread_queue_push_back(queue, task);
    task->queue = queue;
}

void __copy_task_to_thread_queue(thread_queue_t* queue, thread_t* task)
{
    __thread_queue_push_back(queue, task);
}
void __remove_task_from_thread_queue(thread_queue_t* queue, thread_t* task)
{
    __thread_queue_remove(queue, ll_find_item_by_data(queue, task));
}

void __move_task_to_running_queue(thread_queue_t* queue, thread_t* item)
{
    ASSERT(queue && item);
    thread_queue_item_t* it = ll_find_item_by_data(queue, item);
    if (it)
    {
        __multitasking_add_task(item);
        __thread_queue_remove(queue, it);
        item->queue = &running_tasks;
    }
}

void __move_task_to_running_queue_by_item(thread_queue_t* queue, thread_queue_item_t* item)
{
    __multitasking_add_task(item->data);
    __thread_queue_remove(queue, item);
    ((thread_t*)item->data)->queue = &running_tasks;
    if (current_task == idle_task)
        switch_task();
}

void __move_task_from_to_thread_queue(thread_queue_t* queue1, thread_queue_t* queue2, thread_t* item)
{
    thread_queue_item_t* it = ll_find_item_by_data(queue1, item);
    ASSERT(it);
    __thread_queue_remove(queue1, it);
    __thread_queue_push_back(queue2, item);
    item->queue = queue2;
}
void __move_task_from_to_thread_queue_by_item(thread_queue_t* queue1, thread_queue_t* queue2, thread_queue_item_t* item)
{
    thread_t* task = item->data;
    __thread_queue_remove(queue1, item);
    __thread_queue_push_back(queue2, task);
    task->queue = queue2;
}

void __move_task_to_queue(void* queue, thread_t* task)
{
    if (task->queue == queue)
        return;
    ASSERT(task->queue != &dead_tasks && task->queue != &reapable_tasks);
    if (task->queue == &running_tasks)
    {
        if (queue != &running_tasks)
            __move_running_task_to_thread_queue(queue, task);
    }
    else
    {
        if (queue == &running_tasks)
            __move_task_to_running_queue(task->queue, task);
        else
            __move_task_from_to_thread_queue(task->queue, queue, task);
    }
}

void __move_all_tasks_to_running_queue(thread_queue_t* tq)
{
    ASSERT(tq);
    if (!*tq) return;
    thread_queue_item_t* it = *tq;
    if (it)
    {
        do
        {
            thread_queue_item_t* cur = it;
            it = it->next;
            __move_task_to_running_queue_by_item(tq, cur);
        } while (*tq);
    }
}

void __remove_all_tasks_from_queue(thread_queue_t* tq)
{
    ASSERT(tq);
    if (!*tq) return;
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
}

void __move_n_tasks_to_running_queue(thread_queue_t* tq, int n)
{
    ASSERT(tq);
    if (!*tq) return;
    thread_queue_item_t* it = *tq;
    int i = 0;
    if (it)
    {
        do
        {
            thread_queue_item_t* cur = it;
            it = it->next;
            __move_task_to_running_queue_by_item(tq, cur);
            i++;
        } while (*tq && i <= n);
    }
}

void __filter_tasks_to_running_queue(thread_queue_t* tq, bool (*test)(thread_t* task))
{
    ASSERT(tq);
    if (!*tq) return;
    bool first_item = true;
    thread_queue_item_t* it = *tq;
    if (it)
    {
        do
        {
            thread_queue_item_t* cur = it;
            it = it->next;
            if (test(cur->data))
                __move_task_to_running_queue_by_item(tq, cur);
            else
                first_item = false;
        } while (*tq && (it != *tq || first_item));
    }
}

void __run_it_on_queue(thread_queue_t* tq, void (*func)(thread_t* task))
{
    ASSERT(tq);
    if (!*tq) return;
    bool first_item = true;
    thread_queue_item_t* it = *tq;
    if (it)
    {
        do
        {
            thread_queue_item_t* cur = it;
            it = it->next;
            func(cur->data);
            if (it->prev == cur)
                first_item = false;
        } while (*tq && (it != *tq || first_item));
    }
}
