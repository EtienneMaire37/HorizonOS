#pragma once

#include "../cpu/msr.h"
#include "../cpu/cpuid.h"
#include <stdint.h>
#include <stdlib.h>
#include "../cpu/util.h"

#include <stdbool.h>

#define PHYS_MAP_OFFSET     0xffff800000000000
extern uint64_t PHYS_MAP_BASE;

#define PG_SHIFT        12ULL
#define PG_SIZE         (1ULL << PG_SHIFT)
#define IDX_BITS        9ULL
#define IDX_ENTRIES     (1ULL << IDX_BITS)
#define IDX_MASK        (IDX_ENTRIES - 1ULL)
#define CANON_MASK      0x0000FFFFFFFFFFFFULL

#define PAGES_PT        1ULL
#define PAGES_PD        (IDX_ENTRIES * PAGES_PT)    /* 512 pages = 2 MiB */
#define PAGES_PDP       (IDX_ENTRIES * PAGES_PD)    /* 512*512 pages = 1 GiB */
#define PAGES_PML4      (IDX_ENTRIES * PAGES_PDP)

#define PG_SUPERVISOR   0
#define PG_USER         1

#define PG_READ_ONLY    0
#define PG_READ_WRITE   1

#define CACHE_UC        0ULL    // * All accesses are uncacheable. Write combining is not allowed. Speculative accesses are not allowed.
#define CACHE_WC        1ULL    // * All accesses are uncacheable. Write combining is allowed. Speculative reads are allowed. 
#define CACHE_WT        4ULL    // * Reads allocate cache lines on a cache miss. Cache lines are not allocated on a write miss.
                                // - Write hits update the cache and main memory. 
#define CACHE_WP        5ULL    // * Reads allocate cache lines on a cache miss. All writes update main memory.
                                // - Cache lines are not allocated on a write miss. Write hits invalidate the cache line and update main memory. 
#define CACHE_WB        6ULL    // * Reads allocate cache lines on a cache miss, and can allocate to either the shared,
                                // - exclusive, or modified state. Writes allocate to the modified state on a cache miss. 
#define CACHE_UC_MINUS  7ULL    // * Same as uncacheable, except that this can be overriden by Write-Combining MTRRs. 

static const uint8_t pdpt_pat_bits[8] = 
{
    2,
    3,
    0,
    0,
    1,
    2,
    0,
    2
};

extern uint8_t physical_address_width; // M
extern bool pat_enabled;

static inline uint64_t get_physical_address_mask()
{
    if (physical_address_width == 0)
        abort();
    return physical_address_width >= 64 ? 0xffffffffffffffff : (1ULL << physical_address_width) - 1;
}

static inline void init_pat()
{
    uint32_t eax, ebx, ecx, edx = 0;
    cpuid(1, eax, ebx, ecx, edx);

    pat_enabled = (edx >> 16) & 1;

    if (!pat_enabled)
        return;

    // * WC if PAT is set or UC, default else
    wrmsr(IA32_PAT_MSR,
         CACHE_WB | 
        (CACHE_WT << 8) | 
        (CACHE_UC << 16) | 
        (CACHE_WC << 24) |

        (CACHE_WC << 32) |
        (CACHE_WC << 40) |
        (CACHE_WC << 48) |
        (CACHE_WC << 56));
}

uint64_t* create_empty_pdpt();
physical_address_t create_empty_pdpt_phys();
uint64_t* create_empty_virtual_address_space();
bool is_pdpt_entry_present(const uint64_t* entry);
physical_address_t get_pdpt_entry_address(const uint64_t* entry);
uint8_t get_pdpt_entry_privilege(const uint64_t* entry);
uint8_t get_pdpt_entry_read_write(const uint64_t* entry);
void remove_pdpt_entry(uint64_t* entry);
void set_pdpt_entry(uint64_t* entry, uint64_t address, uint8_t privilege, uint8_t read_write, uint8_t cache_type);

void* virtual_to_physical(uint64_t* cr3, uint64_t vaddr);

void remap_range(uint64_t* pml4, 
    uint64_t start_virtual_address, uint64_t start_physical_address, 
    uint64_t pages,
    uint8_t privilege, uint8_t read_write, uint8_t cache_type);
void allocate_range(uint64_t* pml4, 
    uint64_t start_virtual_address, 
    uint64_t pages,
    uint8_t privilege, uint8_t read_write, uint8_t cache_type);
void free_range(uint64_t* pml4, 
    uint64_t start_virtual_address, 
    uint64_t pages);
void copy_mapping(uint64_t* src, uint64_t* dst, 
    uint64_t start_virtual_address, 
    uint64_t pages);