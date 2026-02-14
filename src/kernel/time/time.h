#pragma once

#include "../cpu/util.h"

typedef uint64_t precise_time_t;

extern volatile int64_t system_seconds, system_minutes, system_hours, system_day, system_month;
extern volatile int64_t system_year;
extern volatile int64_t system_thousands;

extern bool time_initialized;

extern volatile precise_time_t global_timer;

#define GLOBAL_TIMER_FREQUENCY  1000
#define GLOBAL_TIMER_INCREMENT  (precise_time_ticks_per_second / GLOBAL_TIMER_FREQUENCY)

static const int precise_time_ticks_per_second = 1000000;  // * microseconds

#define PRECISE_SECONDS         precise_time_ticks_per_second
#define PRECISE_MILLISECONDS    (precise_time_ticks_per_second / 1000)
#define PRECISE_MICROSECONDS    (precise_time_ticks_per_second / 1000000)

static inline uint64_t precise_time_to_milliseconds(precise_time_t time)
{
    return time * 1000 / precise_time_ticks_per_second;
}

static inline void ksleep(precise_time_t time)
{
    time++; // * Should guarantee to wait AT LEAST time
    precise_time_t start_timer = global_timer;
    while (global_timer < start_timer + time)
        hlt();
}