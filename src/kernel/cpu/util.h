#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "../util/cfunc.h"

typedef uint64_t physical_address_t;
typedef uint64_t virtual_address_t;

extern uint64_t* global_cr3;

#define enable_interrupts()     do { asm volatile ("sti"); } while (0)
#define disable_interrupts()    do { asm volatile ("cli"); } while (0)

#define hlt()                   do { asm volatile ("hlt"); } while (0)
#define ud()                    do { asm volatile ("ud2"); } while (0)

#define swapgs()                do { asm volatile ("swapgs"); } while (0)

#define mfence()                do { asm volatile ("mfence"); } while (0)

#define tpause(state, deadline)        do { asm volatile ("tpause %0" :: "r"(state), "a"((deadline) & 0xffffffff), "d"((deadline) >> 32)); } while (0)

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
    (void)func; (void)file; (void)line;
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
static inline bool __attribute__((const)) is_address_canonical(uint64_t address)
{
    return (address < 0x0000800000000000ULL) || (address >= 0xffff800000000000ULL);
}
static inline uint64_t __attribute__((const)) make_address_canonical(uint64_t address)
{
    if (address & 0x0000800000000000ULL)
        return address | 0xffff800000000000ULL;
    else
        return address & 0x00007fffffffffffULL;
}

#define physical_null ((physical_address_t)0)
