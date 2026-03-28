#pragma once

#include "vfs.h"
#include <assert.h>
#include <stdlib.h>
#include <dirent.h>

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

// !!!! HORRIBLE WAY TO DO THINGS
// TODO: Rewrite the whole vfs
static inline struct dirent64 vfs_find_new_child_entry(file_entry_t* entry)
{
    struct dirent64 dir_entry = {.d_ino = 0, .d_off = 0, .d_reclen = sizeof(struct dirent64), .d_type = 0, .d_name = {0}};

    assert(entry);
    assert(entry->entry_type == VFS_ET_FOLDER);
    const char* current_child_name = entry->file_data.folder_child.str;
    vfs_folder_tnode_t* tnode = entry->tnode.folder;
    if (!(tnode->inode->flags & VFS_NODE_EXPLORED))
        vfs_explore(tnode);
    assert(tnode);
    vfs_folder_tnode_t* current_child = tnode->inode->folders;
    vfs_file_tnode_t* file_current_child = tnode->inode->files;
    if (!entry->file_data.folder_child.str)
    {
        entry->file_data.folder_child.str = strdup(".");
        dir_entry.d_ino = tnode->inode->st.st_ino;
        dir_entry.d_type = get_dirent_dt(&tnode->inode->st);
        strncpy(dir_entry.d_name, entry->file_data.folder_child.str, sizeof(dir_entry.d_name));
        return dir_entry;
    }
    if (strcmp(entry->file_data.folder_child.str, ".") == 0)
    {
        free((void*)entry->file_data.folder_child.str);
        entry->file_data.folder_child.str = strdup("..");
        dir_entry.d_ino = tnode->inode->parent->inode->st.st_ino;
        dir_entry.d_type = get_dirent_dt(&tnode->inode->parent->inode->st);
        strncpy(dir_entry.d_name, entry->file_data.folder_child.str, sizeof(dir_entry.d_name));
        return dir_entry;
    }
    if (strcmp(entry->file_data.folder_child.str, "..") == 0)
    {
        free((void*)entry->file_data.folder_child.str);
        if (current_child)
            entry->file_data.folder_child.str = strdup(current_child->name);
        else
            entry->file_data.folder_child.str = file_current_child ? strdup(file_current_child->name) : NULL;

        goto do_dir_return;
    }
    const char* new_str = NULL;
    while (current_child)
    {
        if (strcmp(current_child->name, entry->file_data.folder_child.str) == 0)
        {
            if (!current_child->next)
                break;
            free((void*)entry->file_data.folder_child.str);
            entry->file_data.folder_child.str = strdup(current_child->next->name);
            current_child = current_child->next;
            goto do_dir_return;
        }
        current_child = current_child->next;
    }
    while (file_current_child)
    {
        if (strcmp(file_current_child->name, entry->file_data.folder_child.str) == 0)
        {
            free((void*)entry->file_data.folder_child.str);
            if (!file_current_child->next)
                break;
            entry->file_data.folder_child.str = strdup(file_current_child->next->name);
            file_current_child = file_current_child->next;
            goto do_file_return;
        }
        file_current_child = file_current_child->next;
    }
    free((void*)entry->file_data.folder_child.str);
    entry->file_data.folder_child.str = NULL;

do_dir_return:
    dir_entry.d_ino = current_child ? current_child->inode->st.st_ino : -1;
    dir_entry.d_type = current_child ? get_dirent_dt(&current_child->inode->st) : DT_UNKNOWN;
    if (entry->file_data.folder_child.str)  strncpy(dir_entry.d_name, entry->file_data.folder_child.str, sizeof(dir_entry.d_name));
    else                                    dir_entry.d_name[0] = 0;
    return dir_entry;

do_file_return:
    dir_entry.d_ino = file_current_child ? file_current_child->inode->st.st_ino : -1;
    dir_entry.d_type = file_current_child ? get_dirent_dt(&file_current_child->inode->st) : DT_UNKNOWN;
    if (entry->file_data.folder_child.str)  strncpy(dir_entry.d_name, entry->file_data.folder_child.str, sizeof(dir_entry.d_name));
    else                                    dir_entry.d_name[0] = 0;
    return dir_entry;
}
