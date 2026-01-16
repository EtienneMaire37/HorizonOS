#pragma once

#include "../debug/out.h"

extern uint32_t cpuid_highest_function_parameter, cpuid_highest_extended_function_parameter;

#define cpuid(eax, return_eax, return_ebx, return_ecx, return_edx)  if (cpuid_highest_function_parameter >= eax || ((eax & 0x80000000) && cpuid_highest_extended_function_parameter >= eax)) \
                                                                        asm volatile("cpuid" : "=a" (return_eax), "=b" (return_ebx), "=c" (return_ecx), "=d" (return_edx) : "a" (eax)); \
                                                                    else \
                                                                        LOG(ERROR, "CPUID function not supported (0x%x)", eax);

extern char manufacturer_id_string[13];    // 12 byte string ending with ASCII NULL