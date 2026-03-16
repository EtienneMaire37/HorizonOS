#pragma once

#include "../cpu/registers.h"
#include "../debug/out.h"
#include "fpu.h"
#include "../cpu/cpuid.h"

extern uint64_t fpu_state_component_bitmap;
extern bool xsave_supported, fxsave_supported;

static inline uint64_t get_supported_xcr0()
{
    uint32_t eax = 0, ebx, ecx, edx = 0;
// * 64-bit bitmap of state-components 
// * supported by XCR0 on this CPU.
    cpuid_with_ecx(0x0d, 0, eax, ebx, ecx, edx);
    return ((uint64_t)edx << 32) | eax;
}
static inline uint32_t get_xsave_area_size()
{
    uint32_t eax, ebx = 0, ecx, edx;
// * Maximum size (in bytes) 
// * of XSAVE save area for the set 
// * of state-components currently set in XCR0. 
    cpuid_with_ecx(0x0d, 0, eax, ebx, ecx, edx);
    return ebx;
}

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

static inline void enable_fpu()
{
    uint64_t cr4 = get_cr4();
    if (fxsave_supported)
        cr4 |= (1 << 9);    // * OSFXSR
    if (xsave_supported)
        cr4 |= (1 << 18);   // * OSXSAVE
    load_cr4(cr4);
    uint64_t cr0 = get_cr0();
    cr0 &= ~(1 << 2);   // * clear EM
    cr0 |= (1 << 1);    // * set   MP
    load_cr0(cr0);

    if (xsave_supported)
    {
        fpu_state_component_bitmap = get_supported_xcr0();
        LOG(DEBUG, "XCR0: %#" PRIx64, fpu_state_component_bitmap);
        load_xcr0(fpu_state_component_bitmap);
    }
}