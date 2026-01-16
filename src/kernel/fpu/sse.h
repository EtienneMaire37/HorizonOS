#pragma once

#include "../cpu/registers.h"
#include "../debug/out.h"

extern uint64_t STATE_COMPONENT_BITMAP;

extern uint64_t get_supported_xcr0();
extern uint32_t get_xsave_area_size();

static inline uint64_t get_xcr0()
{
    uint64_t eax, edx;
    asm volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    return (edx << 32) | (eax & 0xffffffff);
}

static inline void load_xcr0(uint64_t xcr0)
{
    asm volatile("xsetbv" :: "a"(xcr0 & 0xffffffff), "c"(0), "d"(xcr0 >> 32));
}

static inline void enable_sse()
{
    uint32_t cr0 = get_cr0();
    cr0 &= ~(1 << 3);   // * Clear TS
    cr0 &= ~(1 << 2);   // * Clear EM
    cr0 |= (1 << 1);    // * Set MP
    load_cr0(cr0);

    uint32_t cr4 = get_cr4();
    // cr4 |= (1 << 9);    // * OSFXSR
    // cr4 |= (1 << 10);   // * OSXMMEXCPT
    cr4 &= ~(1 << 11);  // * !UMIP
    load_cr4(cr4);
}

static inline void enable_avx()
{
    uint64_t cr4 = get_cr4();
    cr4 |= (1 << 18);   // * OSXSAVE
    load_cr4(cr4);

    STATE_COMPONENT_BITMAP = get_supported_xcr0();
    LOG(DEBUG, "XCR0: %#llx", STATE_COMPONENT_BITMAP);
    load_xcr0(STATE_COMPONENT_BITMAP);
}