#include "multitasking.h"
#include "idle.h"
#include "../cpu/memory.h"
#include "../cpu/units.h"
#include "signal.h"
#include "../fpu/fpu.h"

#include <stdlib.h>

hashmap_t* futex_tq_hashmap = NULL;
hashmap_t* pid_to_task_hashmap = NULL;
hashmap_t* pgid_to_tq_hashmap = NULL;
hashmap_t* pid_to_children_tq_hashmap = NULL;

mutex_t file_table_lock = MUTEX_INIT;
uint8_t global_cpu_ticks = 0;

thread_t* running_tasks = NULL;
uint16_t task_count = 0;

uint64_t multitasking_counter = TASK_SWITCH_DELAY;

thread_t* current_task = NULL;
bool multitasking_enabled = false;

pid_t task_reading_stdin = -1;
utf32_buffer_t keyboard_input_buffer;

int task_lock_depth = 0;
bool queued_ts = false;

void multitasking_init()
{
    task_count = 0;
    current_task = NULL;

    utf32_buffer_init(&keyboard_input_buffer);
    task_reading_stdin = -1;

    futex_tq_hashmap = hashmap_create(1 * MB);
    pid_to_task_hashmap = hashmap_create(1 * MB);
    pgid_to_tq_hashmap = hashmap_create(1 * MB);
    pid_to_children_tq_hashmap = hashmap_create(1 * MB);

    vfs_init_file_table();

    multitasking_add_idle_task("idle");
}

void multitasking_start()
{
    fflush(stdout);
    current_task = idle_task;
    multitasking_enabled = true;

    idle_main();
}

void multitasking_add_idle_task(char* name)
{
    if (task_count != 0)
    {
        LOG(CRITICAL, "The kernel task must be the first one");
        abort();
    }

    thread_t* task = task_create_empty();
    task_set_name(task, name);
    task->cr3 = get_cr3();

    multitasking_add_task(task);
    task_count++;
}

void multitasking_add_task(thread_t* task)
{
    lock_scheduler();
    
    if (!running_tasks)
    {
        task->prev = task->next = task;
        running_tasks = task;
    }
    else
    {
        task->prev = running_tasks->prev;
        task->next = running_tasks;
        running_tasks->prev->next = task;
        running_tasks->prev = task;
    }

    task->queue = &running_tasks;

    unlock_scheduler();
}

void multitasking_remove_task(thread_t* task)
{
    lock_scheduler();
    if (task == running_tasks)
    {
        LOG(DEBUG, "multitasking_remove_task: Tried to remove idle task");
        abort();
    }

    task->prev->next = task->next;
    task->next->prev = task->prev;
    unlock_scheduler();
}

void end_context_switch();

void full_context_switch(thread_t* next)
{
    thread_t* old_task = current_task;
    current_task = next;
    TSS.rsp0 = TASK_KERNEL_STACK_TOP_ADDRESS;

    if (old_task->ring != 0)
        swapgs();

    old_task->fs_base = rdfsbase();
    old_task->gs_base = rdgsbase();

    fpu_save_state(old_task->fpu_state);
    
    context_switch(old_task, current_task, current_task->ring == 0 ? KERNEL_DATA_SEGMENT : USER_DATA_SEGMENT);

    end_context_switch();
}

void end_context_switch()
{
    fpu_restore_state(current_task->fpu_state);

    wrfsbase(current_task->fs_base);
    wrgsbase(current_task->gs_base);

    if (current_task->ring != 0)
        swapgs();
}

bool task_is_blocked(thread_t* task)
{
    lock_scheduler();
    if (task_reading_stdin == task->pid) return (unlock_scheduler(), true);
    unlock_scheduler();
    return false;
}

thread_t* find_next_task()
{
    thread_t* start = current_task->next;
    thread_t* task;
    for (task = start; task != start->prev; task = task->next)
    {
        if (task == idle_task) continue;
        if (!task_is_blocked(task)) return task;
    }

    if (!task_is_blocked(task)) return task;
    return idle_task;
}

bool is_fd_valid(int fd)
{
    if (fd < 0 || fd >= OPEN_MAX)
        return false;
    if (current_task->file_table[fd].index == invalid_fd)
        return false;
    if (current_task->file_table[fd].index < 0 || current_task->file_table[fd].index >= MAX_FILE_TABLE_ENTRIES)
        return false;
    return true;
}

pid_t waitpid_find_child_in_tq(thread_queue_t* tq, pid_t pid, int* wstatus, int pgid_on_call)
{
    lock_scheduler();
    thread_queue_item_t* it = *tq;
    if (!it)
    {
        unlock_scheduler();
        return 0;
    }
    do
    {
        thread_t* thread = (thread_t*)it->data;
        if (pid > 0)
        {
            if (thread->pid == pid)
            {
                move_task_from_to_thread_queue(&dead_tasks, &reapable_tasks, it);
                if (wstatus) *wstatus = thread->return_value;
                unlock_scheduler();
                return thread->pid;
            }
        }
        else if (pid == 0)
        {
            if (thread->pgid == pgid_on_call)
            {
                move_task_from_to_thread_queue(&dead_tasks, &reapable_tasks, it);
                if (wstatus) *wstatus = thread->return_value;
                unlock_scheduler();
                return thread->pid;
            }
        }
        else if (pid == -1)
        {
            move_task_from_to_thread_queue(&dead_tasks, &reapable_tasks, it);
            if (wstatus) *wstatus = thread->return_value;
            unlock_scheduler();
            return thread->pid;
        }
        else // * pid < -1
        {
            if (thread->pgid == -pid)
            {
                move_task_from_to_thread_queue(&dead_tasks, &reapable_tasks, it);
                if (wstatus) *wstatus = thread->return_value;
                unlock_scheduler();
                return thread->pid;
            }
        }
        it = it->next;
    } while (it != *tq);
    unlock_scheduler();
    return 0;
}

