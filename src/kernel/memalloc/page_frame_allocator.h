#pragma once

#include "../../libc/include/stdint.h"
#include "../cpu/util.h"
#include "mmap.h"

#include <stdatomic.h>

#define MAX_MEMORY (1 * TB)

extern uint64_t usable_memory;

extern struct mem_block usable_memory_map[MAX_USABLE_MEMORY_BLOCKS];
extern uint8_t usable_memory_blocks;

extern uint8_t first_alloc_block;

extern uint64_t bitmap_size;
extern uint8_t* bitmap;

extern uint64_t first_free_page_index_hint;

extern uint64_t memory_allocated, allocatable_memory;

extern atomic_flag pfa_spinlock;

#ifdef LOG_MEMORY
#define LOG_MEM_ALLOCATED() { uint32_t percentage = 10000 * memory_allocated / allocatable_memory; LOG(TRACE, "Used memory : %llu / %llu bytes (%u.%u%u %%)", memory_allocated, allocatable_memory, percentage / 100, (percentage / 10) % 10, percentage % 10); }
#else
#define LOG_MEM_ALLOCATED()
#endif

void pfa_detect_usable_memory();
physical_address_t pfa_allocate_physical_page() ;
physical_address_t pfa_allocate_physical_contiguous_pages(uint32_t pages);
void pfa_free_physical_page(physical_address_t address);
void* pfa_allocate_page();
void pfa_free_page(const void* ptr);
void* pfa_allocate_contiguous_pages(uint32_t pages);
void pfa_free_contiguous_pages(const void* ptr, uint32_t pages);