#pragma once

#include "../../libc/include/stdint.h"

typedef struct __attribute__((packed)) startup_data_struct
{
    int64_t argc, envc;
    char** cmd;
    char** environ;
} startup_data_struct_t;

startup_data_struct_t startup_data_init_from_command(char** cmd, char** envp);