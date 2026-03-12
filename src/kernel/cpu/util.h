#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include "../util/cfunc.h"

typedef uint64_t physical_address_t;
typedef uint64_t virtual_address_t;

extern uint64_t* global_cr3;

#define enable_interrupts()     asm volatile ("sti")
#define disable_interrupts()    asm volatile ("cli")

#define hlt()                   asm volatile ("hlt")
#define ud()                    asm volatile ("ud2")

#define swapgs()                asm volatile ("swapgs")

static inline void __attribute__((noreturn)) _halt()
{
    while (true)
    {
        disable_interrupts();
        hlt();
    }
    __builtin_unreachable();
}

static inline void __attribute__((noreturn)) cause_halt(const char* func, const char* file, int line)
{
    disable_interrupts();
    // LOG(ERROR, "Kernel halted in function \"%s\" at line %d in file \"%s\"", func, line, file);
    _halt();
}

static inline void simple_cause_halt()
{
    disable_interrupts();
    // LOG(ERROR, "Kernel halted");
    _halt();
}

#define halt() do {fflush(stdout); cause_halt(__CURRENT_FUNC__, __FILE__, __LINE__); __builtin_unreachable();} while (0)

// * Only support 48-bit canonical addresses for now (4 level paging)
static inline bool is_address_canonical(uint64_t address)
{
    return (address < 0x0000800000000000ULL) || (address >= 0xffff800000000000ULL);
}
static inline uint64_t make_address_canonical(uint64_t address)
{
    if (address & 0x0000800000000000ULL)
        return address | 0xffff800000000000ULL;
    else
        return address & 0x00007fffffffffffULL;
}

#define physical_null ((physical_address_t)0)