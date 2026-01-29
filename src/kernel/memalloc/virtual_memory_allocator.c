#include "virtual_memory_allocator.h"
#include "../multitasking/task.h"

const uint64_t start = 0x800000;

void* vmm_find_free_user_space_pages(void* hint, size_t pages)
{
    if (pages > 0x800000000 - start) 
        return NULL;
    for (uint64_t vaddr = (uint64_t)(hint + 0xfff) & ~0xfff;; vaddr += 0x1000)
    {
        vaddr &= ~0xffff800000000000ULL;
        if (vaddr + 0x1000ULL * pages > 0x800000000000)
            vaddr = start;
        if (vaddr <= start)
            vaddr = start;

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
    // return NULL;
    __builtin_unreachable();
}