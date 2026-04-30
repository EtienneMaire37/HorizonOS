#pragma once

#define _GNU_SOURCE

#include <dirent.h>
#include "vfs.h"
#include <assert.h>
#include <stdlib.h>

#include "../vfs/table.h"

static inline unsigned char get_dirent_dt(struct stat* st)
{
    assert(st);
    if (S_ISBLK(st->st_mode))
        return DT_BLK;
    if (S_ISCHR(st->st_mode))
        return DT_CHR;
    if (S_ISDIR(st->st_mode))
        return DT_DIR;
    if (S_ISDIR(st->st_mode))
        return DT_DIR;
    if (S_ISFIFO(st->st_mode))
        return DT_FIFO;
    if (S_ISLNK(st->st_mode))
        return DT_LNK;
    if (S_ISREG(st->st_mode))
        return DT_REG;
    if (S_ISSOCK(st->st_mode))
        return DT_SOCK;
    return DT_UNKNOWN;
}

static inline struct dirent64 vfs_find_new_child_entry(file_entry_t* entry)
{
    struct dirent64 dir_entry = {.d_ino = 0, .d_off = 0, .d_reclen = sizeof(struct dirent64), .d_type = 0, .d_name = {0}};

    assert(entry);
    assert(entry->entry_type == VFS_ET_FOLDER);
    vfs_folder_tnode_t* tnode = entry->tnode.folder;
    if (!(tnode->inode->flags & VFS_NODE_EXPLORED))
        vfs_explore(tnode);
    assert(tnode);
    vfs_folder_tnode_t* folder_current_child = tnode->inode->folders;
    vfs_file_tnode_t* file_current_child = tnode->inode->files;
    if (entry->file_data.folder_child.cur_index == 0)
    {
        entry->file_data.folder_child.cur_index++;
        dir_entry.d_ino = tnode->inode->st.st_ino;
        dir_entry.d_type = get_dirent_dt(&tnode->inode->st);
        strncpy(dir_entry.d_name, ".", sizeof(dir_entry.d_name));
        return dir_entry;
    }
    if (entry->file_data.folder_child.cur_index == 1)
    {
        entry->file_data.folder_child.cur_index++;
        dir_entry.d_ino = tnode->inode->parent->inode->st.st_ino;
        dir_entry.d_type = get_dirent_dt(&tnode->inode->parent->inode->st);
        strncpy(dir_entry.d_name, "..", sizeof(dir_entry.d_name));
        return dir_entry;
    }
    int cur_index = 2;
    for (; folder_current_child; cur_index++)
    {
        if (cur_index == entry->file_data.folder_child.cur_index)
        {
            entry->file_data.folder_child.cur_index++;
            goto do_dir_return;
        }
        folder_current_child = folder_current_child->next;
    }
    for (; file_current_child; cur_index++)
    {
        if (cur_index == entry->file_data.folder_child.cur_index)
        {
            entry->file_data.folder_child.cur_index++;
            goto do_file_return;
        }
        file_current_child = file_current_child->next;
    }
    entry->file_data.folder_child.cur_index = 0;

do_dir_return:
    dir_entry.d_ino = folder_current_child ? folder_current_child->inode->st.st_ino : -1;
    dir_entry.d_type = folder_current_child ? get_dirent_dt(&folder_current_child->inode->st) : DT_UNKNOWN;
    if (entry->file_data.folder_child.cur_index && folder_current_child)    strncpy(dir_entry.d_name, folder_current_child->name, sizeof(dir_entry.d_name));
    else                                                                    dir_entry.d_name[0] = 0;
    return dir_entry;

do_file_return:
    dir_entry.d_ino = file_current_child ? file_current_child->inode->st.st_ino : -1;
    dir_entry.d_type = file_current_child ? get_dirent_dt(&file_current_child->inode->st) : DT_UNKNOWN;
    if (entry->file_data.folder_child.cur_index && file_current_child)      strncpy(dir_entry.d_name, file_current_child->name, sizeof(dir_entry.d_name));
    else                                                                    dir_entry.d_name[0] = 0;
    return dir_entry;
}
