#pragma once

#include <stddef.h>
#include "open_file.h"
#include "../multitasking/thread_def.h"
#include "open_file_descriptor_vector_def.h"

int ofdv_open(open_file_descriptor_vector_t* vec, open_file_descriptor_entry_t entry);
open_file_descriptor_entry_t ofdv_get(open_file_descriptor_vector_t* vec, int fd);
void ofdv_setflags(open_file_descriptor_vector_t* vec, int fd, int flags);
int ofdv_close(open_file_descriptor_vector_t* vec, int fd);
void ofdv_closeall(open_file_descriptor_vector_t* vec);
open_file_descriptor_vector_t ofdv_copyall(open_file_descriptor_vector_t* vec, bool cloexec);

int vfs_isatty(int fd);
int vfs_isatty_ofd(open_file_descriptor_t* ofd);
int vfs_dup(int fd);
int vfs_dup3(int oldfd, int newfd, int flags);

bool __vfs_willblock(open_file_descriptor_t* entry, short events);
void __task_monitor_entry(thread_t* task, open_file_descriptor_t* entry);
void __task_start_polling(thread_t* task, uint64_t timeout);
void __task_stop_polling(thread_t* task);
int __vfs_hup(open_file_descriptor_t* entry);

