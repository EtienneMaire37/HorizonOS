#pragma once

#include "../files/psf.h"
#include <termios.h>
#include "../vga/constants.h"
#include "../multitasking/mutex.h"

static inline bool is_printable_character(char c)
{
    return ((unsigned char)c >= 32) && ((unsigned char)c < 128);
}

typedef uint32_t tty_char_t;

#define TAB_LENGTH       4

#define TTY_ESC_BUFFER     32
#define TTY_OSC_BUFFER     128

#define MAX_TTY_X   1024
#define MAX_TTY_Y   1024

#define TTY_AR      2

#define TTY_BOLD            ((tty_char_t)0x10000)
#define TTY_CONTINUE_CHAR   ((tty_char_t)0x20000)
#define TTY_DIRTY           ((tty_char_t)0x40000)

extern uint32_t tty_dirty;

extern uint32_t tty_res_x, tty_res_y;

extern int32_t tty_cursor;
extern uint8_t tty_color;

extern struct termios tty_ts;

extern pid_t tty_foreground_pgrp;

extern bool tty_cursor_blink;

extern psf_font_t tty_font, tty_font_bold;

extern bool tty_application_cursor_mode;

void tty_init(bool refresh);
void __tty_refresh_screen(bool refresh);
void tty_clear_screen(char c, bool refresh);
void __tty_clear_screen(char c, bool refresh);
void __tty_clear_section(uint32_t start_char, uint32_t end_char, uint8_t clear_color, bool refresh);
void tty_set_color(uint8_t fg_color, uint8_t bg_color);
void tty_set_window_size(int sx, int sy, bool refresh);
void tty_outc_ex(char c, int flags, bool refresh);
void tty_outc(char c);
void __tty_render_cursor(uint32_t cursor, bool refresh);
void __tty_render_character(uint32_t cursor, tty_char_t c, bool refresh);
void __tty_move_characters(uint32_t start, int offset, bool refresh);
