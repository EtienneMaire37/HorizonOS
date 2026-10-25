#include "multitasking.h"
#include "idle.h"
#include "../util/units.h"
#include "queue.h"
#include "signal.h"
#include "../fpu/fpu.h"
#include "sigset.h"
#include "task.h"
#include "sched_lock.h"
#include "loader.h"
#include "../cpu/segbase.h"
#include "syscall.h"
#include "../vfs/open_file_descriptor_vector.h"

hashmap_t* pid_to_task_hashmap = NULL;
hashmap_t* pgid_to_tq_hashmap = NULL;
hashmap_t* pid_to_children_tq_hashmap = NULL;

uint8_t global_cpu_ticks = 0;

thread_t* running_tasks = NULL;
_Atomic uint16_t task_count = 0;

uint64_t multitasking_counter = TASK_SWITCH_DELAY;

thread_t* current_task = NULL;
bool multitasking_enabled = false;

utf32_buffer_t keyboard_input_buffer, keyboard_buffered_input_buffer;
spinlock_noint_t keyboard_input_lock = SPINLOCK_NOINT_INIT;

// * Per CPU
bool queued_ts = false;

void multitasking_init()
{
    task_count = 0;
    current_task = NULL;

    utf32_buffer_init(&keyboard_input_buffer);
    utf32_buffer_init(&keyboard_buffered_input_buffer);

    pid_to_task_hashmap = hashmap_create(1 * MB);
    pgid_to_tq_hashmap = hashmap_create(1 * MB);
    pid_to_children_tq_hashmap = hashmap_create(1 * MB);

    multitasking_add_idle_task("idle");
}

void multitasking_start()
{
    fflush(stdout);
    current_task = NULL;
    multitasking_enabled = true;

    switch_task();

    idle_main();
}

void multitasking_add_idle_task(char* name)
{
    ASSERT(task_count == 0);

    multitasking_add_task_from_function(name, idle_main);
}

void __multitasking_add_task(thread_t* task)
{
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
}

void __multitasking_remove_task(thread_t* task)
{
    if (task == idle_task)
        FATAL("multitasking_remove_task: Tried to remove idle task");

    task->prev->next = task->next;
    task->next->prev = task->prev;
}

void end_context_switch()
{
    fpu_restore_state(current_task->fpu_state);

    wrfsbase(current_task->fs_base);
    wrgsbase(current_task->gs_base);

    // * Now IA32_GS_BASE_MSR contains the value it had before switching tasks
    swapgs();
}

thread_t* __find_next_task()
{
    if (current_task == NULL) return idle_task->next;
    thread_t* start = current_task->next;
    thread_t* task;
    for (task = start; task != start->prev; task = task->next)
    {
        if (task == idle_task) continue;
        return task;
    }

    return task;
}

pid_t __waitpid_find_child_in_tq(thread_queue_t* tq, pid_t pid, int* wstatus, int pgid_on_call)
{
    thread_queue_item_t* it = *tq;
    if (!it)
        return 0;
    do
    {
        thread_t* thread = (thread_t*)it->data;
        if (pid > 0)
        {
            if (thread->pid == pid)
            {
                __move_task_from_to_thread_queue_by_item(&dead_tasks, &reapable_tasks, it);
                if (wstatus) *wstatus = thread->return_value;
                return thread->pid;
            }
        }
        else if (pid == 0)
        {
            if (thread->pgid == pgid_on_call)
            {
                __move_task_from_to_thread_queue_by_item(&dead_tasks, &reapable_tasks, it);
                if (wstatus) *wstatus = thread->return_value;
                return thread->pid;
            }
        }
        else if (pid == -1)
        {
            __move_task_from_to_thread_queue_by_item(&dead_tasks, &reapable_tasks, it);
            if (wstatus) *wstatus = thread->return_value;
            return thread->pid;
        }
        else // * pid < -1
        {
            if (thread->pgid == -pid)
            {
                __move_task_from_to_thread_queue_by_item(&dead_tasks, &reapable_tasks, it);
                if (wstatus) *wstatus = thread->return_value;
                return thread->pid;
            }
        }
        it = it->next;
    } while (it != *tq);
    return 0;
}

