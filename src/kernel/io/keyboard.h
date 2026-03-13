#pragma once

#include "../cpu/util.h"
#include "../util/math.h"
#include <termios.h>

typedef uint32_t utf32_char_t;

typedef struct utf32_buffer
{
    utf32_char_t* characters;
    size_t size;
    size_t put_index, get_index;
} utf32_buffer_t;

typedef enum virtual_key 
{
    VK_INVALID = 0,
    VK_ESCAPE,
    VK_1, VK_2, VK_3, VK_4, VK_5, VK_6, VK_7, VK_8, VK_9, VK_0,
    VK_MINUS, VK_EQUALS, VK_BACKSPACE, VK_TAB,
    VK_Q, VK_W, VK_E, VK_R, VK_T, VK_Y, VK_U, VK_I, VK_O, VK_P,
    VK_LBRACKET, VK_RBRACKET, VK_ENTER,
    VK_LCONTROL, VK_A, VK_S, VK_D, VK_F, VK_G, VK_H, VK_J, VK_K, VK_L,
    VK_SEMICOLON, VK_APOSTROPHE, VK_GRAVE,
    VK_LSHIFT, VK_BACKSLASH,
    VK_Z, VK_X, VK_C, VK_V, VK_B, VK_N, VK_M,
    VK_COMMA, VK_PERIOD, VK_SLASH, VK_RSHIFT,
    VK_KP_MULTIPLY, VK_LALT, VK_SPACE, VK_CAPSLOCK,
    VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6,
    VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
    VK_PRINTSCREEN, VK_PAUSE,
    VK_LWIN, VK_RWIN, VK_APPS, VK_NUMLOCK,
    VK_SCROLLLOCK, VK_KP_7, VK_KP_8, VK_KP_9, VK_KP_MINUS,
    VK_KP_4, VK_KP_5, VK_KP_6, VK_KP_PLUS, VK_KP_1,
    VK_KP_2, VK_KP_3, VK_KP_0, VK_KP_PERIOD,
    
    VK_RCONTROL, VK_RALT,
    VK_HOME, VK_UP, VK_PAGEUP, VK_LEFT, VK_RIGHT, VK_END,
    VK_DOWN, VK_PAGEDOWN, VK_INSERT, VK_DELETE,
    VK_KP_ENTER, VK_KP_DIVIDE,
} virtual_key_t;

typedef struct ps2_keyboard_layout
{
    virtual_key_t vk_table[256];
    virtual_key_t vk_table_e0[256];
    utf32_char_t char_table[256];
    utf32_char_t char_table_e0[256];
    utf32_char_t char_table_shift[256];
    utf32_char_t char_table_altgr[256];
} ps2_keyboard_layout_t;

typedef struct keyboard_layout
{
    ps2_keyboard_layout_t ps2_layout_data;
} keyboard_layout_t;

typedef struct ps2_full_scancode
{
    bool extended, release;
    uint8_t scancode;
} ps2_full_scancode_t;

static const uint8_t extended_numlock_map[256] = 
{
    [0x70] = 0x70,   // Insert → Numpad 0
    [0x69] = 0x69,   // End → Numpad 1
    [0x72] = 0x72,   // Down → Numpad 2
    [0x7A] = 0x7A,   // Page Down → Numpad 3
    [0x6B] = 0x6B,   // Left → Numpad 4
    [0x73] = 0x73,   // Center → Numpad 5
    [0x74] = 0x74,   // Right → Numpad 6
    [0x6C] = 0x6C,   // Home → Numpad 7
    [0x75] = 0x75,   // Up → Numpad 8
    [0x7D] = 0x7D,   // Page Up → Numpad 9
    [0x71] = 0x71,   // Delete → Numpad .
};

#define NUM_KB_LAYOUTS  2

