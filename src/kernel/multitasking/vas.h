#pragma once

#include "../paging/paging.h"
#include "task.h"
#include "../memalloc/page_frame_allocator.h"

physical_address_t task_create_empty_vas(uint8_t privilege);
void task_free_vas(physical_address_t pml4_address);
void task_vas_copy(uint64_t* src, uint64_t* dst, 
    uint64_t start_virtual_address, 
    uint64_t pages);