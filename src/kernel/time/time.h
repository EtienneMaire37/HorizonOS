#pragma once

#include "../cpu/util.h"

typedef uint64_t precise_time_t;

extern volatile int64_t system_seconds, system_minutes, system_hours, system_day, system_month;
extern volatile int64_t system_year;
extern volatile int64_t system_thousands;

extern bool time_initialized;

extern volatile precise_time_t global_timer;

#define GLOBAL_TIMER_FREQUENCY  1000ULL
#define GLOBAL_TIMER_INCREMENT  (precise_time_ticks_per_second / GLOBAL_TIMER_FREQUENCY)

static const uint64_t precise_time_ticks_per_second = 1000000000;  // * nanoseconds

#define PRECISE_SECONDS         precise_time_ticks_per_second
#define PRECISE_MILLISECONDS    (precise_time_ticks_per_second / 1000ULL)
#define PRECISE_MICROSECONDS    (precise_time_ticks_per_second / 1000000ULL)
#define PRECISE_NANOSECONDS     (precise_time_ticks_per_second / 1000000000ULL)

static inline uint64_t precise_time_to_milliseconds(precise_time_t time)
{
    return time / PRECISE_MILLISECONDS;
}

static inline void ksleep(precise_time_t time)
{
    time++; // * Should guarantee to wait AT LEAST time
    precise_time_t start_timer = global_timer;
    while (global_timer < start_timer + time)
        hlt();
}
