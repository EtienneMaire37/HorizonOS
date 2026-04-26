#include "multitasking.h"
#include "idle.h"
#include "../cpu/memory.h"
#include "../cpu/units.h"
#include "signal.h"
#include "../fpu/fpu.h"
#include "sigset.h"
#include "../vfs/table.h"
#include "task.h"

#include <stdlib.h>

hashmap_t* futex_tq_hashmap = NULL;
hashmap_t* pid_to_task_hashmap = NULL;
hashmap_t* pgid_to_tq_hashmap = NULL;
hashmap_t* pid_to_children_tq_hashmap = NULL;

uint8_t global_cpu_ticks = 0;

thread_t* running_tasks = NULL;
uint16_t task_count = 0;

uint64_t multitasking_counter = TASK_SWITCH_DELAY;

thread_t* current_task = NULL;
thread_t* last_task = NULL;
bool multitasking_enabled = false;

utf32_buffer_t keyboard_input_buffer, keyboard_buffered_input_buffer;

int task_lock_depth = 0;
int saved_tld = 0;

uint64_t scheduler_lock_rflags;
bool queued_ts = false;

void multitasking_init()
{
    task_count = 0;
    current_task = NULL;

    utf32_buffer_init(&keyboard_input_buffer);
    utf32_buffer_init(&keyboard_buffered_input_buffer);

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
    last_task = idle_task;
    multitasking_enabled = true;

    switch_task();

    idle_main();
}

void multitasking_add_idle_task(char* name)
{
    assert(task_count == 0);

    thread_t* task = task_create_empty();
    task_set_name(task, name);
    task->cr3 = get_cr3_address();

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

    if (current_task == idle_task)
        switch_task();

    unlock_scheduler();
}

void multitasking_remove_task(thread_t* task)
{
    lock_scheduler();
    if (task == idle_task)
        assert(!"multitasking_remove_task: Tried to remove idle task");

    task->prev->next = task->next;
    task->next->prev = task->prev;
    unlock_scheduler();
}

void end_context_switch();

void full_context_switch(thread_t* next)
{
    // log_context(next);

    thread_t* old_task = current_task;
    last_task = old_task;
    current_task = next;

    swapgs();

    old_task->fs_base = rdfsbase();
    old_task->gs_base = rdgsbase();

    fpu_save_state(old_task->fpu_state);

    saved_tld = task_lock_depth;
    task_lock_depth = 0;

    context_switch(old_task, current_task, (current_task->ring == 0) ? KERNEL_DATA_SEGMENT : USER_DATA_SEGMENT);

    end_context_switch();
}

void end_context_switch()
{
    task_lock_depth = saved_tld;

    fpu_restore_state(current_task->fpu_state);

    wrfsbase(current_task->fs_base);
    wrgsbase(current_task->gs_base);

    swapgs();

    // LOG(DEBUG, "Saved context of the last task:");
    // log_context(last_task);
}

void unlock_scheduler_symbol()
{
    unlock_scheduler();
}

thread_t* find_next_task()
{
    thread_t* start = current_task->next;
    thread_t* task;
    for (task = start; task != start->prev; task = task->next)
    {
        if (task == idle_task) continue;
        return task;
    }

    return task;
}

bool is_fd_valid(int fd)
{
    lock_scheduler();
    if (fd < 0 || fd >= OPEN_MAX)
        return (unlock_scheduler(), false);
    if (current_task->file_table[fd].index == invalid_fd)
        return (unlock_scheduler(), false);
    if (current_task->file_table[fd].index < 0 || current_task->file_table[fd].index >= MAX_FILE_TABLE_ENTRIES)
        return (unlock_scheduler(), false);
    if (file_table[current_task->file_table[fd].index].used <= 0)
        return (unlock_scheduler(), false);
    unlock_scheduler();
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
                move_task_from_to_thread_queue_by_item(&dead_tasks, &reapable_tasks, it);
                if (wstatus) *wstatus = thread->return_value;
                unlock_scheduler();
                return thread->pid;
            }
        }
        else if (pid == 0)
        {
            if (thread->pgid == pgid_on_call)
            {
                move_task_from_to_thread_queue_by_item(&dead_tasks, &reapable_tasks, it);
                if (wstatus) *wstatus = thread->return_value;
                unlock_scheduler();
                return thread->pid;
            }
        }
        else if (pid == -1)
        {
            move_task_from_to_thread_queue_by_item(&dead_tasks, &reapable_tasks, it);
            if (wstatus) *wstatus = thread->return_value;
            unlock_scheduler();
            return thread->pid;
        }
        else // * pid < -1
        {
            if (thread->pgid == -pid)
            {
                move_task_from_to_thread_queue_by_item(&dead_tasks, &reapable_tasks, it);
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

void task_stop(thread_t* thread, int sig)
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
        move_task_to_running_queue(&waitpid_tasks, parent);
    }

    move_task_to_queue(&stopped_tasks, thread);

    if (thread == current_task)
        switch_task();
}
void task_continue(thread_t* thread)
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
        move_task_to_running_queue(&waitpid_tasks, parent);
    }
}

