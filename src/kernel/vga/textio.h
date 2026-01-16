#pragma once

#include "../files/psf.h"
#include "../../libc/include/termios.h"
#include "constants.h"

static inline bool is_printable_character(char c)
{
    return ((unsigned char)c >= 32) && ((unsigned char)c < 128);
}

extern uint16_t tty_data[TTY_RES_X * TTY_RES_Y];

extern uint32_t tty_cursor;
extern uint8_t tty_color;

extern struct termios tty_ts;

extern pid_t tty_foreground_pgrp;

extern bool tty_cursor_blink;

extern psf_font_t tty_font;

extern const uint32_t tty_padding;	// pixels

extern uint8_t tty_control_sequence_buffer[TTY_ANSI_BUFFER];
extern uint8_t tty_escape_sequence_index;
extern bool tty_reading_escape_sequence, tty_reading_control_sequence;

void tty_clear_screen(char c);
void tty_set_color(uint8_t fg_color, uint8_t bg_color);
void tty_outc(char c);
void tty_render_cursor(uint32_t cursor);
void tty_render_character(uint32_t cursor, char c, uint8_t color);