static const keyboard_layout_t us_qwerty = 
{
    .ps2_layout_data = 
    {
        .vk_table = 
        {
            [0x76] = VK_ESCAPE,    [0x16] = VK_1,        [0x1E] = VK_2,
            [0x26] = VK_3,         [0x25] = VK_4,        [0x2E] = VK_5,
            [0x36] = VK_6,         [0x3D] = VK_7,        [0x3E] = VK_8,
            [0x46] = VK_9,         [0x45] = VK_0,        [0x4E] = VK_MINUS,
            [0x55] = VK_EQUALS,    [0x66] = VK_BACKSPACE,[0x0D] = VK_TAB,
            [0x15] = VK_Q,         [0x1D] = VK_W,        [0x24] = VK_E,
            [0x2D] = VK_R,         [0x2C] = VK_T,        [0x35] = VK_Y,
            [0x3C] = VK_U,         [0x43] = VK_I,        [0x44] = VK_O,
            [0x4D] = VK_P,         [0x54] = VK_LBRACKET, [0x5B] = VK_RBRACKET,
            [0x5A] = VK_ENTER,     [0x14] = VK_LCONTROL, [0x1C] = VK_A,
            [0x1B] = VK_S,         [0x23] = VK_D,        [0x2B] = VK_F,
            [0x34] = VK_G,         [0x33] = VK_H,        [0x3B] = VK_J,
            [0x42] = VK_K,         [0x4B] = VK_L,        [0x4C] = VK_SEMICOLON,
            [0x52] = VK_APOSTROPHE,[0x0E] = VK_GRAVE,   [0x12] = VK_LSHIFT,
            [0x5D] = VK_BACKSLASH, [0x1A] = VK_Z,        [0x22] = VK_X,
            [0x21] = VK_C,         [0x2A] = VK_V,        [0x32] = VK_B,
            [0x31] = VK_N,         [0x3A] = VK_M,        [0x41] = VK_COMMA,
            [0x49] = VK_PERIOD,    [0x4A] = VK_SLASH,   [0x59] = VK_RSHIFT,
            [0x7C] = VK_KP_MULTIPLY,[0x11] = VK_LALT,   [0x29] = VK_SPACE,
            [0x58] = VK_CAPSLOCK,  [0x05] = VK_F1,       [0x06] = VK_F2,
            [0x04] = VK_F3,        [0x0C] = VK_F4,       [0x03] = VK_F5,
            [0x0B] = VK_F6,        [0x83] = VK_F7,       [0x0A] = VK_F8,
            [0x01] = VK_F9,        [0x09] = VK_F10, 
            [0x77] = VK_NUMLOCK,   [0x7E] = VK_SCROLLLOCK,
            [0x78] = VK_F11,        [0x07] = VK_F12,
        },
        .vk_table_e0 = 
        {
            [0x14] = VK_RCONTROL,  [0x11] = VK_RALT,     [0x6C] = VK_HOME,
            [0x75] = VK_UP,        [0x7D] = VK_PAGEUP,  [0x6B] = VK_LEFT,
            [0x74] = VK_RIGHT,     [0x69] = VK_END,      [0x72] = VK_DOWN,
            [0x7A] = VK_PAGEDOWN,  [0x70] = VK_INSERT,   [0x71] = VK_DELETE,
            [0x5A] = VK_KP_ENTER,  [0x4A] = VK_KP_DIVIDE, 
            [0x7C] = VK_PRINTSCREEN, [0x1F] = VK_LWIN,
            [0x27] = VK_RWIN,       [0x2F] = VK_APPS,
        },
        .char_table = 
        {
            [0x76] = 0x1b,    // Escape
            [0x0D] = U'\t',       // Tab
            [0x66] = U'\b',       // Backspace

            [0x1C] = U'a',        [0x15] = U'q',        [0x1D] = U'w',
            [0x24] = U'e',        [0x2D] = U'r',        [0x2C] = U't',
            [0x35] = U'y',        [0x3C] = U'u',        [0x43] = U'i',
            [0x44] = U'o',        [0x4D] = U'p',        [0x54] = U'[',
            [0x5B] = U']',        [0x5A] = U'\n',       [0x1B] = U's',
            [0x23] = U'd',        [0x2B] = U'f',        [0x34] = U'g',
            [0x33] = U'h',        [0x3B] = U'j',        [0x42] = U'k',
            [0x4B] = U'l',        [0x4C] = U';',        [0x52] = U'\'',
            [0x0E] = U'`',        [0x1A] = U'z',        [0x22] = U'x',
            [0x21] = U'c',        [0x2A] = U'v',        [0x32] = U'b',
            [0x31] = U'n',        [0x3A] = U'm',        [0x41] = U',',
            [0x49] = U'.',        [0x4A] = U'/',        [0x16] = U'1',
            [0x1E] = U'2',        [0x26] = U'3',        [0x25] = U'4',
            [0x2E] = U'5',        [0x36] = U'6',        [0x3D] = U'7',
            [0x3E] = U'8',        [0x46] = U'9',        [0x45] = U'0',
            [0x4E] = U'_',        [0x55] = U'=',        [0x5D] = U'\\',
            [0x7C] = U'*',        [0x7E] = U' ',        [0x29] = U' ',
            [0x61] = U'\\',

            [0x70] = U'0', [0x69] = U'1', [0x72] = U'2',
            [0x7A] = U'3', [0x6B] = U'4', [0x73] = U'5',
            [0x74] = U'6', [0x6C] = U'7', [0x75] = U'8',
            [0x7D] = U'9', [0x71] = U'.',
        },
        .char_table_e0 = 
        {
            [0x5A] = U'\n',
            [0x71] = 0x7f,
            [0x4A] = U'/'
        },
        .char_table_shift = 
        {
            [0x76] = 0x1b,
            [0x0D] = U'\t',
            [0x66] = U'\b',

            [0x1C] = U'A',        [0x15] = U'Q',        [0x1D] = U'W',
            [0x24] = U'E',        [0x2D] = U'R',        [0x2C] = U'T',
            [0x35] = U'Y',        [0x3C] = U'U',        [0x43] = U'I',
            [0x44] = U'O',        [0x4D] = U'P',        [0x54] = U'{',
            [0x5B] = U'}',        [0x5A] = U'\n',       [0x1B] = U'S',
            [0x23] = U'D',        [0x2B] = U'F',        [0x34] = U'G',
            [0x33] = U'H',        [0x3B] = U'J',        [0x42] = U'K',
            [0x4B] = U'L',        [0x4C] = U':',        [0x52] = U'"',
            [0x0E] = U'~',        [0x1A] = U'Z',        [0x22] = U'X',
            [0x21] = U'C',        [0x2A] = U'V',        [0x32] = U'B',
            [0x31] = U'N',        [0x3A] = U'M',        [0x41] = U'<',
            [0x49] = U'>',        [0x4A] = U'?',        [0x16] = U'!',
            [0x1E] = U'@',        [0x26] = U'#',        [0x25] = U'$',
            [0x2E] = U'%',        [0x36] = U'^',        [0x3D] = U'&',
            [0x3E] = U'*',        [0x46] = U'(',        [0x45] = U')',
            [0x4E] = U'_',        [0x55] = U'+',        [0x5D] = U'|',
            [0x7C] = U'*',        [0x29] = U' ',        [0x61] = U'|',
        },
        .char_table_altgr = 
        { 0 },
    }
};