void task_send_signal_to_pgrp(int sig, pid_t pgrp)
{
    LOG(TRACE, "Sending signal %d to pgrp %d", sig, pgrp);
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
    LOG(TRACE, "Sending signal %d to pid %d", sig, thread->pid);
    lock_scheduler();
    task_set_pending_signal(thread, sig);
    if (!sigset_is_bit_set(thread->sig_mask, sig))
        task_handle_signal(thread, sig);
    unlock_scheduler();
}

void task_handle_sig_dfl(thread_t* task, int sig)
{
    int dfl_action = sig_default_action(sig);
    SC_LOG("Signal action defaulted to %s",
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
        task_stop(task, sig);
        break;
    case SIGDEF_CONT:
        task_continue(task);
        break;
    case SIGDEF_TERM:
    case SIGDEF_CORE:
    default:
        kill_task(task, sig);
    }
}

void task_handle_signal(thread_t* thread, int sig)
{
    if (sig < 0 || sig >= NUM_SIGNALS)
    {
        LOG(WARNING, "task_send_signal: Invalid signal number %d", sig);
        return;
    }

    assert(!(sig >= SIGRTMIN && sig <= SIGRTMAX));

    if (thread == idle_task)
        return;

    lock_scheduler();

    LOG(TRACE, "pid %d receives signal %d", thread->pid, sig);

    task_unset_pending_signal(thread, sig);

    struct sigaction* act = &thread->sig_act_array[sig];

    if (sig == SIGCONT)
        task_continue(thread);

    if (thread->_poll_tqs)
        task_stop_polling(thread);

    switch (act->sa_flags & SA_SIGINFO ? (uint64_t)act->sa_sigaction : (uint64_t)act->sa_handler)
    {
    case (uint64_t)SIG_DFL:
        goto dfl;
    case (uint64_t)SIG_IGN:
        goto ign;
    default:
        goto signal;
    }

dfl:
    if (thread->lock_depth)
    {
        thread->pending_signal_handler = 0;
        thread->sig_pending_user_space = true;
        thread->pending_signal_number = sig;
        move_task_to_queue(&running_tasks, thread);
    }
    else
        task_handle_sig_dfl(thread, sig);
    unlock_scheduler();
    return;

ign:
    // LOG(TRACE, "Signal was ignored by the process.");
    unlock_scheduler();
    return;

signal:
    // LOG(TRACE, "Signal was sent to the process.");
    thread->pending_signal_handler = (act->sa_flags & SA_SIGINFO) ? (uint64_t)act->sa_sigaction : (uint64_t)act->sa_handler;
    thread->sig_pending_user_space = true;
    thread->pending_signal_number = sig;
    move_task_to_queue(&running_tasks, thread);
    unlock_scheduler();
    return;
}

void task_try_handle_signals(thread_t* thread, sigset_t old, sigset_t new)
{
    for (int i = 0; i < sizeof(old.__sig) / sizeof(unsigned long); i++)
    {
        unsigned long were_unset = old.__sig[i] ^ ~new.__sig[i];
        while (were_unset)
        {
            unsigned long bit = were_unset & -were_unset;  // ? find lowest set bit
            int sig = __builtin_ctzll(were_unset) + i * sizeof(unsigned long) * 8;
            if (sigset_is_bit_set(thread->sig_pending, sig))
                task_handle_signal(thread, sig);
            were_unset ^= bit;
        }
    }
}

void vfs_close(int fd)
{
    lock_scheduler();
    if (is_fd_valid(fd))
    {
        vfs_remove_global_file(current_task->file_table[fd].index);
        current_task->file_table[fd].index = invalid_fd;
    }
    unlock_scheduler();
}
