#include <stdint.h>
#include <stdbool.h>

uint64_t PHYS_MAP_BASE = 0;
uint8_t physical_address_width = 36; // M
bool pat_enabled = false;

#include "paging.h"
#include <string.h>
#include "../memalloc/page_frame_allocator.h"
#include "../memalloc/page_data.h"

uint64_t* create_empty_pdpt()
{
    return (uint64_t*)(((uintptr_t)create_empty_pdpt_phys()) + PHYS_MAP_BASE);
}

physical_address_t create_empty_pdpt_phys()
{
    physical_address_t paddr = pfa_allocate_physical_page();
    uint64_t* pdpt = (uint64_t*)(paddr + PHYS_MAP_BASE);
    if (!paddr)
    {
        LOG(ERROR, "Couldn't create PDPT!!!");
        return physical_null;
    }

    memset(pdpt, 0, 4096);
    ASSERT(global_page_table);
    global_page_table[paddr / 0x1000] = page_data_init;

    return paddr;
}

bool is_pdpt_entry_present(const uint64_t* entry)
{
    if (!entry) return false;
    return (*entry) & 1;
}

bool is_pdpt_entry_large(const uint64_t* entry)
{
    if (!entry) return false;
    return ((*entry) & (1 << 7)) != 0;
}

physical_address_t get_pdpt_entry_address(const uint64_t* entry)
{
    return is_pdpt_entry_present(entry) ? (physical_address_t)(((*entry) & 0xfffffffffffff000) & get_physical_address_mask()) : physical_null;
}

uint8_t get_pdpt_entry_privilege(const uint64_t* entry)
{
    return is_pdpt_entry_present(entry) ? (((*entry) >> 2) & 1) : 0x80;
}

uint8_t get_pdpt_entry_read_write(const uint64_t* entry)
{
    return is_pdpt_entry_present(entry) ? (((*entry) >> 1) & 1) : 0x80;
}

uint8_t get_pdpt_entry_cache_type(const uint64_t* entry)
{
    if (!is_pdpt_entry_present(entry))
        return CACHE_WB;

    // * Assumes init_pat was called
    uint64_t pat = PAGE_ATTRIBUTE_TABLE;
    uint8_t pat_index = (((*entry) >> 3) & 0b10) | (((*entry) >> 3) & 1);
    return pat >> (pat_index * 8);
}

void remove_pdpt_entry(uint64_t* entry)
{
    *entry = 0;
}

void set_pdpt_entry(uint64_t* entry, uint64_t address, uint8_t privilege, uint8_t read_write, uint8_t cache_type)
{
    if (address == 0)
    {
        remove_pdpt_entry(entry);
        return;
    }

    cache_type %= sizeof(pdpt_pat_bits) / sizeof(pdpt_pat_bits[0]);

    uint64_t pcd_bit = (pdpt_pat_bits[cache_type] & 3) >> 1,
             pwt_bit = (pdpt_pat_bits[cache_type] & 3) & 1;

    assert (((address & 0xfffffffffffff000) & get_physical_address_mask()) == address);
    *entry = address | (pcd_bit << 4) | (pwt_bit << 3) | ((privilege & 1) << 2) | ((read_write & 1) << 1) | 1;
}

