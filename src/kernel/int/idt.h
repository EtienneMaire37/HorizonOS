#pragma once

#include <stdint.h>
#include "../cpu/util.h"

#define ISR_INTERRUPT_GATE_64   0b1110 
#define ISR_TRAP_GATE_64        0b1111

struct idt_entry
{   
    uint16_t offset_lo           : 16;
    uint16_t segment_selector    : 16;
    uint8_t ist                  : 3;
    uint8_t reserved0            : 5;
    uint8_t gate_type            : 4;
    uint8_t zero                 : 1;
    uint8_t DPL                  : 2;
    uint8_t present              : 1;
    uint64_t offset_hi           : 48;
    uint32_t reserved1           : 32;
} __attribute__((__packed__));

extern struct idt_entry IDT[256] __attribute__((aligned(8)));  // "The base addresses of the IDT should be aligned on an 8-byte boundary to maximize performance of cache line fills."
                                                        // -- Intel manual vol 3A 7.10

extern void load_idt(uint16_t limit, uint64_t address);
extern uint64_t interrupt_table[256];  // ptrs to the isrs

void setup_idt_entry(struct idt_entry* entry, uint16_t segment, physical_address_t offset, uint8_t privilege, uint8_t type);
void install_idt();