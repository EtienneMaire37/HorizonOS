#pragma once

#include "cmos.h"
#include "../time/ktime.h"

bool rtc_binary_mode;   // 0 = BCD, 1 = Binary
bool rtc_24_hour_mode;  // 0 = 12-hour, 1 = 24-hour

void rtc_detect_mode()
{
    cmos_select_register(CMOS_REGISTER_STATUS_B);
    uint8_t status_b = cmos_read_register();

    rtc_binary_mode = status_b & 0b100;
    rtc_24_hour_mode = status_b & 0b10;
}

void rtc_get_time()
{
    rtc_wait_while_updating();
    cmos_select_register(CMOS_REGISTER_SECONDS);
    system_seconds = cmos_read_register();
    cmos_select_register(CMOS_REGISTER_MINUTES);
    system_minutes = cmos_read_register();
    cmos_select_register(CMOS_REGISTER_HOURS);
    system_hours = cmos_read_register();
    cmos_select_register(CMOS_REGISTER_DAY);
    system_day = cmos_read_register();
    cmos_select_register(CMOS_REGISTER_MONTH);
    system_month = cmos_read_register();
    cmos_select_register(CMOS_REGISTER_YEAR);
    system_year = cmos_read_register();
    cmos_select_register(CMOS_CENTURY_REGISTER);
    uint8_t century = cmos_read_register();
    century = rtc_binary_mode ? century : bcd_to_binary(century);

    bool pm = system_hours & 0x80;

    system_seconds = rtc_binary_mode ? system_seconds : bcd_to_binary(system_seconds);
    system_minutes = rtc_binary_mode ? system_minutes : bcd_to_binary(system_minutes);
    system_hours = rtc_binary_mode ? system_hours & 0x7f : bcd_to_binary(system_hours & 0x7f);
    system_day = rtc_binary_mode ? system_day : bcd_to_binary(system_day);
    system_month = rtc_binary_mode ? system_month : bcd_to_binary(system_month);

    system_year = (rtc_binary_mode ? system_year : bcd_to_binary(system_year)) + 100 * century; // ((system_year < 70) ? 2000 : 1900);

    if (!rtc_24_hour_mode)
    {
        if (pm)
            system_hours = (system_hours + 12) % 24;    // 12pm = 0am, 12am = 12am
        else
            system_hours = system_hours;
    }

    system_thousands = 0;

    resolve_time();
}
