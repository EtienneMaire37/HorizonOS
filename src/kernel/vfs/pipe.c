#include "pipe.h"
#include "table.h"
#include "../multitasking/multitasking.h"
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include "../cpu/units.h"

bool vfs_isapipe(file_entry_t* entry)
{
    lock_scheduler();
    if (!entry) return (unlock_scheduler(), false);
    bool ret = entry->entry_type == VFS_ET_FILE ? S_ISFIFO(entry->st.st_mode) : false;
    unlock_scheduler();
    return ret;
}

ssize_t pipe_iofunc(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction)
{
    lock_scheduler();
    assert(task_lock_depth == 1);
    size_t in_buffer = ring_get_buffered_bytes(*entry->file_data.pipe_data.buffer);
    size_t maxwrite = entry->file_data.pipe_data.buffer->size - in_buffer - 1;
    if ((entry->flags & O_NONBLOCK) || (direction == IO_DIR_READ && entry->file_data.pipe_data.other_end == -1))
    {
        if (in_buffer == 0 && direction == IO_DIR_READ)
            return (unlock_scheduler(), 0);
        if (maxwrite == 0 && direction == IO_DIR_WRITE)
            return (unlock_scheduler(), 0);
    }
    switch (direction)
    {
    case IO_DIR_READ:
        if (entry->file_data.pipe_data.other_end != -1)
        {
            if (in_buffer <= 0)
            {
                if (entry->flags & O_NONBLOCK)
                    return (unlock_scheduler(), -EWOULDBLOCK);
                move_running_task_to_thread_queue(&entry->blocked_on_io, current_task);
                switch_task();
                unlock_scheduler();
                lock_scheduler();
                in_buffer = ring_get_buffered_bytes(*entry->file_data.pipe_data.buffer);
            }
            if (in_buffer <= 0)
                return (unlock_scheduler(), entry->file_data.pipe_data.other_end != -1 ? -EINTR : 0);
        }
        // LOG(TRACE, "Reading %zu bytes from pipe (%zu bytes in buffer) [%zu bytes asked by program]", in_buffer < count ? in_buffer : count, in_buffer, count);
        count = in_buffer < count ? in_buffer : count;
        if (count != 0)
        {
            for (size_t i = 0; i < count; i++)
                buf[i] = ((uint8_t*)entry->file_data.pipe_data.buffer->buffer)[(i + entry->file_data.pipe_data.buffer->get_index) % entry->file_data.pipe_data.buffer->size];
            entry->file_data.pipe_data.buffer->get_index = (entry->file_data.pipe_data.buffer->get_index + count) % entry->file_data.pipe_data.buffer->size;
        }
        if (entry->file_data.pipe_data.other_end != -1 && count > 0)
            move_n_tasks_to_running_queue(&file_table[entry->file_data.pipe_data.other_end].blocked_on_io, 1);
        unlock_scheduler();
        return count;
    case IO_DIR_WRITE:
        if (entry->file_data.pipe_data.other_end != -1)
        {
            if (maxwrite <= 0)
            {
                if (entry->flags & O_NONBLOCK)
                    return (unlock_scheduler(), -EWOULDBLOCK);
                move_running_task_to_thread_queue(&entry->blocked_on_io, current_task);
                switch_task();
                unlock_scheduler();
                lock_scheduler();
                maxwrite = entry->file_data.pipe_data.buffer->size - ring_get_buffered_bytes(*entry->file_data.pipe_data.buffer) - 1;
            }
            if (maxwrite <= 0)
                return (unlock_scheduler(), entry->file_data.pipe_data.other_end != -1 ? -EINTR : 0);
        }
        else
            return (unlock_scheduler(), -EPIPE);
        // LOG(TRACE, "Writing %zu bytes to pipe (%zu bytes in buffer) [%zu bytes asked by program]", maxwrite < count ? maxwrite : count, ring_get_buffered_bytes(*entry->file_data.pipe_data.buffer), count);
        count = maxwrite < count ? maxwrite : count;
        if (count != 0)
        {
            for (size_t i = 0; i < count; i++)
                ((uint8_t*)entry->file_data.pipe_data.buffer->buffer)[(i + entry->file_data.pipe_data.buffer->put_index) % entry->file_data.pipe_data.buffer->size] = buf[i];
            entry->file_data.pipe_data.buffer->put_index = (entry->file_data.pipe_data.buffer->put_index + count) % entry->file_data.pipe_data.buffer->size;
        }
        if (count > 0)
            move_n_tasks_to_running_queue(&file_table[entry->file_data.pipe_data.other_end].blocked_on_io, 1);
        unlock_scheduler();
        return count;
    default:
        unlock_scheduler();
        return 0;
    }
}

