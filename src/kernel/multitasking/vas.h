#pragma once

#include "../paging/paging.h"
#include "task.h"

physical_address_t task_create_empty_vas(uint8_t privilege)
{
    physical_address_t paddr = create_empty_pdpt_phys();
    uint64_t* cr3 = (uint64_t*)(paddr + PHYS_MAP_BASE);
    if (!paddr) return physical_null;

    for (int i = 256; i < 512; i++)
        set_pdpt_entry(&cr3[i], get_pdpt_entry_address(&global_cr3[i]), PG_SUPERVISOR, PG_READ_WRITE, CACHE_WB);

    allocate_range(cr3, TASK_STACK_BOTTOM_ADDRESS, TASK_STACK_PAGES,
            privilege, PG_READ_WRITE, CACHE_WB);
    if (privilege != PG_SUPERVISOR)
        allocate_range(cr3, TASK_KERNEL_STACK_BOTTOM_ADDRESS, TASK_KERNEL_STACK_PAGES,
            PG_SUPERVISOR, PG_READ_WRITE, CACHE_WB);

    return paddr;
}

void task_free_vas(physical_address_t pml4_address)
{
    uint64_t* cr3 = (uint64_t*)(pml4_address + PHYS_MAP_BASE);
// * Skip kernel mappings
    for (uint16_t pml4e = 0; pml4e < 256; pml4e++)
    {
        if (!is_pdpt_entry_present(&cr3[pml4e])) continue;

        uint64_t* pdpt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&cr3[pml4e]));
        for (uint16_t pdpte = 0; pdpte < 512; pdpte++)
        {
            if (!is_pdpt_entry_present(&pdpt_address[pdpte])) continue;

            uint64_t* pd_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pdpt_address[pdpte]));
            for (uint16_t pde = 0; pde < 512; pde++)
            {
                if (!is_pdpt_entry_present(&pd_address[pde])) continue;

                uint64_t* pt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pd_address[pde]));
                for (uint16_t pte = 0; pte < 512; pte++)
                {
                    if (!is_pdpt_entry_present(&pt_address[pte])) continue;

                    pfa_free_physical_page(get_pdpt_entry_address(&pt_address[pte]));
                }
                pfa_free_physical_page((physical_address_t)pt_address);
            }
            pfa_free_physical_page((physical_address_t)pd_address);
        }
        pfa_free_physical_page((physical_address_t)pdpt_address);
    }
    pfa_free_physical_page(pml4_address);
}

void tas_vas_copy(uint64_t* src, uint64_t* dst, 
    uint64_t start_virtual_address, 
    uint64_t pages)
{
    if (!src)
    {
        LOG(ERROR, "tas_vas_copy: NULL source virtual address space");
        return;
    }

    if (!dst)
    {
        LOG(ERROR, "tas_vas_copy: NULL destination virtual address space");
        return;
    }

    if (start_virtual_address & 0xfff)
    {
        LOG(CRITICAL, "tas_vas_copy: Kernel tried to map non page aligned addresses");
        abort();
    }

    // * "uncanonize"
    start_virtual_address &= 0xffffffffffff;

    for (uint64_t i = 0; i < pages; i++)
    {
        uint64_t vaddr = start_virtual_address + 0x1000 * i;

        if (vaddr < start_virtual_address)
            break;

        uint64_t pte = (vaddr >> 12) & 0x1ff;
        uint64_t pde = (vaddr >> (12 + 9)) & 0x1ff;
        uint64_t pdpte = (vaddr >> (12 + 2 * 9)) & 0x1ff;
        uint64_t pml4e = (vaddr >> (12 + 3 * 9)) & 0x1ff;

        uint64_t* old_pml4_entry = &src[pml4e];
        if (!is_pdpt_entry_present(old_pml4_entry))
        {
            i += ((uint64_t)1 << (9 * 3)) - (pdpte << (9 * 2)) - (pde << 9) - pte - 1;
            continue;
        }

        uint64_t* new_pml4_entry = &dst[pml4e];

        if (!is_pdpt_entry_present(new_pml4_entry))
            set_pdpt_entry(new_pml4_entry, create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* old_pdpt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(old_pml4_entry));
        
        uint64_t* old_pdpt_entry = &old_pdpt[pdpte];
        if (!is_pdpt_entry_present(old_pdpt_entry))
        {
            i += ((uint64_t)1 << (9 * 2)) - (pde << 9) - pte - 1;
            continue;
        }

        uint64_t* new_pdpt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(new_pml4_entry));

        uint64_t* new_pdpt_entry = &new_pdpt[pdpte];

        if (!is_pdpt_entry_present(new_pdpt_entry))
            set_pdpt_entry(new_pdpt_entry, create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* old_pd = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(old_pdpt_entry));

        uint64_t* old_pd_entry = &old_pd[pde];
        if (!is_pdpt_entry_present(old_pd_entry))
        {
            i += ((uint64_t)1 << 9) - pte - 1;
            continue;
        }

        uint64_t* new_pd = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(new_pdpt_entry));

        uint64_t* new_pd_entry = &new_pd[pde];

        if (!is_pdpt_entry_present(new_pd_entry))
            set_pdpt_entry(new_pd_entry, create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        // * !!!
        uint64_t* old_pt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(old_pd_entry));

        uint64_t* old_pt_entry = &old_pt[pte];
        if (!is_pdpt_entry_present(old_pt_entry))
            continue;

        uint64_t* new_pt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(new_pd_entry));

        uint64_t* new_pt_entry = &new_pt[pte];

        if (!is_pdpt_entry_present(new_pt_entry))
            set_pdpt_entry(new_pt_entry, pfa_allocate_physical_page(), get_pdpt_entry_privilege(old_pt_entry), get_pdpt_entry_read_write(old_pt_entry), CACHE_WB);
        
        memcpy((void*)(PHYS_MAP_BASE + get_pdpt_entry_address(new_pt_entry)), (void*)(PHYS_MAP_BASE + get_pdpt_entry_address(old_pt_entry)), 4096);
    }
}