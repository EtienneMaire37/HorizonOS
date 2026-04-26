#include "../debug/out.h"
#include "../cpu/units.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "../paging/paging.h"
#include "mmap.h"
#include "page_frame_allocator.h"
#include "../boot/limine.h"
#include "../int/kernel_panic.h"

uint64_t usable_memory = 0;
struct mem_block usable_memory_map[MAX_USABLE_MEMORY_BLOCKS];
uint8_t usable_memory_blocks = 0;

uint8_t first_alloc_block;

uint64_t bitmap_size;
uint8_t* bitmap;

uint64_t first_free_page_index_hint = 0;

uint64_t memory_allocated, allocatable_memory;

void pfa_detect_usable_memory()
{
    usable_memory = usable_memory_blocks = 0;
    bitmap = NULL;
    memory_allocated = 0;
    allocatable_memory = 0;
    first_free_page_index_hint = 0;

    LOG(INFO, "Usable memory map:");

    for (int i = 0; i < mmap_request.response->entry_count; i++)
    {
        struct limine_memmap_entry* entry = mmap_request.response->entries[i];
        LOG(DEBUG, "   Limine memory block : address : %#" PRIx64 " ; length : %" PRIu64 " | type : %" PRIu64,
            entry->base, entry->length, entry->type);
        if (entry->type != LIMINE_MEMMAP_USABLE)
            continue;
        if (entry->base & 0xfff)
            continue;
        if (entry->length & 0xfff)
            continue;
        assert(usable_memory_blocks < MAX_USABLE_MEMORY_BLOCKS);

        physical_address_t addr = entry->base;
        uint64_t len = entry->length;

        if (!addr) // * Skip NULL
        {
            addr += 0x1000;
            len -= 0x1000;
            if (len == 0)
                continue;
        }

        if (addr >= MAX_MEMORY)
            break;
        if (addr > MAX_MEMORY - len)
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
        usable_memory_map[usable_memory_blocks].length = len;
        usable_memory += len;
        usable_memory_blocks++;

        LOG(INFO, "   Memory block : address : %#" PRIx64 " ; length : %" PRIu64, addr, len);
        // printf("Memory block : address : %#" PRIx64 " ; length : %" PRIu64 "\n", addr, len);
    }

    first_alloc_block = 0;
    uint64_t total_pages = 0;
    for (uint64_t i = 0; i < usable_memory_blocks; i++)
        total_pages += usable_memory_map[i].length / 0x1000;

    bitmap_size = total_pages / 8; // * Align to qwords (and round down)
    uint64_t bitmap_pages = bitmap_size / 0x1000;

    LOG(TRACE, "Bitmap size: %" PRIu64, bitmap_size);
    printf("PFA: Bitmap size: %" PRIu64 "\n", bitmap_size);

    while (first_alloc_block < usable_memory_blocks && usable_memory_map[first_alloc_block].length / 0x1000 <= bitmap_pages)
        first_alloc_block++;

    assert(first_alloc_block < usable_memory_blocks);

    bitmap = (uint8_t*)(usable_memory_map[first_alloc_block].address + PHYS_MAP_BASE);

    LOG(TRACE, "PFA: bitmap address: %p", bitmap);
    printf("PFA: bitmap address: %p\n", bitmap);

    if (first_alloc_block >= usable_memory_blocks)
    goto no_memory;

    if (usable_memory_map[first_alloc_block].length <= 0x1000 * bitmap_pages)
        first_alloc_block++;
    else
    {
        usable_memory_map[first_alloc_block].length -= 0x1000 * bitmap_pages;
        usable_memory_map[first_alloc_block].address += 0x1000 * bitmap_pages;
    }

    if (first_alloc_block >= usable_memory_blocks)
        goto no_memory;
    LOG(TRACE, "PFA: First alloc address: %#" PRIx64, usable_memory_map[first_alloc_block].address + PHYS_MAP_BASE);

    memset(bitmap, 0, bitmap_size);

    for (uint8_t i = first_alloc_block; i < usable_memory_blocks; i++)
        allocatable_memory += usable_memory_map[i].length;

    LOG(INFO, "Detected %" PRIu64 " bytes of allocatable memory", allocatable_memory);
    return;

no_memory:
    LOG(CRITICAL, "Not enough usable memory detected");
    assert(!"Not enough usable memory detected");
}

physical_address_t pfa_allocate_physical_page()
{
    return pfa_allocate_physical_contiguous_pages(1);
}

