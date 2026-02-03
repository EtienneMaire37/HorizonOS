#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../paging/paging.h"
#include "../cpu/memory.h"

static inline __attribute__((always_inline)) bool is_page_free(uint64_t vaddr)
{
    uint64_t* pml4 = (uint64_t*)(get_cr3() + PHYS_MAP_OFFSET);

    uint64_t pte = (vaddr >> 12) & 0x1ff;
    uint64_t pde = (vaddr >> (12 + 9)) & 0x1ff;
    uint64_t pdpte = (vaddr >> (12 + 2 * 9)) & 0x1ff;
    uint64_t pml4e = (vaddr >> (12 + 3 * 9)) & 0x1ff;

    uint64_t* pml4_entry = &pml4[pml4e];
    if (!is_pdpt_entry_present(pml4_entry))
        return true;

    uint64_t* pdpt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pml4_entry));
    
    uint64_t* pdpt_entry = &pdpt[pdpte];
    if (!is_pdpt_entry_present(pdpt_entry))
        return true;

    uint64_t* pd = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pdpt_entry));

    uint64_t* pd_entry = &pd[pde];
    if (!is_pdpt_entry_present(pd_entry))
        return true;

    uint64_t* pt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pd_entry));

    uint64_t* pt_entry = &pt[pte];
    if (is_pdpt_entry_present(pt_entry))
        return false;

    return true;
}

void* vmm_find_free_user_space_pages(void* hint, size_t pages);
void* vmm_find_free_kernel_space_pages(void* hint, size_t pages);