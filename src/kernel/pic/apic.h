#pragma once

#include "../../libc/include/stdint.h"

#include "defs.h"

// * page 0xFEE00xxx
extern volatile local_apic_registers_t* lapic;

extern uint32_t ps2_1_gsi, ps2_12_gsi;

void madt_extract_data();

void apic_init();
uint8_t apic_get_cpu_id();
void lapic_send_eoi();
void lapic_set_spurious_interrupt_number(uint8_t int_num);
void lapic_enable();
void lapic_set_tpr(uint8_t p);