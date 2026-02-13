#include "../debug/out.h"
#include "../cpu/units.h"
#include <string.h>
#include <stdlib.h>
#include "../paging/paging.h"
#include "mmap.h"
#include "page_frame_allocator.h"

uint64_t usable_memory = 0;
struct mem_block usable_memory_map[MAX_USABLE_MEMORY_BLOCKS];
uint8_t usable_memory_blocks;

uint8_t first_alloc_block;

uint64_t bitmap_size;
uint8_t* bitmap;

uint64_t first_free_page_index_hint = 0;

uint64_t memory_allocated, allocatable_memory;

mutex_t pfa_lock = MUTEX_INIT;

#include "page_frame_allocator.h"

#include <bootboot.h>

extern BOOTBOOT bootboot;

void pfa_detect_usable_memory() 
{
    usable_memory = usable_memory_blocks = 0;
    bitmap = NULL;
    memory_allocated = 0;
    allocatable_memory = 0;
    first_free_page_index_hint = 0;

    LOG(INFO, "Usable memory map:");

    for (MMapEnt* mmap_ent = &bootboot.mmap; (uintptr_t)mmap_ent < (uintptr_t)&bootboot + (uintptr_t)bootboot.size; mmap_ent++) 
    {
        LOG(DEBUG, "   BOOTBOOT memory block : address : %#" PRIx64 " ; length : %" PRIu64 " | type : %" PRIu64, 
            MMapEnt_Ptr(mmap_ent), MMapEnt_Size(mmap_ent), MMapEnt_Type(mmap_ent));
        // printf("   BOOTBOOT memory block : address : %#" PRIx64 " ; length : %" PRIu64 " | type : %u\n", 
        //     MMapEnt_Ptr(mmap_ent), MMapEnt_Size(mmap_ent), MMapEnt_Type(mmap_ent));

        if (!MMapEnt_IsFree(mmap_ent))
            continue;
        if (MMapEnt_Ptr(mmap_ent) == physical_null)
            continue;

        physical_address_t addr = MMapEnt_Ptr(mmap_ent);
        uint64_t len = MMapEnt_Size(mmap_ent);

        if (addr >= MAX_MEMORY)
            break;
        if (addr + len > MAX_MEMORY)
            len = MAX_MEMORY - addr;

        uint64_t align = addr & 0xfff;
        if (align != 0) 
        {
            uint64_t adjust = 0x1000 - align;
            addr += adjust;
            if (len < adjust)
                continue;
            len -= adjust;
        }

        len = (len / 0x1000) * 0x1000;
        if (len == 0)
            continue;

        if (usable_memory_blocks >= MAX_USABLE_MEMORY_BLOCKS) 
        {
            LOG(WARNING, "Too many memory blocks detected");
            break;
        }

        usable_memory_map[usable_memory_blocks].address = addr;
        usable_memory_map[usable_memory_blocks].length = (uint64_t)len;
        usable_memory += len;
        usable_memory_blocks++;

        LOG(INFO, "   Memory block : address : %#" PRIx64 " ; length : %" PRIu64, addr, len);
    }

    first_alloc_block = 0;
    uint64_t total_pages = 0;
    for (uint64_t i = 0; i < usable_memory_blocks; i++)
        total_pages += usable_memory_map[i].length / 0x1000;
    
    bitmap_size = ((total_pages + 7) / 8 + 7) & ~7ULL; // * Align to qwords
    uint64_t bitmap_pages = (bitmap_size + 0xfff) / 0x1000;

    while (first_alloc_block < usable_memory_blocks && (usable_memory_map[first_alloc_block].length / 0x1000) < bitmap_pages)
        first_alloc_block++;

    bitmap = (uint8_t*)usable_memory_map[first_alloc_block].address;

    printf("pfa: bitmap address: %#" PRIx64 "\n", (uint64_t)bitmap);

    if (usable_memory == 0 || first_alloc_block >= usable_memory_blocks) 
        goto no_memory;

    usable_memory_map[first_alloc_block].length -= 0x1000 * bitmap_pages;
    usable_memory_map[first_alloc_block].address += 0x1000 * bitmap_pages;
    if (usable_memory_map[first_alloc_block].length == 0)
        first_alloc_block++;

    if (first_alloc_block >= usable_memory_blocks)
        goto no_memory;

    memset(bitmap, 0, bitmap_size);

    for (uint8_t i = first_alloc_block; i < usable_memory_blocks; i++)
        allocatable_memory += usable_memory_map[i].length;

    LOG(INFO, "Detected %" PRIu64 " bytes of allocatable memory", allocatable_memory);
    return;

no_memory:
    LOG(CRITICAL, "No usable memory detected");
    abort();
}

physical_address_t pfa_allocate_physical_page() 
{
    if (memory_allocated + 0x1000 > allocatable_memory) 
    {
        LOG(CRITICAL, "pfa_allocate_physical_page: Out of memory at start!");
        return physical_null;
    }

    if (memory_allocated + 0x1000 > allocatable_memory * 9 / 10) 
        LOG(WARNING, "pfa_allocate_physical_page: Over 90%% of memory used!");

    acquire_mutex(&pfa_lock);

    for (uint64_t i = first_free_page_index_hint / 8; i < bitmap_size; i += 8) 
    {
        uint64_t* qword = (uint64_t*)&bitmap[i];
        if (*qword == 0xffffffffffffffff) continue;

        uint8_t bit = __builtin_ffsll(~(*qword)) - 1;

        *qword |= ((uint64_t)1 << bit);

        uint64_t page_index = 64 / 8 * i + bit;

        uint64_t remaining = page_index;
        for (uint32_t j = first_alloc_block; j < usable_memory_blocks; j++) 
        {
            uint64_t block_pages = usable_memory_map[j].length / 0x1000;
            if (remaining < block_pages) 
            {
                physical_address_t addr = usable_memory_map[j].address + remaining * 0x1000;
                first_free_page_index_hint = 8 * i + bit;
                memory_allocated += 0x1000;
                LOG_MEM_ALLOCATED();
                release_mutex(&pfa_lock);
                // LOG(TRACE, "Allocated page: %#" PRIx64, addr);
                return addr;
            }
            remaining -= block_pages;
        }
    }

    LOG(CRITICAL, "pfa_allocate_physical_page: Out of memory at end!");
    release_mutex(&pfa_lock);
    return physical_null;
}

