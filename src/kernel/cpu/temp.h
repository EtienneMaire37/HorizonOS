#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "cpuid.h"
#include "msr.h"
#include "../util/likely.h"

extern uint8_t tcc_activation_temperature;
extern bool temperature_sensor;

static inline void cpu_init_sensor()
{
    // * The processor supports a digital thermal sensor if CPUID.06H.EAX[0] = 1.
    // * -- Intel manual vol 3B 15.8.5.1
    if (cpuid_highest_function_parameter >= 0x6)
    {
        uint32_t eax = 0, ebx, ecx, edx;
        cpuid(0x6, eax, ebx, ecx, edx);
        temperature_sensor = eax & 1;
    }
    else
        temperature_sensor = false;
}

static inline void cpu_read_tcc()
{
    if (!temperature_sensor) return;
    uint64_t val = rdmsr(MSR_TEMPERATURE_TARGET_MSR);
    tcc_activation_temperature = (val >> 16) & 0xff;
}

static inline uint8_t cpu_read_temp()
{
    if (!temperature_sensor) return 0;
    uint64_t val = rdmsr(IA32_THERM_STATUS_MSR);
    if (unlikely(((val >> 31) & 1) == 0))
        return 0;
    uint8_t readout = (val >> 16) & 0x7f;
    if (unlikely(readout > tcc_activation_temperature))
        return 0;
    // uint8_t resolution = (val >> 27) & 0x0f;
    return tcc_activation_temperature - readout;
}