static const keyboard_layout_t fr_azerty = 
{
    .ps2_layout_data = 
    {
        .vk_table = 
        {
            [0x76] = VK_ESCAPE,    [0x16] = VK_1,        [0x1E] = VK_2,
            [0x26] = VK_3,         [0x25] = VK_4,        [0x2E] = VK_5,
            [0x36] = VK_6,         [0x3D] = VK_7,        [0x3E] = VK_8,
            [0x46] = VK_9,         [0x45] = VK_0,        [0x4E] = VK_MINUS,
            [0x55] = VK_EQUALS,    [0x66] = VK_BACKSPACE,[0x0D] = VK_TAB,
            [0x15] = VK_Q,         [0x1D] = VK_W,        [0x24] = VK_E,
            [0x2D] = VK_R,         [0x2C] = VK_T,        [0x35] = VK_Y,
            [0x3C] = VK_U,         [0x43] = VK_I,        [0x44] = VK_O,
            [0x4D] = VK_P,         [0x54] = VK_LBRACKET, [0x5B] = VK_RBRACKET,
            [0x5A] = VK_ENTER,     [0x14] = VK_LCONTROL, [0x1C] = VK_A,
            [0x1B] = VK_S,         [0x23] = VK_D,        [0x2B] = VK_F,
            [0x34] = VK_G,         [0x33] = VK_H,        [0x3B] = VK_J,
            [0x42] = VK_K,         [0x4B] = VK_L,        [0x4C] = VK_SEMICOLON,
            [0x52] = VK_APOSTROPHE,[0x0E] = VK_GRAVE,    [0x12] = VK_LSHIFT,
            [0x5D] = VK_BACKSLASH, [0x1A] = VK_Z,        [0x22] = VK_X,
            [0x21] = VK_C,         [0x2A] = VK_V,        [0x32] = VK_B,
            [0x31] = VK_N,         [0x3A] = VK_M,        [0x41] = VK_COMMA,
            [0x49] = VK_PERIOD,    [0x4A] = VK_SLASH,    [0x59] = VK_RSHIFT,
            [0x7C] = VK_KP_MULTIPLY,[0x11] = VK_LALT,    [0x29] = VK_SPACE,
            [0x58] = VK_CAPSLOCK,  [0x05] = VK_F1,       [0x06] = VK_F2,
            [0x04] = VK_F3,        [0x0C] = VK_F4,       [0x03] = VK_F5,
            [0x0B] = VK_F6,        [0x83] = VK_F7,       [0x0A] = VK_F8,
            [0x01] = VK_F9,        [0x09] = VK_F10,      [0x77] = VK_NUMLOCK,
            [0x7E] = VK_SCROLLLOCK,[0x78] = VK_F11,      [0x07] = VK_F12,
        },
        .vk_table_e0 = 
        {
            [0x14] = VK_RCONTROL,  [0x11] = VK_RALT,     [0x6C] = VK_HOME,
            [0x75] = VK_UP,        [0x7D] = VK_PAGEUP,  [0x6B] = VK_LEFT,
            [0x74] = VK_RIGHT,     [0x69] = VK_END,      [0x72] = VK_DOWN,
            [0x7A] = VK_PAGEDOWN,  [0x70] = VK_INSERT,   [0x71] = VK_DELETE,
            [0x5A] = VK_KP_ENTER,  [0x4A] = VK_KP_DIVIDE, 
            [0x7C] = VK_PRINTSCREEN, [0x1F] = VK_LWIN,
            [0x27] = VK_RWIN,       [0x2F] = VK_APPS,
        },
        .char_table = 
        {
            [0x16] = U'&',        [0x1E] = U'é',        [0x26] = U'"',
            [0x25] = U'\'',       [0x2E] = U'(',        [0x36] = U'-',
            [0x3D] = U'è',        [0x3E] = U'_',        [0x46] = U'ç',
            [0x45] = U'à',        [0x4E] = U')',        [0x55] = U'=',
            [0x0E] = U'²',        [0x15] = U'a',        [0x1D] = U'z',
            [0x24] = U'e',        [0x2D] = U'r',        [0x2C] = U't',
            [0x35] = U'y',        [0x3C] = U'u',        [0x43] = U'i',
            [0x44] = U'o',        [0x4D] = U'p',        [0x54] = U'^',
            [0x5B] = U'$',        [0x5A] = U'\n',       [0x1C] = U'q',
            [0x1B] = U's',        [0x23] = U'd',        [0x2B] = U'f',
            [0x34] = U'g',        [0x33] = U'h',        [0x3B] = U'j',
            [0x42] = U'k',        [0x4B] = U'l',        [0x4C] = U'm',
            [0x52] = U'ù',       [0x5D] = U'*',        [0x1A] = U'w',
            [0x22] = U'x',        [0x21] = U'c',        [0x2A] = U'v',
            [0x32] = U'b',        [0x31] = U'n',        [0x3A] = U',',
            [0x41] = U';',        [0x49] = U':',        [0x4A] = U'!',
            [0x76] = U'\x1B',    [0x0D] = U'\t',       [0x66] = U'\b',
            [0x29] = U' ',        [0x70] = U'0',        [0x69] = U'1',
            [0x72] = U'2',        [0x7A] = U'3',        [0x6B] = U'4',
            [0x73] = U'5',        [0x74] = U'6',        [0x6C] = U'7',
            [0x75] = U'8',        [0x7D] = U'9',        [0x71] = U'.',
            [0x61] = U'<',
            [0x7B] = U'-',        [0x79] = U'+',        [0x7c] = U'*'
        },
        .char_table_e0 = 
        {
            [0x4A] = U'/',        [0x5A] = '\n'
        },
        .char_table_shift = 
        {
            [0x16] = U'1',        [0x1E] = U'2',        [0x26] = U'3',
            [0x25] = U'4',        [0x2E] = U'5',        [0x36] = U'6',
            [0x3D] = U'7',        [0x3E] = U'8',        [0x46] = U'9',
            [0x45] = U'0',        [0x4E] = U'°',        [0x55] = U'+',
            [0x0E] = U'³',        [0x15] = U'A',        [0x1D] = U'Z',
            [0x24] = U'E',        [0x2D] = U'R',        [0x2C] = U'T',
            [0x35] = U'Y',        [0x3C] = U'U',        [0x43] = U'I',
            [0x44] = U'O',        [0x4D] = U'P',        [0x54] = U'¨',
            [0x5B] = U'£',        [0x4C] = U'M',        [0x52] = U'%',
            [0x3A] = U'?',        [0x41] = U'.',        [0x49] = U'/',
            [0x4A] = U'§',        [0x76] = U'\x1B',    [0x0D] = U'\t',
            [0x66] = U'\b',       [0x29] = U' ',
            [0x61] = U'>',
        },
        .char_table_altgr = 
        {
            [0x1E] = U'~',        [0x26] = U'#',        [0x25] = U'{',
            [0x2E] = U'[',        [0x36] = U'|',        [0x3D] = U'`',
            [0x3E] = U'\\',       [0x46] = U'^',        [0x45] = U'@',
            [0x4E] = U']',        [0x55] = U'}',        [0x24] = U'€',
            [0x54] = U'^',        [0x5B] = U'¤',        [0x52] = U'µ',
            [0x4C] = U'%',        [0x3A] = U'.',        [0x41] = U',',
            [0x49] = U';',        [0x4A] = U'!',        [0x2D] = U'¶',
            [0x32] = U'ß',        [0x43] = U'î',        [0x44] = U'ô',
            [0x35] = U'ÿ',        [0x31] = U'ñ',        [0x21] = U'©',
            [0x22] = U'×',        [0x2A] = U'√',        [0x1A] = U'Ω',
            [0x61] = U'|',
        }
    }
};

static const keyboard_layout_t* keyboard_layouts[NUM_KB_LAYOUTS] = 
{
    &us_qwerty,
    &fr_azerty
};

extern const keyboard_layout_t* current_keyboard_layout;  // TODO: Make it so we can have one layout per keyboard

#define get_buffered_characters(buffer) ((size_t)imod(((int)(buffer).put_index - (buffer).get_index), (buffer).size))
#define no_buffered_characters(buffer)  ((buffer).put_index == (buffer).get_index)

static inline char utf32_to_bios_oem(utf32_char_t ch)
{
    return ch < 128 ? (char)ch : (char)0;
}

void utf32_buffer_init(utf32_buffer_t* buffer);
void utf32_buffer_clear(utf32_buffer_t* buffer);
void utf32_buffer_destroy(utf32_buffer_t* buffer);
void utf32_buffer_putchar(utf32_buffer_t* buffer, utf32_char_t character);
utf32_char_t utf32_buffer_getchar(utf32_buffer_t* buffer);
void utf32_buffer_create_and_copy(const utf32_buffer_t* from, utf32_buffer_t* to);

bool keyboard_is_key_pressed(virtual_address_t vk);
void keyboard_handle_character(utf32_char_t character, virtual_key_t vk, struct termios* ts);
