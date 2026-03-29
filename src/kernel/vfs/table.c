#include "table.h"
#include "pipe.h"
#include <fcntl.h>
#include <time.h>
#include <sys/poll.h>

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
        }

		unlock_scheduler();
        return;
    }

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
            return ring_no_buffered_bytes(*entry->file_data.pipe_data.buffer);
        }
        break;
    case POLLOUT:
        assert(!"TODO: Implement write polling");
        break;
    default:
        break;
    }
    return false;
}
void vfs_block(file_entry_t* entry, precise_time_t timeout, short events)
{
    assert(events == POLLIN || events == POLLOUT);
    switch (events)
    {
    case POLLIN:
        if (vfs_isatty(entry))
        {
            if (no_buffered_characters(keyboard_buffered_input_buffer))
            {
                current_task->timeout_deadline = timeout != PRECISE_TIME_MAX ? global_timer + timeout : PRECISE_TIME_MAX;
                move_running_task_to_thread_queue(&waiting_for_stdin_tasks, current_task);
                switch_task();
                unlock_scheduler();
                lock_scheduler();
            }
        }
        else if (vfs_isapipe(entry))
        {
            if (entry->flags & O_NONBLOCK)
                return;
            if ((entry->flags & O_ACCMODE) != O_RDONLY)
                return;
            current_task->timeout_deadline = timeout != PRECISE_TIME_MAX ? global_timer + timeout : PRECISE_TIME_MAX;
            move_running_task_to_thread_queue(&entry->blocked_on_io, current_task);
            switch_task();
            unlock_scheduler();
            lock_scheduler();
        }
        break;
    case POLLOUT:
        assert(!"TODO: Implement write blocking");
        break;
    default:
        break;
    }
}
int vfs_hup(file_entry_t* entry)
{
    return (vfs_isapipe(entry) && entry->file_data.pipe_data.other_end == -1) ? POLLHUP : 0;
}
