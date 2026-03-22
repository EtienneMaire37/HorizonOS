#include "table.h"

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
        }

		unlock_scheduler();
        return;
    }

	unlock_scheduler();
}
