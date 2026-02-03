#include "virtual_memory_allocator.h"
#include "../multitasking/task.h"
#include "../cpu/memory.h"

const uint64_t start = 0x800000;

static void* vmm_find_free_pages(void* hint, size_t pages, memory_half_t half)
{
    if (pages > 0x800000000 / 2 - start) 
        return NULL;
    for (uint64_t vaddr = (uint64_t)(hint + 0xfff) & ~0xfff;; vaddr += 0x1000)
    {
        if (half == LOWER_HALF) vaddr &= ~0xffff800000000000ULL;
        else                    vaddr |=  0xffff800000000000ULL;
        if ((vaddr & ~0xffff800000000000ULL) + 0x1000ULL * pages > 0x800000000000)
            vaddr = start + half * 0xffff800000000000ULL;
        if (vaddr <= start + half * 0xffff800000000000ULL)
            vaddr = start + half * 0xffff800000000000ULL;

        bool found_space = true;
        for (uint64_t offset = 0; offset <= 0x1000ULL * pages; offset += 0x1000)
        {
            if (!is_page_free(vaddr + offset))
            {
                vaddr += offset;
                found_space = false;
                break;
            }
        }

        if (!found_space) continue;

        return (void*)vaddr;
    }
    __builtin_unreachable();
}

void* vmm_find_free_user_space_pages(void* hint, size_t pages)
{
    return vmm_find_free_pages(hint, pages, LOWER_HALF);
}

void* vmm_find_free_kernel_space_pages(void* hint, size_t pages)
{
    return vmm_find_free_pages(hint, pages, HIGHER_HALF);
}