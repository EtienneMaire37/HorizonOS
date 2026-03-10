#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../cpu/msr.h"
#include "defs.h"

// * xAPIC MMIO base
extern volatile local_apic_registers_t* lapic;

#define LAPIC_TIMER_DIVIDE_BY_1     0b1011
#define LAPIC_TIMER_DIVIDE_BY_2     0b0000
#define LAPIC_TIMER_DIVIDE_BY_4     0b0001
#define LAPIC_TIMER_DIVIDE_BY_8     0b0010
#define LAPIC_TIMER_DIVIDE_BY_16    0b0011
#define LAPIC_TIMER_DIVIDE_BY_32    0b1000
#define LAPIC_TIMER_DIVIDE_BY_64    0b1001
#define LAPIC_TIMER_DIVIDE_BY_128   0b1010

#define LAPIC_TIMER_ONE_SHOT        0x00000
#define LAPIC_TIMER_PERIODIC        0x20000
#define LAPIC_TIMER_TSC_DEADLINE    0x40000

#define LAPIC_TIMER_UNMASKED        0x00000
#define LAPIC_TIMER_MASKED          0x10000

extern uint32_t ps2_1_gsi, ps2_12_gsi;

void madt_extract_data();

void apic_init();
uint32_t apic_get_cpu_id();
void lapic_send_eoi();
void lapic_set_spurious_interrupt_number(uint8_t int_num);
void lapic_enable();
void lapic_set_tpr(uint8_t p);
void apic_timer_init();

static inline bool is_bsp()
{
    // * BSP bit
    return (rdmsr(IA32_APIC_BASE_MSR) & (1ULL << 8)) != 0;
}