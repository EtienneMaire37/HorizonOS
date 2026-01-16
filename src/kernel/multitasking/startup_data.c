#include "startup_data.h"
#include "../debug/out.h"

startup_data_struct_t startup_data_init_from_command(char** cmd, char** envp)
{
    LOG(DEBUG, "Creating program startup data...");
    startup_data_struct_t data;

    data.cmd_line = cmd;
    data.environ = envp;

    return data;
}