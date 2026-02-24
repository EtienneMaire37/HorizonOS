#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../util/defs.h"

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

void fpu_init_defaults();
void fpu_init();
void fpu_save_state(uint8_t* fpu_state);
void fpu_restore_state(uint8_t* fpu_state);
void fpu_state_init(uint8_t* fpu_state);
uint8_t* fpu_state_create();
void fpu_state_destroy(uint8_t** data);
uint8_t* fpu_state_create_copy(uint8_t* state);