void __task_stop(thread_t* thread, int sig)
{
    LOG(TRACE, "Stopping task \"%s\"", thread->name);

    thread_t* global_parent = __find_task_by_pid_anywhere(thread->ppid);
    if (global_parent)
    {
        if (!(global_parent->sig_act_array[SIGCHLD].sa_flags & SA_NOCLDSTOP))
            __task_send_signal(global_parent, SIGCHLD);
    }

    thread_t* parent = __find_task_by_pid_in_queue(&waitpid_tasks, thread->ppid);

    if (parent && (parent->waitpid_flags & WUNTRACED))
    {
        parent->waitpid_ret = thread->pid;
        parent->wstatus = ((sig & 0xff) << 8) | 0x7f;
        __move_task_to_running_queue(&waitpid_tasks, parent);
    }

    __move_task_to_queue(&stopped_tasks, thread);

    if (thread == current_task)
        switch_task();
}
void __task_continue(thread_t* thread)
{
    LOG(TRACE, "Resuming task \"%s\"", thread->name);
    if (thread->queue == &stopped_tasks)
        __move_task_to_queue(&running_tasks, thread);

    thread_t* global_parent = __find_task_by_pid_anywhere(thread->ppid);
    if (global_parent)
    {
        if (!(global_parent->sig_act_array[SIGCHLD].sa_flags & SA_NOCLDSTOP))
            __task_send_signal(global_parent, SIGCHLD);
    }

    thread_t* parent = __find_task_by_pid_in_queue(&waitpid_tasks, thread->ppid);
    if (parent && (parent->waitpid_flags & WCONTINUED))
    {
        parent->waitpid_ret = thread->pid;
        parent->wstatus = 0xffff;
        __move_task_to_running_queue(&waitpid_tasks, parent);
    }
}

void task_send_signal_to_pgrp(int sig, pid_t pgrp)
{
    uint32_t flags = lock_scheduler();
    __task_send_signal_to_pgrp(sig, pgrp);
    unlock_scheduler(flags);
}

void __task_send_signal_to_pgrp(int sig, pid_t pgrp)
{
    LOG(TRACE, "Sending signal %d to pgrp %d", sig, pgrp);
    thread_queue_t* tq = hashmap_get_item(pgid_to_tq_hashmap, pgrp);
    if (!tq || !*tq)
        return;
    thread_queue_item_t* it = *tq;
    int i = 0;
    do
    {
        thread_t* cur = it->data;
        it = it->next;
        __task_send_signal(cur, sig);
    } while (*tq && it != *tq);
}

void task_send_signal(thread_t* thread, int sig)
{
    uint32_t flags = lock_scheduler();
    __task_send_signal(thread, sig);
    unlock_scheduler(flags);
}

void __task_send_signal(thread_t* thread, int sig)
{
    LOG(TRACE, "Sending signal %d to pid %d", sig, thread->pid);
    __task_set_pending_signal(thread, sig);
    if (!sigset_is_bit_set(thread->sig_mask, sig))
        __task_handle_signal(thread, sig);
}

void __task_handle_sig_dfl(thread_t* task, int sig)
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
        __task_stop(task, sig);
        break;
    case SIGDEF_CONT:
        __task_continue(task);
        break;
    case SIGDEF_TERM:
    case SIGDEF_CORE:
    default:
        __kill_task(task, sig);
    }
}

void __task_handle_signal(thread_t* thread, int sig)
{
    if (sig < 0 || sig >= NUM_SIGNALS)
    {
        LOG(WARNING, "task_send_signal: Invalid signal number %d", sig);
        return;
    }

    ASSERT(!(sig >= SIGRTMIN && sig <= SIGRTMAX));

    if (thread == idle_task)
        return;

    if (thread->queue == &dead_tasks || thread->queue == &reapable_tasks)
        return;

    LOG(TRACE, "pid %d receives signal %d", thread->pid, sig);

    __task_unset_pending_signal(thread, sig);

    struct sigaction* act = &thread->sig_act_array[sig];

    if (sig == SIGCONT)
        __task_continue(thread);

    if (thread->_poll_tqs)
        __task_stop_polling(thread);

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
    __task_handle_sig_dfl(thread, sig);
    return;

ign:
    // LOG(TRACE, "Signal was ignored by the process.");
    return;

signal:
    // LOG(TRACE, "Signal was sent to the process.");
    thread->pending_signal_handler = (act->sa_flags & SA_SIGINFO) ? (uint64_t)act->sa_sigaction : (uint64_t)act->sa_handler;
    thread->sig_pending_user_space = true;
    thread->pending_signal_number = sig;
    __move_task_to_queue(&running_tasks, thread);
}

void __task_try_handle_signals(thread_t* thread, sigset_t old, sigset_t new)
{
    for (size_t i = 0; i < sizeof(old.__sig) / sizeof(old.__sig[0]); i++)
    {
        unsigned long were_unset = old.__sig[i] ^ ~new.__sig[i];
        while (were_unset)
        {
            unsigned long bit = were_unset & -were_unset;  // ? find lowest set bit
            size_t sig = __builtin_ctzll(were_unset) + i * sizeof(unsigned long) * 8;
            if (sigset_is_bit_set(thread->sig_pending, sig))
                __task_handle_signal(thread, sig);
            were_unset ^= bit;
        }
    }
}
