#pragma once

#include "../vfs/vfs.h"

bool vfs_initrd_file_in_directory(char* fname, const char* direc) 
{
    if (!fname || !direc) return false;

    // LOG(DEBUG, "\"%s\" | \"%s\"", fname, direc);

    if (*direc == 0) 
    {
        for (char* f = fname; *f; f++) 
            if (*f == '/')
                return false;
        return true;
    }

    const char* f = fname;
    const char* d = direc;

    while (*d && *f && (*f == *d)) 
    {
        f++;
        d++;
    }

    if (*d) 
        return false;

    if (*f != '/') 
        return false;

    f++;

    while (*f) 
    {
        if (*f == '/') return false;
        f++;
    }

    return true;
}

void vfs_initrd_do_explore(vfs_folder_tnode_t* tnode)
{
    if (tnode->inode->drive.type != DT_INITRD) 
    {
        LOG(ERROR, "vfs_initrd_do_explore: not an initrd mounted folder!!!");
        return;
    }
    char contructed_path[PATH_MAX];
    vfs_realpath_from_folder_tnode(tnode, contructed_path);
    // LOG(DEBUG, "Exploring \"%s\"", contructed_path);
    tnode->inode->files = NULL;
    tnode->inode->folders = NULL;
    char* path = strcmp(contructed_path, "/initrd") == 0 ? "" : &contructed_path[strlen("/initrd/")];
    vfs_file_tnode_t** current_file_tnode = &tnode->inode->files;
    vfs_folder_tnode_t** current_folder_tnode = &tnode->inode->folders;
    for (int i = 0; i < initrd_files_count; i++)
    {
        if (vfs_initrd_file_in_directory(initrd_files[i].name, path))
        {
            if (initrd_files[i].type != USTAR_TYPE_FILE_1 && initrd_files[i].type != USTAR_TYPE_DIRECTORY) continue;
            char* name = initrd_files[i].name;
            for (ssize_t j = 0; name[j] != 0; j++)
            {
                if (name[j] == '/' && name[j + 1] != 0)
                {
                    name = &name[j + 1];
                    j = -1;
                    continue;
                }
            }
            switch (initrd_files[i].type)
            {
            case USTAR_TYPE_FILE_1:
                *current_file_tnode = malloc(sizeof(vfs_file_tnode_t));

                if (!*current_file_tnode)
                    continue;

                (*current_file_tnode)->name = malloc(strlen(name) + 1);
                memcpy((*current_file_tnode)->name, name, strlen(name) + 1);

                (*current_file_tnode)->inode = malloc(sizeof(vfs_file_inode_t));
                (*current_file_tnode)->inode->io_func = initrd_iofunc;
                (*current_file_tnode)->inode->file_data.initrd = &initrd_files[i];
                (*current_file_tnode)->inode->drive.type = DT_INITRD;
                (*current_file_tnode)->inode->st = initrd_files[i].st;
                (*current_file_tnode)->inode->st.st_ino = vfs_generate_inode_number();
                (*current_file_tnode)->inode->st.st_dev = tnode->inode->st.st_dev;
                (*current_file_tnode)->inode->st.st_rdev = 0; // S_ISCHR(initrd_files[i].st.st_mode) || S_ISBLK(initrd_files[i].st.st_mode) ? vfs_generate_device_id() : 0;
                
                (*current_file_tnode)->next = NULL;
                (*current_file_tnode)->inode->parent = tnode;
                current_file_tnode = &(*current_file_tnode)->next;
                break;

            case USTAR_TYPE_DIRECTORY:
                *current_folder_tnode = malloc(sizeof(vfs_folder_tnode_t));

                if (!*current_folder_tnode)
                    continue;

                (*current_folder_tnode)->name = malloc(strlen(name) + 1);
                memcpy((*current_folder_tnode)->name, name, strlen(name) + 1);

                (*current_folder_tnode)->inode = malloc(sizeof(vfs_folder_inode_t));
                (*current_folder_tnode)->inode->drive.type = DT_INITRD;
                (*current_folder_tnode)->inode->st = initrd_files[i].st;
                (*current_folder_tnode)->inode->st.st_ino = vfs_generate_inode_number();
                (*current_folder_tnode)->inode->st.st_dev = tnode->inode->st.st_dev;
                (*current_folder_tnode)->inode->st.st_rdev = S_ISCHR((*current_folder_tnode)->inode->st.st_mode) || S_ISBLK((*current_folder_tnode)->inode->st.st_mode) ? vfs_generate_device_id() : 0;

                (*current_folder_tnode)->inode->flags = VFS_NODE_INIT;

                (*current_folder_tnode)->next = NULL;
                (*current_folder_tnode)->inode->parent = tnode;
                current_folder_tnode = &(*current_folder_tnode)->next;
                break;

            default:
            }
        }
    }
}

ssize_t initrd_iofunc(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction)
{
    initrd_file_t* file = entry->tnode.file->inode->file_data.initrd;
    if (entry->position + count > file->size)
        count = file->size - entry->position;

    memcpy(buf, &file->data[entry->position], count);

    return count;
}