void remap_range(uint64_t* pml4,
    uint64_t start_virtual_address, uint64_t start_physical_address,
    uint64_t pages,
    uint8_t privilege, uint8_t read_write, uint8_t cache_type)
{
    uint64_t i = 0;
    uint32_t pml4_flags = lock_page_table(pml4);
    uint16_t pml4e = (start_virtual_address >> 39) & 0x1ff;
    uint16_t pdpte = (start_virtual_address >> 30) & 0x1ff;
    uint16_t pde = (start_virtual_address >> 21) & 0x1ff;
    uint16_t pte = (start_virtual_address >> 12) & 0x1ff;
    uint64_t end_virtual_address = make_address_canonical(start_virtual_address + 0x1000 * pages);
    while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
    {
        if (pml4e >= 512)
            break;

        if (!is_pdpt_entry_present(&pml4[pml4e]))
            set_pdpt_entry(&pml4[pml4e], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* pdpt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pml4[pml4e]));
        uint32_t pdpt_flags = lock_page_table(pdpt_address);
        while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
        {
            if (pdpte >= 512)
            {
                simplify_pdpte_paging_indices(&pdpte, &pml4e);
                break;
            }
            if (!is_pdpt_entry_present(&pdpt_address[pdpte]))
                set_pdpt_entry(&pdpt_address[pdpte], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);
            else if (is_pdpt_entry_large(&pdpt_address[pdpte]))
                abort();

            uint64_t* pd_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pdpt_address[pdpte]));
            uint32_t pd_flags = lock_page_table(pd_address);
            while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
            {
                if (pde >= 512)
                {
                    simplify_pdpte_paging_indices(&pde, &pdpte);
                    break;
                }
                if (!is_pdpt_entry_present(&pd_address[pde]))
                    set_pdpt_entry(&pd_address[pde], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);
                else if (is_pdpt_entry_large(&pd_address[pde]))
                    abort();

                uint64_t* pt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pd_address[pde]));
                uint32_t pt_flags = lock_page_table(pt_address);
                for (; vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address; pte++, i++)
                {
                    ASSERT(vaddr_from_indices(pml4e, pdpte, pde, pte) == make_address_canonical(start_virtual_address + 0x1000ULL * i));
                    if (pte >= 512)
                    {
                        simplify_pdpte_paging_indices(&pte, &pde);
                        break;
                    }
                    set_pdpt_entry(&pt_address[pte], vaddr_from_indices(pml4e, pdpte, pde, pte) - start_virtual_address + start_physical_address,
                        privilege, read_write,
                        cache_type);
                }
                unlock_page_table(pt_address, pt_flags);
            }
            unlock_page_table(pd_address, pd_flags);
        }
        unlock_page_table(pdpt_address, pdpt_flags);
    }
    unlock_page_table(pml4, pml4_flags);
}

void allocate_range(uint64_t* pml4,
    uint64_t start_virtual_address,
    uint64_t pages,
    uint8_t privilege, uint8_t read_write, uint8_t cache_type)
{
    uint64_t i = 0;
    uint32_t pml4_flags = lock_page_table(pml4);
    uint16_t pml4e = (start_virtual_address >> 39) & 0x1ff;
    uint16_t pdpte = (start_virtual_address >> 30) & 0x1ff;
    uint16_t pde = (start_virtual_address >> 21) & 0x1ff;
    uint16_t pte = (start_virtual_address >> 12) & 0x1ff;
    uint64_t end_virtual_address = make_address_canonical(start_virtual_address + 0x1000 * pages);
    while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
    {
        if (pml4e >= 512)
            break;

        if (!is_pdpt_entry_present(&pml4[pml4e]))
            set_pdpt_entry(&pml4[pml4e], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* pdpt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pml4[pml4e]));
        uint32_t pdpt_flags = lock_page_table(pdpt_address);
        while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
        {
            if (pdpte >= 512)
            {
                simplify_pdpte_paging_indices(&pdpte, &pml4e);
                break;
            }
            if (!is_pdpt_entry_present(&pdpt_address[pdpte]))
                set_pdpt_entry(&pdpt_address[pdpte], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);
            assert (!is_pdpt_entry_large(&pdpt_address[pdpte]));

            uint64_t* pd_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pdpt_address[pdpte]));
            uint32_t pd_flags = lock_page_table(pd_address);
            while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
            {
                if (pde >= 512)
                {
                    simplify_pdpte_paging_indices(&pde, &pdpte);
                    break;
                }
                if (!is_pdpt_entry_present(&pd_address[pde]))
                    set_pdpt_entry(&pd_address[pde], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);
                assert (!is_pdpt_entry_large(&pd_address[pde]));

                uint64_t* pt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pd_address[pde]));
                uint32_t pt_flags = lock_page_table(pt_address);
                for (; vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address; pte++, i++)
                {
                    ASSERT(vaddr_from_indices(pml4e, pdpte, pde, pte) == make_address_canonical(start_virtual_address + 0x1000ULL * i));
                    if (pte >= 512)
                    {
                        simplify_pdpte_paging_indices(&pte, &pde);
                        break;
                    }
                    if (!is_pdpt_entry_present(&pt_address[pte]))
                        set_pdpt_entry(&pt_address[pte], create_empty_pdpt_phys(), privilege, read_write, cache_type);
                }
                unlock_page_table(pt_address, pt_flags);
            }
            unlock_page_table(pd_address, pd_flags);
        }
        unlock_page_table(pdpt_address, pdpt_flags);
    }
    unlock_page_table(pml4, pml4_flags);
}

