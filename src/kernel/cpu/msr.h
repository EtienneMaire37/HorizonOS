#pragma once

#include "../../libc/include/stdint.h"

#define IA32_APIC_BASE_MSR          0x1B
#define IA32_APIC_BASE_MSR_BSP      0x100
#define IA32_APIC_BASE_MSR_ENABLE   0x800

#define IA32_PAT_MSR                0x277

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t low, high;
    asm volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t value) 
{
    uint32_t low = (uint32_t)(value & 0xffffffff);
    uint32_t high = (uint32_t)(value >> 32);
    asm volatile ("wrmsr" :: "c"(msr), "a"(low), "d"(high));
}