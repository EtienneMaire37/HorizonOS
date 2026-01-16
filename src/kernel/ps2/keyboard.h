#pragma once

#include "ps2.h"
#include "../io/keyboard.h"

extern uint8_t ps2_keyboard_state[256];
extern uint8_t ps2_keyboard_state_e0[256];

extern bool ps2_kb_caps_lock[2], ps2_kb_num_lock[2], ps2_kb_scroll_lock[2];

extern ps2_full_scancode_t current_ps2_keyboard_scancodes[2];

extern bool enable_ps2_kb_input;

extern uint8_t ps2_kb_1_scancode_set, ps2_kb_2_scancode_set;

void ps2_init_keyboards();
void ps2_handle_keyboard_scancode(uint8_t port, uint8_t scancode, bool* task_switch);
void ps2_kb_get_scancode_set();
bool ps2_kb_is_key_pressed(virtual_key_t vk);
bool ps2_kb_is_key_pressed_with_port(virtual_key_t vk, uint8_t port);
utf32_char_t ps2_scancode_to_unicode(ps2_full_scancode_t scancode, uint8_t port);
void ps2_kb_update_leds(uint8_t port);