// TODO: Reference count and unmap page tables
void free_range(uint64_t* pml4,
    uint64_t start_virtual_address,
    uint64_t pages)
{
    uint64_t i = 0;
    uint32_t pml4_flags = lock_page_table(pml4);
    uint16_t pml4e = (start_virtual_address >> 39) & 0x1ff;
    uint16_t pdpte = (start_virtual_address >> 30) & 0x1ff;
    uint16_t pde = (start_virtual_address >> 21) & 0x1ff;
    uint16_t pte = (start_virtual_address >> 12) & 0x1ff;
    uint64_t end_virtual_address = make_address_canonical(start_virtual_address + 0x1000 * pages);
    while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
    {
        if (pml4e >= 512)
            break;

        if (!is_pdpt_entry_present(&pml4[pml4e]))
        {
            pml4e++;
            continue;
        }

        uint64_t* pdpt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pml4[pml4e]));
        uint32_t pdpt_flags = lock_page_table(pdpt_address);
        while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
        {
            if (pdpte >= 512)
            {
                simplify_pdpte_paging_indices(&pdpte, &pml4e);
                break;
            }
            if (!is_pdpt_entry_present(&pdpt_address[pdpte]))
            {
                pdpte++;
                continue;
            }

            uint64_t* pd_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pdpt_address[pdpte]));
            uint32_t pd_flags = lock_page_table(pd_address);
            while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
            {
                if (pde >= 512)
                {
                    simplify_pdpte_paging_indices(&pde, &pdpte);
                    break;
                }
                if (!is_pdpt_entry_present(&pd_address[pde]))
                {
                    pde++;
                    continue;
                }

                uint64_t* pt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pd_address[pde]));
                uint32_t pt_flags = lock_page_table(pt_address);
                for (; vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address; pte++, i++)
                {
                    ASSERT(vaddr_from_indices(pml4e, pdpte, pde, pte) == make_address_canonical(start_virtual_address + 0x1000ULL * i));
                    if (pte >= 512)
                    {
                        simplify_pdpte_paging_indices(&pte, &pde);
                        break;
                    }
                    if (!is_pdpt_entry_present(&pt_address[pte])) continue;
                    pfa_free_physical_page(get_pdpt_entry_address(&pt_address[pte]));
                    remove_pdpt_entry(&pt_address[pte]);
                }
                unlock_page_table(pt_address, pt_flags);
            }
            unlock_page_table(pd_address, pd_flags);
        }
        unlock_page_table(pdpt_address, pdpt_flags);
    }
    unlock_page_table(pml4, pml4_flags);
}