physical_address_t pfa_allocate_physical_contiguous_pages(uint32_t pages)
{
    if (memory_allocated + 0x1000 * pages > allocatable_memory) 
    {
        LOG(CRITICAL, "pfa_allocate_physical_contiguous_pages: Out of memory at start!");
        return physical_null;
    }

    if (memory_allocated + 0x1000 * pages > allocatable_memory * 9 / 10) 
        LOG(WARNING, "pfa_allocate_physical_contiguous_pages: Over 90%% of memory used!");

    acquire_mutex(&pfa_lock);

    for (uint64_t i = first_free_page_index_hint / 8; i < bitmap_size; i += 8) 
    {
    loop_start:
        uint64_t* qword = (uint64_t*)&bitmap[i];
        if (*qword == 0xffffffffffffffff) continue;

        uint8_t bit = __builtin_ffsll(~(*qword)) - 1;
        uint32_t offset = 0;

        for (uint64_t j = 0; j < pages; j++)
        {
            if (bit >= 64)
            {
                bit = 0;
                offset++;
                qword = (uint64_t*)&bitmap[i + offset];
            }
            if ((*qword) & (1ULL << bit))
            {
                i += j * 8 / 64 + 8;
                goto loop_start;
            }
            bit++;
        }

        qword = (uint64_t*)&bitmap[i];

        bit = __builtin_ffsll(~(*qword)) - 1;

        uint64_t page_index = i * 8 + bit;
        offset = 0;

        for (uint64_t j = 0; j < pages; j++)
        {
            if (bit >= 64)
            {
                bit = 0;
                offset++;
                qword = (uint64_t*)&bitmap[i + offset];
            }
            *qword |= ((uint64_t)1 << bit);
            bit++;
        }

        uint64_t remaining = page_index;
        for (uint32_t j = first_alloc_block; j < usable_memory_blocks; j++) 
        {
            uint64_t block_pages = usable_memory_map[j].length / 0x1000;
            if (remaining <= block_pages - pages) 
            {
                physical_address_t addr = usable_memory_map[j].address + remaining * 0x1000;
                first_free_page_index_hint = 64 / 8 * i + bit;
                memory_allocated += 0x1000 * pages;
                LOG_MEM_ALLOCATED();
                release_mutex(&pfa_lock);
                // LOG(TRACE, "Allocated page: %#" PRIx64, addr);
                return addr;
            }
            remaining -= block_pages;
        }
    }

    LOG(CRITICAL, "pfa_allocate_physical_contiguous_pages: Out of memory at end!");
    release_mutex(&pfa_lock);
    return physical_null;
}

void pfa_free_physical_page(physical_address_t address) 
{
    if (address == physical_null) 
    {
        LOG(WARNING, "pfa_free_physical_page: Kernel tried to free NULL");
        return;
    }

    if (address & 0xfff) 
    {
        LOG(CRITICAL, "pfa_free_physical_page: Unaligned address (%#" PRIx64 ")", address);
        abort();
    }

    uint64_t page_index = 0;
    for (uint32_t i = first_alloc_block; i < usable_memory_blocks; i++) 
    {
        if (address >= usable_memory_map[i].address && 
            address < usable_memory_map[i].address + usable_memory_map[i].length) 
        {
            page_index += (address - usable_memory_map[i].address) / 0x1000;
            break;
        }
        page_index += usable_memory_map[i].length / 0x1000;
    }

    uint64_t byte = page_index / 8;
    uint8_t bit = page_index & 0b111;
    if (byte >= bitmap_size) return;
    acquire_mutex(&pfa_lock);
    bitmap[byte] &= ~(1 << bit);
    if (page_index < first_free_page_index_hint)
        first_free_page_index_hint = page_index;

    memory_allocated -= 0x1000;
    release_mutex(&pfa_lock);
    // LOG(TRACE, "Freed page: %#" PRIx64, address);
    LOG_MEM_ALLOCATED();
}

void pfa_free_physical_contiguous_pages(physical_address_t address, uint32_t pages)
{
    for (uint32_t i = 0; i < pages; i++)
        pfa_free_physical_page(address + 0x1000ULL * i);
}

void* pfa_allocate_page()
{
    physical_address_t paddr = pfa_allocate_physical_page();
    if (!paddr) return NULL;
    return (void*)(paddr + PHYS_MAP_OFFSET);
}

void pfa_free_page(const void* ptr)
{
    if (!ptr) return;
    pfa_free_physical_page((physical_address_t)ptr - PHYS_MAP_OFFSET);
}

void* pfa_allocate_contiguous_pages(uint32_t pages)
{
    physical_address_t paddr = pfa_allocate_physical_contiguous_pages(pages);
    if (!paddr) return NULL;
    return (void*)(paddr + PHYS_MAP_OFFSET);
}

void pfa_free_contiguous_pages(const void* ptr, uint32_t pages)
{
    if (!ptr) return;
    pfa_free_physical_contiguous_pages((physical_address_t)ptr - PHYS_MAP_OFFSET, pages);
}