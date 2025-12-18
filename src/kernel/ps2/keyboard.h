#pragma once

#include "ps2.h"

#define PS2_KB_KEY_DETECTION_ERROR_0     0x00
#define PS2_KB_INTERNAL_BUFFER_OVERRUN_0 0x00
#define PS2_KB_SELF_TEST_PASSED          0xaa
#define PS2_KB_ECHO                      0xee
#define PS2_KB_SELF_TEST_FAILED_0        0xfc
#define PS2_KB_SELF_TEST_FAILED_1        0xfd
#define PS2_KB_KEY_DETECTION_ERROR_1     0xff
#define PS2_KB_INTERNAL_BUFFER_OVERRUN_1 0xff

#define PS2_KB_SET_LED_STATE            0xed
#define PS2_KB_ECHO                     0xee
#define PS2_KB_GET_SET_SCANCODE_SET     0xf0
#define PS2_KB_SET_TYPEMATIC_BYTE       0xf3
#define PS2_KB_SET_DEFAULT_PARAMETERS   0xf6

#define PS2_DEVICE_1_KB   (ps2_device_1_connected && ps2_device_1_type == PS2_DEVICE_KEYBOARD)
#define PS2_DEVICE_2_KB   (ps2_device_2_connected && ps2_device_2_type == PS2_DEVICE_KEYBOARD)

uint8_t ps2_keyboard_state[256] = 
{
    0
};
uint8_t ps2_keyboard_state_e0[256] = 
{
    0
};

bool ps2_kb_caps_lock[2] = { false, false }, ps2_kb_num_lock[2] = { false, false }, ps2_kb_scroll_lock[2] = { false, false };

struct ps2_full_scancode;

typedef struct ps2_full_scancode ps2_full_scancode_t;

ps2_full_scancode_t current_ps2_keyboard_scancodes[2] = { { 0, 0, 0 }, { 0, 0, 0 } };

bool enable_ps2_kb_input;

uint8_t ps2_kb_1_scancode_set, ps2_kb_2_scancode_set;

void ps2_init_keyboards();
void ps2_handle_keyboard_scancode(uint8_t port, uint8_t scancode, bool* task_switch);
void ps2_kb_get_scancode_set();
bool ps2_kb_is_key_pressed(virtual_key_t vk);
bool ps2_kb_is_key_pressed_with_port(virtual_key_t vk, uint8_t port);
utf32_char_t ps2_scancode_to_unicode(ps2_full_scancode_t scancode, uint8_t port);
void ps2_kb_update_leds(uint8_t port);