void unmap_range(uint64_t* pml4,
    uint64_t start_virtual_address,
    uint64_t pages)
{
    uint64_t i = 0;
    uint32_t pml4_flags = lock_page_table(pml4);
    uint16_t pml4e = (start_virtual_address >> 39) & 0x1ff;
    uint16_t pdpte = (start_virtual_address >> 30) & 0x1ff;
    uint16_t pde = (start_virtual_address >> 21) & 0x1ff;
    uint16_t pte = (start_virtual_address >> 12) & 0x1ff;
    uint64_t end_virtual_address = make_address_canonical(start_virtual_address + 0x1000 * pages);
    while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
    {
        if (pml4e >= 512)
            break;

        if (!is_pdpt_entry_present(&pml4[pml4e]))
        {
            pml4e++;
            continue;
        }

        uint64_t* pdpt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pml4[pml4e]));
        uint32_t pdpt_flags = lock_page_table(pdpt_address);
        while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
        {
            if (pdpte >= 512)
            {
                simplify_pdpte_paging_indices(&pdpte, &pml4e);
                break;
            }
            if (!is_pdpt_entry_present(&pdpt_address[pdpte]))
            {
                pdpte++;
                continue;
            }

            uint64_t* pd_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pdpt_address[pdpte]));
            uint32_t pd_flags = lock_page_table(pd_address);
            while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
            {
                if (pde >= 512)
                {
                    simplify_pdpte_paging_indices(&pde, &pdpte);
                    break;
                }
                if (!is_pdpt_entry_present(&pd_address[pde]))
                {
                    pde++;
                    continue;
                }

                uint64_t* pt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pd_address[pde]));
                uint32_t pt_flags = lock_page_table(pt_address);
                for (; vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address; pte++, i++)
                {
                    ASSERT(vaddr_from_indices(pml4e, pdpte, pde, pte) == make_address_canonical(start_virtual_address + 0x1000ULL * i));
                    if (pte >= 512)
                    {
                        simplify_pdpte_paging_indices(&pte, &pde);
                        break;
                    }
                    if (!is_pdpt_entry_present(&pt_address[pte])) continue;
                    remove_pdpt_entry(&pt_address[pte]);
                }
                unlock_page_table(pt_address, pt_flags);
            }
            unlock_page_table(pd_address, pd_flags);
        }
        unlock_page_table(pdpt_address, pdpt_flags);
    }
    unlock_page_table(pml4, pml4_flags);
}

void copy_mapping(uint64_t* src, uint64_t* dst,
    uint64_t start_virtual_address,
    uint64_t pages)
{
    uint32_t pml4_flags = lock_page_table(src);
    uint32_t dst_pml4_flags = lock_page_table(dst);
    uint16_t pml4e = (start_virtual_address >> 39) & 0x1ff;
    uint16_t pdpte = (start_virtual_address >> 30) & 0x1ff;
    uint16_t pde = (start_virtual_address >> 21) & 0x1ff;
    uint16_t pte = (start_virtual_address >> 12) & 0x1ff;
    uint64_t end_virtual_address = make_address_canonical(start_virtual_address + 0x1000 * pages);
    while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
    {
        if (pml4e >= 512)
            break;

        if (!is_pdpt_entry_present(&src[pml4e]))
        {
            pml4e++;
            continue;
        }

        if (!is_pdpt_entry_present(&dst[pml4e]))
            set_pdpt_entry(&dst[pml4e], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* pdpt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&src[pml4e]));
        uint64_t* dst_pdpt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&dst[pml4e]));
        uint32_t pdpt_flags = lock_page_table(pdpt_address);
        uint32_t dst_pdpt_flags = lock_page_table(dst_pdpt_address);
        while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
        {
            if (pdpte >= 512)
            {
                simplify_pdpte_paging_indices(&pdpte, &pml4e);
                break;
            }

            if (!is_pdpt_entry_present(&pdpt_address[pdpte]))
            {
                pdpte++;
                continue;
            }

            // * 1GB page
            if (is_pdpt_entry_large(&pdpt_address[pdpte]))
            {
                dst_pdpt_address[pdpte] = pdpt_address[pdpte];
                pdpte++;
                continue;
            }

            if (!is_pdpt_entry_present(&dst_pdpt_address[pdpte]))
                set_pdpt_entry(&dst_pdpt_address[pdpte], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

            uint64_t* pd_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pdpt_address[pdpte]));
            uint64_t* dst_pd_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&dst_pdpt_address[pdpte]));
            uint32_t pd_flags = lock_page_table(pd_address);
            uint32_t dst_pd_flags = lock_page_table(dst_pd_address);
            for (; vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address; pde++)
            {
                if (pde >= 512)
                {
                    simplify_pdpte_paging_indices(&pde, &pdpte);
                    break;
                }

                if (!is_pdpt_entry_present(&pd_address[pde])) continue;

                // * 2MB page
                if (is_pdpt_entry_large(&pd_address[pde]))
                {
                    dst_pd_address[pde] = pd_address[pde];
                    continue;
                }

                if (!is_pdpt_entry_present(&dst_pd_address[pde]))
                    set_pdpt_entry(&dst_pd_address[pde], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

                uint64_t* pt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pd_address[pde]));
                uint64_t* dst_pt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&dst_pd_address[pde]));
                uint32_t pt_flags = lock_page_table(pt_address);
                uint32_t dst_pt_flags = lock_page_table(dst_pt_address);

                memcpy(dst_pt_address, pt_address, 4096);

                unlock_page_table(dst_pt_address, dst_pt_flags);
                unlock_page_table(pt_address, pt_flags);
            }
            unlock_page_table(dst_pd_address, dst_pd_flags);
            unlock_page_table(pd_address, pd_flags);
        }
        unlock_page_table(dst_pdpt_address, dst_pdpt_flags);
        unlock_page_table(pdpt_address, pdpt_flags);
    }
    unlock_page_table(dst, dst_pml4_flags);
    unlock_page_table(src, pml4_flags);
}

