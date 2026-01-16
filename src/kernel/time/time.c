#include "time.h"

volatile int64_t system_seconds = 0, system_minutes = 0, system_hours = 0, system_day = 0, system_month = 0;
volatile int64_t system_year = 0;
volatile int64_t system_thousands = 0;

bool time_initialized = false;

volatile precise_time_t global_timer = 0;