#pragma once

#include "time.h"
#include "gdn.h"
#include <time.h>

static inline time_t ktime(time_t* t)
{
    time_t now = time_to_unix(system_year, system_month, system_day, system_hours, system_minutes, system_seconds);
    if (t) *t = now;
    return now;
}

static inline bool is_leap_year(int64_t year)
{
    if ((year % 400) == 0)
        return true;
    if ((year % 100) == 0)
        return false;
    return year % 4 == 0;
}

static inline uint8_t get_num_days_in_month(int64_t month, int64_t year)
{
    month = imod(month, 12);
    if (month == 1) // February
        return 28 + (is_leap_year(year) ? 1 : 0);
    if (month <= 6) // July
        return 30 + ((month & 1) == 0);
    return 30 + ((month & 1) == 1);
}

static inline void resolve_time()
{
    while (system_thousands < 0 || system_thousands >= 1000)
    {
        system_seconds -= system_thousands >= 1000 ? -1 : 1;
        system_thousands += (system_thousands >= 1000 ? -1 : 1) * 1000;
    }
    while (system_seconds < 0 || system_seconds >= 60)
    {
        system_minutes -= system_seconds >= 60 ? -1 : 1;
        system_seconds += (system_seconds >= 60 ? -1 : 1) * 60;
    }
    while (system_minutes < 0 || system_minutes >= 60)
    {
        system_hours -= system_minutes >= 60 ? -1 : 1;
        system_minutes += (system_minutes >= 60 ? -1 : 1) * 60;
    }
    while (system_hours < 0 || system_hours >= 24)
    {
        system_day -= system_hours >= 24 ? -1 : 1;
        system_hours += (system_hours >= 24 ? -1 : 1) * 24;
    }
    uint8_t num_days_current_month = 0;
    while (system_day < 0 || system_day >= (num_days_current_month = get_num_days_in_month(system_month, system_year)))
    {
        system_month -= system_day >= num_days_current_month ? -1 : 1;
        system_day += (system_day >= num_days_current_month ? -1 : 1) * num_days_current_month;
    }
    while (system_month < 0 || system_month >= 12)
    {
        system_year -= system_month >= 12 ? -1 : 1;
        system_month += (system_month >= 12 ? -1 : 1) * 12;
    }
}