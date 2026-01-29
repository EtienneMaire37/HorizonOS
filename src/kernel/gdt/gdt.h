#pragma once

#include <stdint.h>
#include "../../libc/include/stddef.h"
#include "../cpu/util.h"

struct gdt_decriptor
{
    uint16_t size;      // The size of the table in bytes subtracted by 1
    uint64_t address;   // The linear address of the GDT (not the physical address, paging applies).
} __attribute__((__packed__));

struct gdt_entry
{   
    uint16_t 	limit_lo   	: 16;
    uint16_t 	base_lo    	: 16;
    uint8_t 	base_mid    : 8;
    uint8_t 	access_byte : 8;
    uint8_t 	limit_hi    : 4;
    uint8_t 	flags       : 4;
    uint8_t 	base_hi     : 8;
} __attribute__((__packed__));

struct tss_entry 
{
	uint32_t reserved0;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t reserved1;
	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;
	uint64_t reserved2;
	uint16_t reserved3;
	uint16_t iopb;
} __attribute__((__packed__));

static const int tss_rsp0_offset = offsetof(struct tss_entry, rsp0);

#define TSS_TYPE_16BIT_TSS_AVL  0x1
#define TSS_TYPE_16BIT_TSS_BSY  0x3
#define TSS_TYPE_32BIT_TSS_AVL  0x9
#define TSS_TYPE_32BIT_TSS_BSY  0xB
#define TSS_TYPE_LDT            0x2

#define KERNEL_CODE_SEGMENT 	0x08
#define KERNEL_DATA_SEGMENT 	0x10

#define USER_DATA_SEGMENT   	(0x18 | 3)
#define USER_CODE_SEGMENT   	(0x20 | 3)

#define TSS_SEGMENT   			0x28

extern struct gdt_entry GDT[7];	// 5 + 2 for TSS
extern struct tss_entry TSS;

extern void load_gdt(uint16_t limit, uint64_t address);
extern void load_tss();

void setup_gdt_entry(struct gdt_entry* entry, physical_address_t base, uint32_t limit, uint8_t access_byte, uint8_t flags);
void setup_ssd_gdt_entry(struct gdt_entry* entry, physical_address_t base, uint32_t limit, uint8_t access_byte, uint8_t flags);
void install_gdt();