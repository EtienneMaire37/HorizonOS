#pragma once

#include "../debug/out.h"
#include <assert.h>

extern uint32_t cpuid_highest_function_parameter, cpuid_highest_extended_function_parameter;

#define cpuid_no_check(eax, return_eax, return_ebx, return_ecx, return_edx)  asm volatile("cpuid" : "=a" (return_eax), "=b" (return_ebx), "=c" (return_ecx), "=d" (return_edx) : "a" (eax));
#define cpuid(eax, return_eax, return_ebx, return_ecx, return_edx)  assert(cpuid_highest_function_parameter >= eax || ((eax & 0x80000000) && cpuid_highest_extended_function_parameter >= eax)); \
                                                                    asm volatile("cpuid" : "=a" (return_eax), "=b" (return_ebx), "=c" (return_ecx), "=d" (return_edx) : "a" (eax));
#define cpuid_with_ecx(eax, ecx, return_eax, return_ebx, return_ecx, return_edx)    assert(cpuid_highest_function_parameter >= eax || ((eax & 0x80000000) && cpuid_highest_extended_function_parameter >= eax)); \
                                                                                    asm volatile("cpuid" : "=a" (return_eax), "=b" (return_ebx), "=c" (return_ecx), "=d" (return_edx) : "a" (eax), "c"(ecx));
extern char manufacturer_id_string[13];    // * 12 byte string ending with ASCII NULL

#define CPU_UNKNOWN 0
#define CPU_INTEL   1
#define CPU_AMD     2

extern int cpu_brand;

static inline uint8_t cpuid_get_cpu_id()
{
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, eax, ebx, ecx, edx);
    return ebx >> 24;
}