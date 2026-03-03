#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../cpu/msr.h"
#include "defs.h"

// * xAPIC MMIO base
extern volatile local_apic_registers_t* lapic;

extern uint32_t ps2_1_gsi, ps2_12_gsi;

void madt_extract_data();

void apic_init();
uint32_t __attribute__((const)) apic_get_cpu_id();
void lapic_send_eoi();
void lapic_set_spurious_interrupt_number(uint8_t int_num);
void lapic_enable();
void lapic_set_tpr(uint8_t p);
void apic_timer_init();

static inline bool is_bsp()
{
    // * BSP bit
    return (rdmsr(IA32_APIC_BASE_MSR) & (1 << 8)) != 0;
}