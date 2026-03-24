#pragma once

#include "../util/ring.h"
#include <stdbool.h>

typedef struct file_entry file_entry_t;

struct pipe_data
{
    struct ring_buffer* buffer;

    int other_end;
};

int vfs_setup_pipe(int* ds, int flags);
bool vfs_isapipe(file_entry_t* entry);
