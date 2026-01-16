#pragma once

typedef struct __attribute__((packed)) startup_data_struct
{
    char** cmd_line;
    char** environ;
} startup_data_struct_t;

startup_data_struct_t startup_data_init_from_command(char** cmd, char** envp);