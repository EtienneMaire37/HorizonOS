#pragma once

#include <stdint.h>
#include <stdbool.h>

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