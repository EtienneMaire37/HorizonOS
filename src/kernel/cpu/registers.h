#pragma once

#include "../../libc/include/stdint.h"

static inline uint64_t get_cr0()
{
    uint64_t rax;
    asm volatile("mov rax, cr0" : "=a" (rax));
    return rax;
}

static inline void load_cr0(uint64_t cr0)
{
    asm volatile("mov cr0, rax" : : "a" (cr0));
}

static inline uint64_t get_cr4()
{
    uint64_t rax;
    asm volatile("mov rax, cr4" : "=a" (rax));
    return rax;
}

static inline void load_cr4(uint64_t cr4)
{
    asm volatile("mov cr4, rax" : : "a" (cr4));
}

extern uint64_t get_rflags();
extern void set_eflags(uint64_t value);