static inline void task_stop(thread_t* thread, int sig)
{
    LOG(TRACE, "Stopping task \"%s\"", thread->name);

    thread_t* global_parent = find_task_by_pid_anywhere(thread->ppid);
    if (global_parent)
    {
        if (!(global_parent->sig_act_array[SIGCHLD].sa_flags & SA_NOCLDSTOP))
            task_send_signal(global_parent, SIGCHLD);
    }

    thread_t* parent = find_task_by_pid_in_queue(&waitpid_tasks, thread->ppid);

    if (parent && (parent->waitpid_flags & WUNTRACED))
    {
        parent->waitpid_ret = thread->pid;
        parent->wstatus = ((sig & 0xff) << 8) | 0x7f;
        move_task_to_running_queue(&waitpid_tasks, ll_find_item_by_data(&waitpid_tasks, parent));
    }

    move_task_to_queue(&stopped_tasks, thread);

    if (thread == current_task)
        switch_task();
}
static inline void task_continue(thread_t* thread)
{
    LOG(TRACE, "Resuming task \"%s\"", thread->name);
    if (thread->queue == &stopped_tasks)
        move_task_to_queue(&running_tasks, thread);

    thread_t* global_parent = find_task_by_pid_anywhere(thread->ppid);
    if (global_parent)
    {
        if (!(global_parent->sig_act_array[SIGCHLD].sa_flags & SA_NOCLDSTOP))
            task_send_signal(global_parent, SIGCHLD);
    }
    
    thread_t* parent = find_task_by_pid_in_queue(&waitpid_tasks, thread->ppid);
    if (parent && (parent->waitpid_flags & WCONTINUED))
    {
        parent->waitpid_ret = thread->pid;
        parent->wstatus = 0xffff;
        move_task_to_running_queue(&waitpid_tasks, ll_find_item_by_data(&waitpid_tasks, parent));
    }
}

void task_send_signal_to_pgrp(int sig, pid_t pgrp)
{
    LOG(DEBUG, "Sending signal %d to pgrp %d", sig, pgrp);
    lock_scheduler();
    thread_queue_t* tq = hashmap_get_item(pgid_to_tq_hashmap, pgrp);
    if (!tq || !*tq)
    {
        unlock_scheduler();
        return;
    }
    thread_queue_item_t* it = *tq;
    int i = 0;
    do
    {
        thread_t* cur = it->data;
        it = it->next;
        task_send_signal(cur, sig);
    } while (*tq && it != *tq);
    unlock_scheduler();
}
void task_send_signal(thread_t* thread, int sig)
{
    lock_scheduler();
    if (thread == idle_task)
    {
        unlock_scheduler();
        return;
    }
    if (sig < 0 || sig >= NUM_SIGNALS)
    {
        LOG(WARNING, "task_send_signal: Invalid signal number %d", sig);
        unlock_scheduler();
        return;
    }

    LOG(DEBUG, "pid %d receiving signal %d", thread->pid, sig);

    struct sigaction* act = &thread->sig_act_array[sig];
    
    if (sig == SIGCONT)
        task_continue(thread);
    
    if (act->sa_flags & SA_SIGINFO)
    {
        switch ((uint64_t)act->sa_sigaction)
        {
        case (uint64_t)SIG_DFL:
            goto dfl;
        case (uint64_t)SIG_IGN:
            goto ign;
        default:
            LOG(DEBUG, "act->sa_sigaction = %p", act->sa_sigaction);
            abort();
        }
    }
    else
    {
        switch ((uint64_t)act->sa_handler)
        {
        case (uint64_t)SIG_DFL:
            goto dfl;
        case (uint64_t)SIG_IGN:
            goto ign;
        default:
            LOG(DEBUG, "act->sa_handler = %p", act->sa_handler);
            abort();
        }
    }

dfl:
    int dfl_action = sig_default_action(sig);
    LOG(DEBUG, "Signal action defaulted to %s", 
        dfl_action == SIGDEF_IGN ? "\"ignore\"" :
       (dfl_action == SIGDEF_STOP ? "\"stop\"" :
       (dfl_action == SIGDEF_CONT ? "\"continue\"" :
       (dfl_action == SIGDEF_TERM ? "\"kill\"" :
       (dfl_action == SIGDEF_CORE ? "\"core dump\"" :
        "\"invalid action\"")))));
    switch (dfl_action)
    {
    case SIGDEF_IGN:
        break;
    case SIGDEF_STOP:
        task_stop(thread, sig);
        break;
    case SIGDEF_CONT:
        task_continue(thread);
        break;
    case SIGDEF_TERM:
    case SIGDEF_CORE:
    default:
        kill_task(thread, sig);
    }
    unlock_scheduler();
    return;

ign:
    LOG(DEBUG, "Signal was ignored by the process.");
    unlock_scheduler();
    return;
}