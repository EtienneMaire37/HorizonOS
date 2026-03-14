#pragma once

#include "../files/psf.h"
#include <termios.h>
#include "../vga/constants.h"

static inline bool is_printable_character(char c)
{
    return ((unsigned char)c >= 32) && ((unsigned char)c < 128);
}

#define TAB_LENGTH       4

#define TTY_ANSI_BUFFER     32

#define MAX_TTY_X   1024
#define MAX_TTY_Y   1024

#define TTY_AR      2

extern uint16_t tty_data[MAX_TTY_X * MAX_TTY_Y];

extern uint32_t tty_res_x, tty_res_y;

extern uint32_t tty_cursor;
extern uint8_t tty_color;

extern struct termios tty_ts;

extern pid_t tty_foreground_pgrp;

extern bool tty_cursor_blink;

extern psf_font_t tty_font;

extern uint8_t tty_control_sequence_buffer[TTY_ANSI_BUFFER];
extern uint8_t tty_escape_sequence_index;
extern bool tty_reading_escape_sequence, tty_reading_control_sequence;

void tty_init();
void tty_refresh_screen();
void tty_clear_screen(char c);
void tty_set_color(uint8_t fg_color, uint8_t bg_color);
void tty_set_window_size(int sx, int sy);
void tty_outc(char c);
void tty_render_cursor(uint32_t cursor);
void tty_render_character(uint32_t cursor, char c, uint8_t color);