physical_address_t pfa_allocate_physical_contiguous_pages(size_t pages)
{
//     LOG(TRACE, "pfa_allocate_physical_contiguous_pages(%zu)", pages);

    if (memory_allocated + 0x1000 * pages > allocatable_memory)
    {
        LOG(CRITICAL, "pfa_allocate_physical_contiguous_pages: Out of memory at start!");
        kernel_panic_ex(NULL, PANIC_OUT_OF_MEMORY);
        abort();
    }

    #ifdef DEBUG_ALLOCATOR
    {
        lock_scheduler();
        static int page_index = 0;
        uint64_t remaining = page_index;
        for (uint8_t j = first_alloc_block; j < usable_memory_blocks; j++)
        {
            uint64_t block_pages = usable_memory_map[j].length / 0x1000;
            if (remaining + pages <= block_pages)
            {
                physical_address_t addr = usable_memory_map[j].address + remaining * 0x1000;
                first_free_page_index_hint = page_index + pages;
                memory_allocated += 0x1000 * pages;
                LOG_MEM_ALLOCATED();
                unlock_scheduler();
                // LOG(TRACE, "Allocated page: %#" PRIx64, addr);
                page_index++;
                return addr;
            }
            remaining -= block_pages;
        }
        abort();
    }
    #endif

//     if (memory_allocated + 0x1000 * pages > allocatable_memory * 9 / 10)
//         LOG(WARNING, "pfa_allocate_physical_contiguous_pages: Over 90%% of memory used!");

    lock_scheduler();

    for (uint64_t qword_index = first_free_page_index_hint / 64; qword_index < bitmap_size / 8; qword_index++)
    {
    loop_start:
        uint64_t* qword = (uint64_t*)&bitmap[8 * qword_index];
        if (*qword == 0xffffffffffffffff) continue;

        const uint8_t first_bit_index = __builtin_ffsll(~(*qword)) - 1;
        uint8_t bit = first_bit_index;
        size_t offset = 0;

        for (uint64_t j = 0; j < pages; j++)
        {
            if (bit >= 64)
            {
                bit = 0;
                offset++;
                if (8 * (qword_index + offset) >= bitmap_size)
                {
                    qword_index = 0;
                    goto loop_start;
                }
                qword = (uint64_t*)&bitmap[8 * (qword_index + offset)];
            }
            if ((*qword) & (1ULL << bit))
            {
                qword_index += offset + 1;
                goto loop_start;
            }
            bit++;
        }

        qword = (uint64_t*)&bitmap[8 * qword_index];
        bit = first_bit_index;

        uint64_t page_index = qword_index * 64 + bit;
        offset = 0;

        uint64_t remaining = page_index;
        for (uint8_t j = first_alloc_block; j < usable_memory_blocks; j++)
        {
            uint64_t block_pages = usable_memory_map[j].length / 0x1000;
            if (remaining + pages <= block_pages)
            {
                for (size_t k = 0; k < pages; k++)
                {
                    if (bit >= 64)
                    {
                        bit = 0;
                        offset++;
                        if (8 * (qword_index + offset) >= bitmap_size) goto loop_start;
                        qword = (uint64_t*)&bitmap[8 * (qword_index + offset)];
                    }
                    *qword |= (1ULL << bit);
                    bit++;
                }

                physical_address_t addr = usable_memory_map[j].address + remaining * 0x1000;
                first_free_page_index_hint = page_index + pages;
                memory_allocated += 0x1000 * pages;
                LOG_MEM_ALLOCATED();
                unlock_scheduler();
                // LOG(TRACE, "Allocated page: %#" PRIx64, addr);
                return addr;
            }
            remaining -= block_pages;
        }

        qword_index += offset + 1;
    }

    LOG(CRITICAL, "pfa_allocate_physical_contiguous_pages: Out of memory at end!");
    kernel_panic_ex(NULL, PANIC_OUT_OF_MEMORY);
    abort();
}

void pfa_free_physical_page(physical_address_t address)
{
    if (address == physical_null)
    {
        LOG(WARNING, "pfa_free_physical_page: Kernel tried to free NULL");
        return;
    }

    assert(!(address & 0xfff));

    #ifdef DEBUG_ALLOCATOR
    return;
    #endif

    uint64_t page_index = 0;
    for (uint32_t i = first_alloc_block; i < usable_memory_blocks; i++)
    {
        if (address >= usable_memory_map[i].address &&
            address < usable_memory_map[i].address + usable_memory_map[i].length)
        {
            page_index += (address - usable_memory_map[i].address) / 0x1000;
            break;
        }
        if (i == usable_memory_blocks - 1)
            return;
        page_index += usable_memory_map[i].length / 0x1000;
    }

    uint64_t qword = page_index / 64;
    uint8_t bit = page_index % 64;
    if (qword * 8 >= bitmap_size) return;
    lock_scheduler();
    *(uint64_t*)&bitmap[8 * qword] &= ~(1ULL << bit);
    if (page_index < first_free_page_index_hint)
        first_free_page_index_hint = page_index;

    memory_allocated -= 0x1000;
    unlock_scheduler();
    // LOG(TRACE, "Freed page: %#" PRIx64, address);
    LOG_MEM_ALLOCATED();
}

void pfa_free_physical_contiguous_pages(physical_address_t address, size_t pages)
{
    for (size_t i = 0; i < pages; i++)
        pfa_free_physical_page(address + 0x1000ULL * i);
}

__attribute__((malloc, malloc(pfa_free_page, 1))) void* pfa_allocate_page()
{
    physical_address_t paddr = pfa_allocate_physical_page();
    if (!paddr) return NULL;
    return (void*)(paddr + PHYS_MAP_BASE);
}

void pfa_free_page(const void* ptr)
{
    if (!ptr) return;
    pfa_free_physical_page((physical_address_t)ptr - PHYS_MAP_BASE);
}

__attribute__((malloc, malloc(pfa_free_contiguous_pages, 1))) void* pfa_allocate_contiguous_pages(size_t pages)
{
    physical_address_t paddr = pfa_allocate_physical_contiguous_pages(pages);
    if (!paddr) return NULL;
    return (void*)(paddr + PHYS_MAP_BASE);
}

void pfa_free_contiguous_pages(const void* ptr, size_t pages)
{
    if (!ptr) return;
    pfa_free_physical_contiguous_pages((physical_address_t)ptr - PHYS_MAP_BASE, pages);
}
