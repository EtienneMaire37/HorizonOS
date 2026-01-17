#include "../../libc/include/stdint.h"
#include "../../libc/include/string.h"
#include "../../libc/include/stdlib.h"
#include <stdbool.h>

uint16_t fpu_test;

bool has_fpu;

uint8_t* fpu_default_state = NULL;
uint32_t xsave_area_size, xsave_area_pages;

#include "fpu.h"
#include "sse.h"

#include "../memalloc/page_frame_allocator.h"
#include "../debug/out.h"
#include "../../libc/include/assert.h"

void fpu_init_defaults()
{
    xsave_area_size = get_xsave_area_size();
    xsave_area_pages = (xsave_area_size + 0xfff) / 0x1000;

    LOG(DEBUG, "XSAVE area is %u bytes long (%u page%s)", xsave_area_size, xsave_area_pages, xsave_area_pages == 1 ? "" : "s");

    fpu_default_state = pfa_allocate_contiguous_pages(xsave_area_pages);
    assert(((uintptr_t)fpu_default_state & 0xfff) == 0);
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
    asm volatile("xsaves [rdi]" :: "a"(STATE_COMPONENT_BITMAP & 0xffffffff), "D"(s), "d"(STATE_COMPONENT_BITMAP >> 32) : "memory");
}

void fpu_restore_state(uint8_t* s)
{
    asm volatile("xrstors [rdi]" :: "a"(STATE_COMPONENT_BITMAP & 0xffffffff), "D" (s), "d"(STATE_COMPONENT_BITMAP >> 32) : "memory");
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