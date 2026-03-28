#pragma once

#include "../time/time.h"
#include "../cpu/cpuid.h"

extern uint64_t tsc_cycles_per_second;

static inline uint64_t rdtsc()
{
    uint32_t eax, edx;
    asm volatile("rdtsc" : "=a"(eax), "=d"(edx));
    return ((uint64_t)edx << 32) | eax;
}

static inline void calibrate_tsc()
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
    precise_time_t start = global_timer;
    while (start == global_timer)
        hlt();
    start = global_timer;
    uint64_t start_tsc = rdtsc();
    const int ms = 1000;
    while (global_timer < start + ms * PRECISE_MILLISECONDS)
        hlt();
    uint64_t end_tsc = rdtsc();
    tsc_cycles_per_second = (end_tsc - start_tsc) * 1000 / ms;
    // * Round up
    // * round(n) = floor((floor(2n)+1)/2)
    const uint64_t roundfac = 10000000;
    uint64_t two_n = 2 * tsc_cycles_per_second / roundfac;
    uint64_t n = (two_n + 1) / 2;
    tsc_cycles_per_second = n * roundfac;
}
