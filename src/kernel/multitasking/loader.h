#pragma once

#include <stdbool.h>

#include <stdint.h>
#include "startup_data.h"

#include "../vfs/vnode_ref.h"
#include "thread_def.h"

thread_t* multitasking_add_task_from_function(const char* name, void (*func)());
thread_t* __multitasking_add_task_from_vfs(const char* name, const char* path, uint8_t ring, bool system, const startup_data_struct_t* data, vnode_ref_t cwd);