// * Doesn't support large pages
void copy_vas(uint64_t* src, uint64_t* dst,
    uint64_t start_virtual_address,
    uint64_t pages)
{
    uint32_t pml4_flags = lock_page_table(src);
    uint32_t dst_pml4_flags = lock_page_table(dst);
    uint16_t pml4e = (start_virtual_address >> 39) & 0x1ff;
    uint16_t pdpte = (start_virtual_address >> 30) & 0x1ff;
    uint16_t pde = (start_virtual_address >> 21) & 0x1ff;
    uint16_t pte = (start_virtual_address >> 12) & 0x1ff;
    uint64_t end_virtual_address = make_address_canonical(start_virtual_address + 0x1000 * pages);
    while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
    {
        if (pml4e >= 512)
            break;

        if (!is_pdpt_entry_present(&src[pml4e]))
        {
            pml4e++;
            continue;
        }

        if (!is_pdpt_entry_present(&dst[pml4e]))
            set_pdpt_entry(&dst[pml4e], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* pdpt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&src[pml4e]));
        uint64_t* dst_pdpt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&dst[pml4e]));
        uint32_t pdpt_flags = lock_page_table(pdpt_address);
        uint32_t dst_pdpt_flags = lock_page_table(dst_pdpt_address);
        while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
        {
            if (pdpte >= 512)
            {
                simplify_pdpte_paging_indices(&pdpte, &pml4e);
                break;
            }

            if (!is_pdpt_entry_present(&pdpt_address[pdpte]))
            {
                pdpte++;
                continue;
            }

            if (!is_pdpt_entry_present(&dst_pdpt_address[pdpte]))
                set_pdpt_entry(&dst_pdpt_address[pdpte], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

            uint64_t* pd_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pdpt_address[pdpte]));
            uint64_t* dst_pd_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&dst_pdpt_address[pdpte]));
            uint32_t pd_flags = lock_page_table(pd_address);
            uint32_t dst_pd_flags = lock_page_table(dst_pd_address);
            while (vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address)
            {
                if (pde >= 512)
                {
                    simplify_pdpte_paging_indices(&pde, &pdpte);
                    break;
                }

                if (!is_pdpt_entry_present(&pd_address[pde]))
                {
                    pde++;
                    break;
                }

                if (!is_pdpt_entry_present(&dst_pd_address[pde]))
                    set_pdpt_entry(&dst_pd_address[pde], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

                uint64_t* pt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pd_address[pde]));
                uint64_t* dst_pt_address = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(&dst_pd_address[pde]));
                uint32_t pt_flags = lock_page_table(pt_address);
                uint32_t dst_pt_flags = lock_page_table(dst_pt_address);
                for (; vaddr_from_indices(pml4e, pdpte, pde, pte) < end_virtual_address; pte++)
                {
                    if (pte >= 512)
                    {
                        simplify_pdpte_paging_indices(&pte, &pde);
                        break;
                    }

                    if (!is_pdpt_entry_present(&pt_address[pte])) continue;

                    if (!is_pdpt_entry_present(&dst_pt_address[pte]))
                        set_pdpt_entry(&dst_pt_address[pte], pfa_allocate_physical_page(), get_pdpt_entry_privilege(&pt_address[pte]), get_pdpt_entry_read_write(&pt_address[pte]), CACHE_WB);
                    memcpy((void*)(PHYS_MAP_BASE + get_pdpt_entry_address(&dst_pt_address[pte])), (void*)(PHYS_MAP_BASE + get_pdpt_entry_address(&pt_address[pte])), 4096);
                }
                unlock_page_table(dst_pt_address, dst_pt_flags);
                unlock_page_table(pt_address, pt_flags);
            }
            unlock_page_table(dst_pd_address, dst_pd_flags);
            unlock_page_table(pd_address, pd_flags);
        }
        unlock_page_table(dst_pdpt_address, dst_pdpt_flags);
        unlock_page_table(pdpt_address, pdpt_flags);
    }
    unlock_page_table(dst, dst_pml4_flags);
    unlock_page_table(src, pml4_flags);
}

