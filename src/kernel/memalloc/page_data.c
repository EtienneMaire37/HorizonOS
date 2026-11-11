#include "page_data.h"
#include "../debug/out.h"
#include "page_frame_allocator.h"
#include "../paging/paging.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

page_table_data_t* global_page_table = NULL;
size_t npages = 0;

void page_data_table_init()
{
    ASSERT(!global_page_table);
    LOG(TRACE, "Max allocatable address: %#" PRIx64, max_allocatable_address);
    npages = ((sizeof(page_table_data_t) * max_allocatable_address) + 0xfff) / 0x1000; // * How many pages we need to keep track of
    npages = (npages + 0xfff) / 0x1000; // * Hom many pages we need to keep track of these
    global_page_table = pfa_allocate_contiguous_pages(npages);
    memset(global_page_table, 0, npages * 4096ULL);
    LOG(DEBUG, "global_page_table = %p [%zu pages]", global_page_table, npages);
    ASSERT(global_page_table);
}
uint32_t lock_page_table(uint64_t* addr)
{
    if (global_page_table && (uint64_t)addr >= PHYS_MAP_BASE && (uint64_t)addr - PHYS_MAP_BASE <= 4096ULL * npages)
        return acquire_spinlock_noint(&global_page_table[((uint64_t)addr - PHYS_MAP_BASE) / 4096].lock);
    return get_rflags();
}
void unlock_page_table(uint64_t* addr, uint32_t eflags)
{
    if (global_page_table && (uint64_t)addr >= PHYS_MAP_BASE && (uint64_t)addr - PHYS_MAP_BASE <= 4096ULL * npages)
        release_spinlock_noint(&global_page_table[((uint64_t)addr - PHYS_MAP_BASE) / 4096].lock, eflags);
    else
        set_rflags_if(eflags);
}