void pipe_destroy(file_entry_t* entry)
{
    assert(entry);
    lock_scheduler();
    if (entry->file_data.pipe_data.other_end == -1)
    {
        free(entry->file_data.pipe_data.buffer->buffer);
        free(entry->file_data.pipe_data.buffer);
        entry->file_data.pipe_data.buffer = NULL;
    }
    else
    {
        file_entry_t* other = &file_table[entry->file_data.pipe_data.other_end];
        other->file_data.pipe_data.other_end = -1;
        move_all_tasks_to_running_queue(&other->blocked_on_io);
    }
    unlock_scheduler();
}

void vfs_setup_pipe_end(int fildes, int other, int flags)
{
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_mode = S_IFIFO;

    assert(file_table[fildes].used == 1);
    file_table[fildes].entry_type = VFS_ET_FILE;

    if (file_table[other].file_data.pipe_data.other_end == -1)
    {
        file_table[fildes].file_data.pipe_data.buffer = malloc(sizeof(struct ring_buffer));
        assert(file_table[fildes].file_data.pipe_data.buffer);
        file_table[fildes].file_data.pipe_data.buffer->size = 64 * KB;
        file_table[fildes].file_data.pipe_data.buffer->buffer = malloc(file_table[fildes].file_data.pipe_data.buffer->size);
        assert(file_table[fildes].file_data.pipe_data.buffer->buffer);
        file_table[fildes].file_data.pipe_data.buffer->put_index = 0;
        file_table[fildes].file_data.pipe_data.buffer->get_index = 0;
    }
    else
        file_table[fildes].file_data.pipe_data.buffer = file_table[other].file_data.pipe_data.buffer;
    assert(file_table[fildes].file_data.pipe_data.buffer);
    file_table[fildes].file_data.pipe_data.other_end = other;

    file_table[fildes].tnode.file = NULL;
    file_table[fildes].tnode.folder = NULL;
    file_table[fildes].flags = flags;
    file_table[fildes].st = st;

    file_table[fildes].iofunc = pipe_iofunc;
    file_table[fildes].on_destroy = pipe_destroy;
}

int vfs_setup_pipe(int* fds, int flags)
{
    lock_scheduler();
    int fildes1 = vfs_allocate_global_file();
    if (fildes1 == -1)
        return (unlock_scheduler(), ENFILE);
    int fildes2 = vfs_allocate_global_file();
    if (fildes2 == -1)
    {
        vfs_remove_global_file(fildes1);
        unlock_scheduler();
        return ENFILE;
    }
    int fd1 = vfs_allocate_thread_file(current_task);
    if (fd1 == -1)
    {
        vfs_remove_global_file(fildes1);
        vfs_remove_global_file(fildes2);
        unlock_scheduler();
        return EMFILE;
    }
    current_task->file_table[fd1].index = fildes1;
    int fd2 = vfs_allocate_thread_file(current_task);
    current_task->file_table[fd2].index = fildes2;
    if (fd2 == -1)
    {
        vfs_close(fd1);
        vfs_remove_global_file(fildes1);
        vfs_remove_global_file(fildes2);
        unlock_scheduler();
        return EMFILE;
    }
    fds[0] = fd1;
    fds[1] = fd2;

    file_table[fildes1].file_data.pipe_data.other_end = -1;
    file_table[fildes2].file_data.pipe_data.other_end = -1;
    vfs_setup_pipe_end(fildes1, fildes2, O_RDONLY | flags);
    vfs_setup_pipe_end(fildes2, fildes1, O_WRONLY | flags);

    assert(file_table[fildes1].file_data.pipe_data.buffer);
    current_task->file_table[fd1].flags = 0;
    current_task->file_table[fd2].flags = 0;
    if (flags & O_CLOEXEC)
    {
        current_task->file_table[fd1].flags |= FD_CLOEXEC;
        current_task->file_table[fd2].flags |= FD_CLOEXEC;
    }
    // LOG(TRACE, "pipe: [%d, %d]", fd1, fd2);
    unlock_scheduler();
    return 0;
}
