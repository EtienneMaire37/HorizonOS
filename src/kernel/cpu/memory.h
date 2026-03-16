#pragma once

#include <stdint.h>

typedef enum
{
    MEMORY_LOWER_HALF = 0,
    MEMORY_HIGHER_HALF = 1
} memory_half_t;

#define LOWER_HALF  MEMORY_LOWER_HALF
#define HIGHER_HALF MEMORY_HIGHER_HALF

static inline void invlpg(uint64_t addr)
{
    asm volatile("invlpg [%0]" :: "r" (addr) : "memory");
}

static inline void load_cr3(uint64_t addr)
{
    asm volatile("mov cr3, rax" :: "a" ((uint64_t)addr) : "memory");
}

static inline uint64_t get_cr3()
{
    uint64_t val;
    asm volatile("mov rax, cr3" : "=a"(val));
    return val;
}

static inline uint64_t get_cr3_address()
{
    return get_cr3() & ~0xfffULL;
}

#define memory_barrier()    asm volatile("" ::: "memory")