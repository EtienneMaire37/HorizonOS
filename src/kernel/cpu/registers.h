#pragma once

#include <stdint.h>

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
static inline uint64_t get_rbp()
{
    uint64_t rbp;
    asm volatile ("mov rax, rbp" : "=a"(rbp));
    return rbp;
}
static inline void set_rbp(uint64_t rbp)
{
    asm volatile ("mov rbp, rax" :: "a"(rbp));
}

extern uint64_t get_rflags();
extern void set_rflags(uint64_t value);
extern uint64_t set_rflags_if(uint64_t val);