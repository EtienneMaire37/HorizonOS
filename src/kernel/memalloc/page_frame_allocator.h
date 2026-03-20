#pragma once

#include <stdint.h>
#include "../cpu/util.h"
#include "mmap.h"
#include "../multitasking/mutex.h"

#include <stdatomic.h>

#define MAX_MEMORY (1 * TB)

extern uint64_t usable_memory;

extern struct mem_block usable_memory_map[MAX_USABLE_MEMORY_BLOCKS];
extern uint8_t usable_memory_blocks;

extern uint64_t memory_allocated, allocatable_memory;

#define DO_LOG_MEM_ALLOCATED()      do { uint32_t percentage = 10000 * memory_allocated / allocatable_memory; LOG(TRACE, "Used memory : %" PRIu64 " / %" PRIu64 " bytes (%u.%u%u %%)", memory_allocated, allocatable_memory, percentage / 100, (percentage / 10) % 10, percentage % 10); } while (0)

#ifdef LOG_MEMORY
#define LOG_MEM_ALLOCATED() DO_LOG_MEM_ALLOCATED()
#else
#define LOG_MEM_ALLOCATED()
#endif

void pfa_detect_usable_memory();
physical_address_t pfa_allocate_physical_page();
physical_address_t pfa_allocate_physical_contiguous_pages(size_t pages);
void pfa_free_physical_page(physical_address_t address);
void* pfa_allocate_page();
void pfa_free_page(const void* ptr);
void* pfa_allocate_contiguous_pages(size_t pages);
void pfa_free_contiguous_pages(const void* ptr, size_t pages);
