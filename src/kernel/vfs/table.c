#include "table.h"
#include "pipe.h"
#include <fcntl.h>
#include <time.h>
#include <sys/poll.h>
#include "../util/lambda.h"
#include <asm-generic/errno.h>
#include "../util/error.h"

file_entry_t file_table[MAX_FILE_TABLE_ENTRIES];

void vfs_init_file_table()
{
    lock_scheduler();

    file_table[0] = file_entry_create_empty();
    file_table[0].used = 1;
    file_table[0].entry_type = VFS_ET_FILE;
    file_table[0].tnode.file = vfs_get_file_tnode("/dev/tty", NULL);
    file_table[0].position = 0;
    file_table[0].flags = O_RDONLY;
    file_table[0].iofunc = task_chr_tty;
    file_table[0].st = file_table[0].tnode.file->inode->st;

    file_table[1] = file_entry_create_empty();
    file_table[1].used = 1;
    file_table[1].entry_type = VFS_ET_FILE;
    file_table[1].tnode.file = vfs_get_file_tnode("/dev/tty", NULL);
    file_table[1].position = 0;
    file_table[1].flags = O_WRONLY;
    file_table[1].iofunc = task_chr_tty;
    file_table[1].st = file_table[1].tnode.file->inode->st;

    file_table[2] = file_entry_create_empty();
    file_table[2].used = 1;
    file_table[2].entry_type = VFS_ET_FILE;
    file_table[2].tnode.file = vfs_get_file_tnode("/dev/tty", NULL);
    file_table[2].position = 0;
    file_table[2].flags = O_WRONLY;
    file_table[2].iofunc = task_chr_tty;
    file_table[2].st = file_table[2].tnode.file->inode->st;

    unlock_scheduler();
}

int vfs_allocate_global_file()
{
    // !!! ASAP
    // TODO: Implement a more general "atomic lock" which can be used for several things and not just the scheduler
    // ! Will need to be implemented to reduce lock contention when adding SMP
	lock_scheduler();
    for (int i = 0; i < MAX_FILE_TABLE_ENTRIES; i++)
    {
        if (file_table[i].used == 0)
        {
            file_table[i] = file_entry_create_empty();
            file_table[i].used = 1;
			unlock_scheduler();
            return i;
        }
    }
	unlock_scheduler();
    return -1;
}

void vfs_remove_global_file(int fd)
{
	lock_scheduler();

    if (fd >= 0 && fd < MAX_FILE_TABLE_ENTRIES)
    {
        file_table[fd].used--;
        assert(file_table[fd].used >= 0);
        if (file_table[fd].used == 0)
        {
            if (file_table[fd].on_destroy)
                file_table[fd].on_destroy(&file_table[fd]);
            move_all_tasks_to_running_queue(&file_table[fd].blocked_on_io);
            run_it_on_queue(&file_table[fd].blocked_on_poll, lambda(void, (thread_t* thread)
            {
                ll_remove(&thread->_poll_tqs, ll_find_item_by_data(&thread->_poll_tqs, &file_table[fd].blocked_on_poll));
                task_stop_polling(thread);
            }));
            remove_all_tasks_from_queue(&file_table[fd].blocked_on_poll);
        }

		unlock_scheduler();
        return;
    }

	unlock_scheduler();
}

void task_monitor_entry(thread_t* task, file_entry_t* entry)
{
    assert(entry && task);
    if (vfs_isatty(entry))
        copy_task_to_thread_queue(&_waiting_for_stdin_tasks, task);
    else
    {
        copy_task_to_thread_queue(&entry->blocked_on_poll, task);
        ll_push_back(&task->_poll_tqs, &entry->blocked_on_poll);
    }
}

void task_start_polling(thread_t* task, precise_time_t timeout)
{
    current_task->timeout_deadline = timeout == NO_TIMEOUT ? NO_TIMEOUT : global_timer + timeout;
    move_task_to_queue(&waiting_for_time_tasks, task);
}

void task_stop_polling(thread_t* task)
{
    lock_scheduler();
    ll_item_t* it = task->_poll_tqs;
    if (it)
    {
        do
        {
            ll_item_t* cur = it;
            thread_queue_t* tq = cur->data;
            it = it->next;
            thread_queue_remove(tq, ll_find_item_by_data(tq, task));
            ll_remove(&task->_poll_tqs, cur);
        } while (task->_poll_tqs && it != task->_poll_tqs);
    }
    thread_queue_remove(&_waiting_for_stdin_tasks, ll_find_item_by_data(&_waiting_for_stdin_tasks, task));
    if (task->queue == &waiting_for_time_tasks)
        move_task_to_queue(&running_tasks, task);
    // else
    //     assert(task->queue == &dead_tasks || task->queue == &reapable_tasks);
    unlock_scheduler();
}

bool vfs_willblock(file_entry_t* entry, short events)
{
    assert(events == POLLIN || events == POLLOUT);
    switch (events)
    {
    case POLLIN:
        if (vfs_isatty(entry))
        {
            if (no_buffered_characters(keyboard_buffered_input_buffer))
                return true;
        }
        else if (vfs_isapipe(entry))
        {
            if (entry->flags & O_NONBLOCK)
                return false;
            if ((entry->flags & O_ACCMODE) != O_RDONLY)
                return false;
            if (entry->file_data.pipe_data.other_end == -1)
                return false;
            return ring_no_buffered_bytes(*entry->file_data.pipe_data.buffer);
        }
        break;
    case POLLOUT:
        FATAL("TODO: Implement write polling");
        break;
    default:
        break;
    }
    return false;
}
int vfs_hup(file_entry_t* entry)
{
    return (vfs_isapipe(entry) && entry->file_data.pipe_data.other_end == -1) ? POLLHUP : 0;
}

int vfs_dup(int fd)
{
    lock_scheduler();
    if (!is_fd_valid(fd))
    {
        unlock_scheduler();
        return -EBADF;
    }
    int newfd = vfs_allocate_thread_file(current_task);
    if (newfd == -1)
    {
        unlock_scheduler();
        return -EMFILE;
    }
    current_task->file_table[newfd].index = current_task->file_table[fd].index;
    current_task->file_table[newfd].flags = 0;
    file_table[current_task->file_table[newfd].index].used++;
    unlock_scheduler();
    return newfd;
}
