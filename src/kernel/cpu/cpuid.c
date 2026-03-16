#include "cpuid.h"

uint32_t cpuid_highest_function_parameter = 0xffffffff, cpuid_highest_extended_function_parameter = 0xffffffff;
char manufacturer_id_string[13] = { 0 };
int cpu_brand = CPU_UNKNOWN;