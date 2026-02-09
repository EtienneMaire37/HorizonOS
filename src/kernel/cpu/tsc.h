#pragma once

#include "../time/time.h"

extern uint64_t tsc_cyles_per_second;

static inline uint64_t rdtsc()
{
    uint32_t eax, edx;
    asm volatile("rdtsc" : "=a"(eax), "=d"(edx));
    return ((uint64_t)edx << 32) | eax;
}

static inline void calibrate_tsc()
{
    precise_time_t start = global_timer;
    while (start == global_timer);
    start = global_timer;
    uint64_t start_tsc = rdtsc();
    while (global_timer < start + 1000 * PRECISE_MILLISECONDS);
    uint64_t end_tsc = rdtsc();
    tsc_cyles_per_second = (end_tsc - start_tsc) * 1000 / 1000;
}