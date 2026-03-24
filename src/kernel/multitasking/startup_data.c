#include "startup_data.h"
#include "../debug/out.h"
#include "multitasking.h"

startup_data_struct_t startup_data_init_from_command(char** cmd, char** envp)
{
    LOG(DEBUG, "Creating program startup data...");
    startup_data_struct_t data;

    data.cmd = cmd;
    data.environ = envp;
    data.argc = 0;
    data.envc = 0;
    if (cmd)
    {
        while (cmd[data.argc])
            data.argc++;
    }

    if (envp)
    {
        while (envp[data.envc])
            data.envc++;
    }

    LOG(DEBUG, "argc: %" PRId64 ", envc: %" PRId64, data.argc, data.envc);

    return data;
}
