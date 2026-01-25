#pragma once

#include "../../libc/include/stdint.h"

#define IA32_APIC_BASE_MSR          0x1B

#define IA32_PAT_MSR                0x277

#define IA32_EFER_MSR               0xC0000080
#define IA32_STAR_MSR               0xC0000081
#define IA32_LSTAR_MSR              0xC0000082

#define IA32_KERNEL_GS_BASE_MSR     0xC0000102

#define IA32_FS_BASE_MSR            0xC0000100
#define IA32_GS_BASE_MSR            0xC0000101

static inline __attribute__((always_inline)) uint64_t rdmsr(uint32_t msr)
{
    uint32_t low, high;
    asm volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr) : "memory");
    return ((uint64_t)high << 32) | low;
}

static inline __attribute__((always_inline)) void wrmsr(uint32_t msr, uint64_t value) 
{
    uint32_t low = (uint32_t)(value & 0xffffffff);
    uint32_t high = (uint32_t)(value >> 32);
    asm volatile ("wrmsr" :: "c"(msr), "a"(low), "d"(high) : "memory");
}