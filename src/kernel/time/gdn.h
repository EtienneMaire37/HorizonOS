#pragma once

#include <time.h>

#define GDN_EPOCH 719162

static inline uint32_t year_to_gdn(uint16_t year, bool* leap) 
{
    if (year >= (uint32_t)(-1) / 146097)
    {
        LOG(CRITICAL, "Year is too large (); Can't convert to Gregorian Day Number");
        abort();
    }

    year--;
    uint16_t cycle = year / 400;
    uint16_t year_in_cycle = year % 400;
    uint16_t century = year_in_cycle / 100;
    uint16_t year_in_century = year_in_cycle % 100;
    uint16_t group = year_in_century / 4;
    uint16_t year_in_group = year_in_century % 4;
    if (leap) *leap = year_in_group == 3 && (year_in_century != 99 || year_in_cycle == 399); // *leap = (year % 4 == 0) && (year % 400 != 0);
    return (uint32_t)146097 * cycle + 36524 * century + 1461 * group + 365* year_in_group;
}

static inline uint32_t time_to_gdn(uint16_t year, uint8_t month, uint8_t day)
{
    month -= 1;
    bool leap;
    static const unsigned short month_to_gdn[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    uint32_t gdn = year_to_gdn(year, &leap);
    gdn += month_to_gdn[month] + (leap && month > 1);
    gdn += day - 1;
    return gdn;
}

static inline time_t time_to_unix(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    uint32_t gdn = time_to_gdn(year, month, day);
    return (gdn - GDN_EPOCH) * 86400LL + hour * 3600 + minute * 60 + second;
}