#pragma once

#include <signal.h>
#include "../time/time.h"
#include "../util/linked_list.h"
#include "../vfs/open_file_descriptor_vector_def.h"

#define THREAD_NAME_MAX 64
#define NUM_SIGNALS     (SIGRTMAX + 1)

typedef struct thread thread_t;

// TODO: Make task use a clean reference counted system, have more scheduler lock granularity and rename these files to thread.*!
// XXX: DO THIS ASAP
// NOTE: This is probably the most important thing to change rn!
typedef struct thread
{
    struct sigaction sig_act_array[NUM_SIGNALS];
    open_file_descriptor_vector_t open_file_table;

    char name[THREAD_NAME_MAX];

    ll_t _poll_tqs;

    precise_time_t timeout_deadline;

    uint32_t return_value;

    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;

    mode_t umask;

    uint64_t pending_signal_handler;
    int pending_signal_number;

    sigset_t sig_pending, sig_mask;
    bool sig_pending_user_space;

    // * waitpid shenanigans
    pid_t wait_pid, waitpid_ret, pgid_on_waitpid;
    int waitpid_flags;
    uint32_t wstatus;

    pid_t forked_pid;

    void* queue;

    uint8_t ring;
    pid_t pid, ppid, pgid, sid;
    bool system_task;    // * Allow causing kernel panics (temporary hack, TODO: remove this)

    // TODO: Add a per process root
    vnode_ref_t cwd;
    uint8_t* fpu_state;

    uint16_t stored_cpu_ticks, current_cpu_ticks;   // * In milliseconds

    uint64_t rsp, cr3;
    uint64_t fs_base, gs_base;

    bool can_be_killed; // * Probably shouldn't kill tasks when in a critical section of the kernel
    bool waiting_for_kill;
    int waiting_for_kill_ret;

    // * We still have to keep them here in the case
    // * where the current process is blocked before context switching
    thread_t *prev, *next;
} thread_t;

extern const uint64_t task_rsp_offset;
extern const uint64_t task_cr3_offset;

extern void syscall_handler();

#define TASK_STACK_PAGES            0x400       // * 4MB
#define TASK_KERNEL_STACK_PAGES     0x20        // * 128KB

#define TASK_STACK_TOP_ADDRESS              0x800000000000
#define TASK_STACK_BOTTOM_ADDRESS           (TASK_STACK_TOP_ADDRESS - 0x1000 * TASK_STACK_PAGES)
#define TASK_KERNEL_STACK_TOP_ADDRESS       TASK_STACK_BOTTOM_ADDRESS
#define TASK_KERNEL_STACK_BOTTOM_ADDRESS    (TASK_KERNEL_STACK_TOP_ADDRESS - 0x1000 * TASK_KERNEL_STACK_PAGES)

#define TASK_SWITCH_DELAY 10 // ms
#define TASK_SWITCHES_PER_SECOND (1000 / TASK_SWITCH_DELAY)
