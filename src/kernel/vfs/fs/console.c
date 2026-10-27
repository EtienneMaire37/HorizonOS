#include "console.h"
#include "../../multitasking/multitasking.h"
#include "../../multitasking/queue.h"
#include "../../terminal/textio.h"
#include <unistd.h>
#include <errno.h>

ssize_t console_read(inode_ref_t node, void* buf, size_t count, off_t off)
{
    (void)node;
    (void)off;
    if (count == 0)
        return 0;
    if (current_task->pgid != tty_foreground_pgrp)
    {
        task_send_signal(current_task, SIGTTIN);
        return 0;
    }
    if (no_buffered_characters(keyboard_buffered_input_buffer))
    {
        current_task->timeout_deadline = PRECISE_TIME_MAX;
        uint32_t flags = lock_scheduler();
        __copy_task_to_thread_queue(&_waiting_for_stdin_tasks, current_task);
        __move_running_task_to_thread_queue(&waiting_for_time_tasks, current_task);
        switch_task();
        unlock_scheduler(flags);
    }
    if (no_buffered_characters(keyboard_buffered_input_buffer))
        return -EINTR;
    uint64_t ret = minint(get_buffered_characters(keyboard_buffered_input_buffer), count);
    for (uint32_t i = 0; i < ret; i++)
        // *** Only ASCII for now ***
        ((uint8_t*)buf)[i] = utf32_to_bios_oem(utf32_buffer_getchar(&keyboard_buffered_input_buffer));
    return ret;
}
ssize_t console_write(inode_ref_t node, void* buf, size_t count, off_t off)
{
    (void)node;
    (void)off;
    return write(STDOUT_FILENO, buf, count);
}
