#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../util/defs.h"
#include "../cpu/registers.h"
#include "../cpu/cpuid.h"
#include "../debug/out.h"

#define XSAVES      0
#define XSAVEOPT    1
#define XSAVEC      2
#define XSAVE       3
#define FXSAVE      4
#define FSAVE       5
#define NO_FPU      6

static inline char* fpu_get_save_instruction_name(int inst)
{
    switch (inst)
    {
    def_case(XSAVES)
    def_case(XSAVEOPT)
    def_case(XSAVEC)
    def_case(XSAVE)
    def_case(FXSAVE)
    def_case(FSAVE)
    default:
        return "INVALID";
    }
}

extern int xsave_instruction;

extern uint16_t fpu_test;

extern bool has_fpu;

extern uint8_t* fpu_default_state;
extern uint32_t xsave_area_size, xsave_area_pages;

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

void fpu_init_defaults();
void fpu_init();
void fpu_save_state(uint8_t* fpu_state);
void fpu_restore_state(uint8_t* fpu_state);
void fpu_state_init(uint8_t* fpu_state);
uint8_t* fpu_state_create();
void fpu_state_destroy(uint8_t** data);
uint8_t* fpu_state_create_copy(uint8_t* state);
