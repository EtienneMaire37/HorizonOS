#pragma once

bool vfs_initrd_file_in_directory(char* fname, const char* direc);
void vfs_initrd_do_explore(vfs_folder_tnode_t* tnode);
ssize_t initrd_iofunc(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction);