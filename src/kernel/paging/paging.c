#include <stdint.h>
#include <stdbool.h>

uint64_t PHYS_MAP_BASE = 0;
uint8_t physical_address_width = 0; // M
bool pat_enabled = false;

#include "../cpu/memory.h"
#include "paging.h"
#include <string.h>
#include "../memalloc/page_frame_allocator.h"

uint64_t* create_empty_pdpt()
{
    uint64_t* pdpt = pfa_allocate_page();
    if (!pdpt) 
    {
        LOG(ERROR, "Couldn't create PDPT!!!");
        return NULL;
    }

    memset(pdpt, 0, 4096);

    return (uint64_t*)pdpt;
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

    return paddr;
}

bool is_pdpt_entry_present(const uint64_t* entry)
{
    if (!entry) return false;
    return (*entry) & 1;
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
        return 0x80 | CACHE_WB;

    uint64_t pat = rdmsr(IA32_PAT_MSR);
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

    cache_type &= 7;

    uint64_t pcd_bit = (pdpt_pat_bits[cache_type] & 3) >> 1,
             pwt_bit = (pdpt_pat_bits[cache_type] & 3) & 1;

    uint64_t masked_address = (address & 0xfffffffffffff000) & get_physical_address_mask();
    if (masked_address != address)
    {
        LOG(CRITICAL, "Kernel tried to map physical address %#" PRIx64 " but it doesn't fit in %u bits", address, physical_address_width);
        abort();
    }

    *entry = masked_address | (pcd_bit << 4) | (pwt_bit << 3) | ((privilege & 1) << 2) | ((read_write & 1) << 1) | 1;
}

void remap_range(uint64_t* pml4, 
    uint64_t start_virtual_address, uint64_t start_physical_address, 
    uint64_t pages,
    uint8_t privilege, uint8_t read_write, uint8_t cache_type)
{
    if (!pml4)
    {
        LOG(ERROR, "remap_range: NULL virtual address space");
        return;
    }

    if ((start_virtual_address | start_physical_address) & 0xfff)
    {
        LOG(CRITICAL, "remap_range: Kernel tried to map non page aligned addresses");
        abort();
    }

    // * "uncanonize"
    start_virtual_address &= 0xffffffffffff;

    // uint64_t end_virtual_address = start_virtual_address + 0x1000 * pages;

    // for (uint64_t vaddr = start_virtual_address; vaddr < end_virtual_address; vaddr += 0x1000)
    for (uint64_t i = 0; i < pages; i++)
    {
        uint64_t vaddr = start_virtual_address + 0x1000 * i;

        uint64_t pte = (vaddr >> 12) & 0x1ff;
        uint64_t pde = (vaddr >> (12 + 9)) & 0x1ff;
        uint64_t pdpte = (vaddr >> (12 + 2 * 9)) & 0x1ff;
        uint64_t pml4e = (vaddr >> (12 + 3 * 9)) & 0x1ff;

        uint64_t* pml4_entry = &pml4[pml4e];
        if (!is_pdpt_entry_present(pml4_entry))
            set_pdpt_entry(pml4_entry, create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* pdpt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pml4_entry));
        
        uint64_t* pdpt_entry = &pdpt[pdpte];
        if (!is_pdpt_entry_present(pdpt_entry))
            set_pdpt_entry(pdpt_entry, create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* pd = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pdpt_entry));

        uint64_t* pd_entry = &pd[pde];
        if (!is_pdpt_entry_present(pd_entry))
            set_pdpt_entry(pd_entry, create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* pt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pd_entry));

        uint64_t* pt_entry = &pt[pte];
        // if (is_pdpt_entry_present(pt_entry))
        //     continue;

        set_pdpt_entry(pt_entry, vaddr - start_virtual_address + start_physical_address, 
            privilege, read_write,
            cache_type);
    }
}

void allocate_range(uint64_t* pml4, 
    uint64_t start_virtual_address, 
    uint64_t pages,
    uint8_t privilege, uint8_t read_write, uint8_t cache_type)
{
    if (!pml4)
    {
        LOG(ERROR, "allocate_range: NULL virtual address space");
        return;
    }

    if (start_virtual_address & 0xfff)
    {
        LOG(CRITICAL, "allocate_range: Kernel tried to map non page aligned addresses");
        abort();
    }

    // * "uncanonize"
    start_virtual_address &= 0xffffffffffff;

    // uint64_t end_virtual_address = start_virtual_address + 0x1000 * pages;

    // for (uint64_t vaddr = start_virtual_address; vaddr < end_virtual_address; vaddr += 0x1000)
    for (uint64_t i = 0; i < pages; i++)
    {
        uint64_t vaddr = start_virtual_address + 0x1000 * i;

        uint64_t pte = (vaddr >> 12) & 0x1ff;
        uint64_t pde = (vaddr >> (12 + 9)) & 0x1ff;
        uint64_t pdpte = (vaddr >> (12 + 2 * 9)) & 0x1ff;
        uint64_t pml4e = (vaddr >> (12 + 3 * 9)) & 0x1ff;

        uint64_t* pml4_entry = &pml4[pml4e];
        if (!is_pdpt_entry_present(pml4_entry))
            set_pdpt_entry(pml4_entry, create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* pdpt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pml4_entry));
        
        uint64_t* pdpt_entry = &pdpt[pdpte];
        if (!is_pdpt_entry_present(pdpt_entry))
            set_pdpt_entry(pdpt_entry, create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* pd = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pdpt_entry));

        uint64_t* pd_entry = &pd[pde];
        if (!is_pdpt_entry_present(pd_entry))
            set_pdpt_entry(pd_entry, create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* pt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pd_entry));

        uint64_t* pt_entry = &pt[pte];
        if (is_pdpt_entry_present(pt_entry))
            continue;

        set_pdpt_entry(pt_entry, pfa_allocate_physical_page(), 
            privilege, read_write,
            cache_type);
    }
}

void free_range(uint64_t* pml4, 
    uint64_t start_virtual_address, 
    uint64_t pages)
{
    if (!pml4)
    {
        LOG(ERROR, "free_range: NULL virtual address space");
        return;
    }

    if (start_virtual_address & 0xfff)
    {
        LOG(CRITICAL, "free_range: Kernel tried to free non page aligned addresses");
        abort();
    }

    // * "uncanonize"
    start_virtual_address &= 0xffffffffffff;

    for (uint64_t i = 0; i < pages; i++)
    {
        uint64_t vaddr = start_virtual_address + 0x1000 * i;

        uint64_t pte = (vaddr >> 12) & 0x1ff;
        uint64_t pde = (vaddr >> (12 + 9)) & 0x1ff;
        uint64_t pdpte = (vaddr >> (12 + 2 * 9)) & 0x1ff;
        uint64_t pml4e = (vaddr >> (12 + 3 * 9)) & 0x1ff;

        uint64_t* pml4_entry = &pml4[pml4e];
        if (!is_pdpt_entry_present(pml4_entry))
        {
            i += ((uint64_t)1 << (9 * 3)) - (pdpte << (9 * 2)) - (pde << 9) - pte - 1;
            continue;
        }

        uint64_t* pdpt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pml4_entry));
        
        uint64_t* pdpt_entry = &pdpt[pdpte];
        if (!is_pdpt_entry_present(pdpt_entry))
        {
            i += ((uint64_t)1 << (9 * 2)) - (pde << 9) - pte - 1;
            continue;
        }

        uint64_t* pd = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pdpt_entry));

        uint64_t* pd_entry = &pd[pde];
        if (!is_pdpt_entry_present(pd_entry))
        {
            i += ((uint64_t)1 << 9) - pte - 1;
            continue;
        }

        uint64_t* pt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pd_entry));

        uint64_t* pt_entry = &pt[pte];
        if (!is_pdpt_entry_present(pt_entry))
            continue;

        uint64_t address = (uint64_t)get_pdpt_entry_address(pt_entry);
        remove_pdpt_entry(pt_entry);
        pfa_free_physical_page(address);
    }
}

void copy_mapping(uint64_t* src, uint64_t* dst, 
    uint64_t start_virtual_address, 
    uint64_t pages)
{
    if (!src)
    {
        LOG(ERROR, "copy_mapping: NULL source virtual address space");
        return;
    }

    if (!dst)
    {
        LOG(ERROR, "copy_mapping: NULL destination virtual address space");
        return;
    }

    if (start_virtual_address & 0xfff)
    {
        LOG(CRITICAL, "copy_mapping: Kernel tried to map non page aligned addresses");
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
            set_pdpt_entry(new_pml4_entry, create_empty_pdpt_phys(), PG_SUPERVISOR, PG_READ_WRITE, CACHE_WB);

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
            set_pdpt_entry(new_pdpt_entry, create_empty_pdpt_phys(), PG_SUPERVISOR, PG_READ_WRITE, CACHE_WB);

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
            set_pdpt_entry(new_pd_entry, create_empty_pdpt_phys(), PG_SUPERVISOR, PG_READ_WRITE, CACHE_WB);

        uint64_t* old_pt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(old_pd_entry));
        uint64_t* new_pt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(new_pd_entry));

        memcpy(new_pt, old_pt, 4096);

        i += ((uint64_t)1 << 9) - pte - 1;
    }
}

void* virtual_to_physical(uint64_t* cr3, uint64_t vaddr)
{	
    if (!cr3)
        return NULL;
        
    // * "uncanonize"
    vaddr &= 0xffffffffffff;

    uint64_t pte = (vaddr >> 12) & 0x1ff;
    uint64_t pde = (vaddr >> (12 + 9)) & 0x1ff;
    uint64_t pdpte = (vaddr >> (12 + 2 * 9)) & 0x1ff;
    uint64_t pml4e = (vaddr >> (12 + 3 * 9)) & 0x1ff;

    uint64_t* pml4_entry = &cr3[pml4e];
    if (!is_pdpt_entry_present(pml4_entry))
        return NULL;

    uint64_t* pdpt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pml4_entry));
    
    uint64_t* pdpt_entry = &pdpt[pdpte];
    if (!is_pdpt_entry_present(pdpt_entry))
        return NULL;

    uint64_t* pd = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pdpt_entry));

    uint64_t* pd_entry = &pd[pde];
    if (!is_pdpt_entry_present(pd_entry))
        return NULL;

    uint64_t* pt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pd_entry));

    uint64_t* pt_entry = &pt[pte];
    if (!is_pdpt_entry_present(pt_entry))
        return NULL;

    uint8_t* page = (uint8_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pt_entry));

    return (void*)&page[vaddr & 0xfff];
}
