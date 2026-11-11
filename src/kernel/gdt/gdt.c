#include <string.h>
#include "gdt.h"
#include "../multitasking/thread_def.h"
#include "../util/assert.h"

uint64_t GDT[7];	// 5 + 2 for TSS
struct tss_entry TSS;

void setup_gdt_entry(uint64_t* entry, physical_address_t base, uint32_t limit, uint8_t access_byte, uint8_t flags)
{
    ASSERT(entry);
    *entry = (((uint64_t)base & 0xff000000) << 32) | (((uint64_t)flags & 0xf) << 52) | ((((uint64_t)limit >> 16) & 0xf) << 48) |
        ((uint64_t)access_byte << 40) | ((((uint64_t)base >> 16) & 0xff) << 32) | ((base & 0xffff) << 16) | (limit & 0xffff);
}

void setup_ssd_gdt_entry(uint64_t* entry, physical_address_t base, uint32_t limit, uint8_t access_byte, uint8_t flags)
{
    ASSERT(entry);
    setup_gdt_entry(entry, base & 0xffffffff, limit, access_byte, flags);
    *(uint64_t*)&entry[1] = (base >> 32) & 0xffffffff;
}

void install_gdt()
{
    load_gdt(sizeof(GDT) - 1, (uint64_t)&GDT);
}

void setup_gdt_tss()
{
    memset(&GDT[0], 0, sizeof(uint64_t));       // NULL Descriptor
    setup_gdt_entry(&GDT[1], 0, 0xfffff, 0x9A, 0xA);    // Kernel mode code segment
    setup_gdt_entry(&GDT[2], 0, 0xfffff, 0x92, 0xC);    // Kernel mode data segment
    setup_gdt_entry(&GDT[3], 0, 0xfffff, 0xF2, 0xC);    // User mode data segment
    setup_gdt_entry(&GDT[4], 0, 0xfffff, 0xFA, 0xA);    // User mode code segment

    memset(&TSS, 0, sizeof(struct tss_entry));
    TSS.rsp0 = TASK_KERNEL_STACK_TOP_ADDRESS;
    setup_ssd_gdt_entry(&GDT[5], (physical_address_t)&TSS, sizeof(struct tss_entry) - 1, 0x89, 0);  // TSS

    install_gdt();
    load_tss();
}
