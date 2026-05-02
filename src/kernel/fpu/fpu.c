#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "fpu.h"
#include "../util/error.h"

int xsave_instruction = NO_FPU;

uint16_t fpu_test;

bool has_fpu;

uint8_t* fpu_default_state = NULL;
uint32_t xsave_area_size, xsave_area_pages;

#include "fpu.h"
#include "sse.h"

#include "../memalloc/page_frame_allocator.h"
#include "../debug/out.h"
#include <assert.h>

void fpu_init_defaults()
{
    xsave_area_size = xsave_supported ? get_xsave_area_size() : 512;
    xsave_area_pages = (xsave_area_size + 0xfff) / 0x1000;

    LOG(DEBUG, "%s area is %u bytes long (%u page%s)", fpu_get_save_instruction_name(xsave_instruction), xsave_area_size, xsave_area_pages, xsave_area_pages == 1 ? "" : "s");
    printf("%s area is %u bytes long (%u page%s)\n", fpu_get_save_instruction_name(xsave_instruction), xsave_area_size, xsave_area_pages, xsave_area_pages == 1 ? "" : "s");

    fpu_default_state = pfa_allocate_contiguous_pages(xsave_area_pages);
    memset((uint8_t*)fpu_default_state, 0, xsave_area_size);
    fpu_init();
    fpu_save_state(fpu_default_state);

// !!! The XSAVE instruction does not write any part of the XSAVE header other than the XSTATE_BV field; in particular,
// !!! it does *not* write to the XCOMP_BV field.
}

void fpu_init()
{
    asm volatile("fninit" ::: "memory");
}

void fpu_save_state(uint8_t* s)
{
    switch (xsave_instruction)
    {
    case XSAVES:
        asm volatile("xsaves [rdi]" :: "a"(fpu_state_component_bitmap & 0xffffffff), "D"(s), "d"(fpu_state_component_bitmap >> 32) : "memory");
        break;
    case XSAVEOPT:
        asm volatile("xsaveopt [rdi]" :: "a"(fpu_state_component_bitmap & 0xffffffff), "D"(s), "d"(fpu_state_component_bitmap >> 32) : "memory");
        break;
    case XSAVEC:
        asm volatile("xsavec [rdi]" :: "a"(fpu_state_component_bitmap & 0xffffffff), "D"(s), "d"(fpu_state_component_bitmap >> 32) : "memory");
        break;
    case XSAVE:
        asm volatile("xsave [rdi]" :: "a"(fpu_state_component_bitmap & 0xffffffff), "D"(s), "d"(fpu_state_component_bitmap >> 32) : "memory");
        break;
    case FXSAVE:
        asm volatile("fxsave [rdi]" :: "D"(s) : "memory");
        break;
    case FSAVE:
        asm volatile("fsave [rdi]" :: "D"(s) : "memory");
        break;
    default:
        FATAL("Unknown XSAVE instruction");
    }
}

void fpu_restore_state(uint8_t* s)
{
    switch (xsave_instruction)
    {
    case XSAVES:
        asm volatile("xrstors [rdi]" :: "a"(fpu_state_component_bitmap & 0xffffffff), "D"(s), "d"(fpu_state_component_bitmap >> 32) : "memory");
        break;
    case XSAVEOPT:
        asm volatile("xrstor [rdi]" :: "a"(fpu_state_component_bitmap & 0xffffffff), "D"(s), "d"(fpu_state_component_bitmap >> 32) : "memory");
        break;
    case XSAVEC:
        asm volatile("xrstor [rdi]" :: "a"(fpu_state_component_bitmap & 0xffffffff), "D"(s), "d"(fpu_state_component_bitmap >> 32) : "memory");
        break;
    case XSAVE:
        asm volatile("xrstor [rdi]" :: "a"(fpu_state_component_bitmap & 0xffffffff), "D"(s), "d"(fpu_state_component_bitmap >> 32) : "memory");
        break;
    case FXSAVE:
        asm volatile("fxrstor [rdi]" :: "D"(s) : "memory");
        break;
    case FSAVE:
        asm volatile("frstor [rdi]" :: "D"(s) : "memory");
        break;
    default:
        FATAL("Unknown XRSTOR instruction");
    }
}

void fpu_state_init(uint8_t* s)
{
    if (!s)
    {
        LOG(WARNING, "fpu_state_init: s == NULL");
        return;
    }
    memcpy((uint8_t*)s, (uint8_t*)fpu_default_state, xsave_area_size);
}

uint8_t* fpu_state_create_empty()
{
    // * Alignment is needed
    uint8_t* ptr = pfa_allocate_contiguous_pages(xsave_area_pages);

    assert(ptr);
    assert(((uintptr_t)ptr & 0xfff) == 0);

    memset(ptr, 0, xsave_area_size);
    // LOG(DEBUG, "Allocated FPU state: %p", ptr);
    return ptr;
}

uint8_t* fpu_state_create()
{
    uint8_t* data = fpu_state_create_empty();
    fpu_state_init(data);
    return data;
}

uint8_t* fpu_state_create_copy(uint8_t* state)
{
    if (!state)
    {
        LOG(WARNING, "fpu_state_create_copy: state == NULL");
        return NULL;
    }
    uint8_t* data = fpu_state_create_empty();
    memcpy((uint8_t*)data, (uint8_t*)state, xsave_area_size);
    return data;
}

void fpu_state_destroy(uint8_t** data)
{
    if (!data) return;
    if (!(*data)) return;

    // LOG(DEBUG, "Freeing FPU state: %p", *data);

    pfa_free_contiguous_pages((void*)*data, xsave_area_pages);
    (*data) = NULL;
}