void* virtual_to_physical(uint64_t* cr3, uint64_t vaddr)
{
    ASSERT(cr3);

    uint32_t pml4_flags = lock_page_table(cr3);

    uint64_t pte = (vaddr >> 12) & 0x1ff;
    uint64_t pde = (vaddr >> (12 + 9)) & 0x1ff;
    uint64_t pdpte = (vaddr >> (12 + 2 * 9)) & 0x1ff;
    uint64_t pml4e = (vaddr >> (12 + 3 * 9)) & 0x1ff;

    uint64_t* pml4_entry = &cr3[pml4e];
    if (!is_pdpt_entry_present(pml4_entry))
    {
        unlock_page_table(cr3, pml4_flags);
        LOG(TRACE, "virtual_to_physical: pml4 entry not present");
        return NULL;
    }

    uint64_t* pdpt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pml4_entry));
    uint32_t pdpt_flags = lock_page_table(pdpt);

    uint64_t* pdpt_entry = &pdpt[pdpte];
    if (!is_pdpt_entry_present(pdpt_entry))
    {
        unlock_page_table(pdpt, pdpt_flags);
        unlock_page_table(cr3, pml4_flags);
        LOG(TRACE, "virtual_to_physical: pdpt entry not present");
        return NULL;
    }

    uint64_t* pd = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pdpt_entry));
    uint32_t pd_flags = lock_page_table(pd);

    uint64_t* pd_entry = &pd[pde];
    if (!is_pdpt_entry_present(pd_entry))
    {
        unlock_page_table(pd, pd_flags);
        unlock_page_table(pdpt, pdpt_flags);
        unlock_page_table(cr3, pml4_flags);
        LOG(TRACE, "virtual_to_physical: pd entry not present");
        return NULL;
    }

    uint64_t* pt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pd_entry));
    uint32_t pt_flags = lock_page_table(pt);

    uint64_t* pt_entry = &pt[pte];
    if (!is_pdpt_entry_present(pt_entry))
    {
        unlock_page_table(pt, pt_flags);
        unlock_page_table(pd, pd_flags);
        unlock_page_table(pdpt, pdpt_flags);
        unlock_page_table(cr3, pml4_flags);
        LOG(TRACE, "virtual_to_physical: pt entry not present");
        return NULL;
    }

    uint8_t* page = (uint8_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pt_entry));

    unlock_page_table(pt, pt_flags);
    unlock_page_table(pd, pd_flags);
    unlock_page_table(pdpt, pdpt_flags);
    unlock_page_table(cr3, pml4_flags);

    return (void*)&page[vaddr & 0xfff];
}
