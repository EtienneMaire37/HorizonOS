#pragma once

#include "../multitasking/multitasking.h"
#include "../multitasking/queue.h"

#include <fcntl.h>

#define MAX_FILE_TABLE_ENTRIES  1024

typedef struct file_entry
{
    int used;
    int flags;
    off_t position;
    uint8_t entry_type;

    struct stat st;

    union
    {
        struct folder_child_data folder_child;
        struct pipe_data pipe_data;
    } file_data;
    union
    {
        vfs_file_tnode_t* file;
        vfs_folder_tnode_t* folder;
    } tnode;

    thread_queue_t blocked_on_io;
    thread_queue_t blocked_on_poll;

    ssize_t (*iofunc)(file_entry_t*, uint8_t* buf, size_t count, uint8_t direction);
    void (*on_destroy)(file_entry_t*);
} file_entry_t;

static inline file_entry_t file_entry_create_empty()
{
    file_entry_t ent;
    memset(&ent, 0, sizeof(ent));
    ent.blocked_on_io = TQ_INIT;
    ent.blocked_on_poll = TQ_INIT;
    return ent;
}

extern file_entry_t file_table[MAX_FILE_TABLE_ENTRIES];

static inline file_entry_t* get_global_file_entry(int fd)
{
    lock_scheduler();
    if (!is_fd_valid(fd))
        return (unlock_scheduler(), NULL);
    file_entry_t* entry = &file_table[current_task->file_table[fd].index];
    unlock_scheduler();
    return entry;
}

bool vfs_willblock(file_entry_t* entry, short events);
int vfs_hup(file_entry_t* entry);

void task_monitor_entry(thread_t* task, file_entry_t* entry);
void task_start_polling(thread_t* task, precise_time_t timeout);
void task_stop_polling(thread_t* task);
