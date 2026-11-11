#pragma once

#include "../cpu/cpuid.h"

extern uint64_t tsc_cycles_per_second;
extern uint64_t last_tsc;

static inline uint64_t rdtsc()
{
    uint32_t eax, edx;
    asm volatile("rdtsc" : "=a"(eax), "=d"(edx));
    return ((uint64_t)edx << 32) | eax;
}

static inline void try_calibrate_tsc_with_cpuid()
{
    if (cpuid_highest_function_parameter >= 0x15)
    {
        uint32_t eax, ebx, ecx, edx;
        cpuid(0x15, eax, ebx, ecx, edx);
        if (eax != 0 && ebx != 0 && ecx != 0)
        {
            tsc_cycles_per_second = (uint64_t)ecx * ebx / eax;
            return;
        }
    }
}
