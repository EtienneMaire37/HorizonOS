#include "../files/psf.h"
#include <termios.h>
#include "constants.h"
#include "../multitasking/multitasking.h"

uint16_t* tty_data = NULL;

uint32_t TTY_RES_X = 0, TTY_RES_Y = 0;

uint32_t tty_cursor = 0;
uint8_t tty_color;

struct termios tty_ts;

pid_t tty_foreground_pgrp = 1;

bool tty_cursor_blink = true;

psf_font_t tty_font;

uint8_t tty_control_sequence_buffer[TTY_ANSI_BUFFER] = {0};
uint8_t tty_escape_sequence_index = 0;
bool tty_reading_escape_sequence = false, tty_reading_control_sequence = false;

#include "textio.h"
#include "vga.h"
#include "../graphics/linear_framebuffer.h"

void tty_init()
{
	tty_set_window_size(framebuffer.width / 10, framebuffer.height / 20);
}

void tty_clear_screen(char c)
{
	lock_scheduler();
	for (uint32_t i = 0; i < TTY_RES_X * TTY_RES_Y; i++)
		tty_data[i] = ((uint16_t)tty_color << 8) | ' ';
	if (c == 0 || c == ' ')
	{
		srgb_t bg_color = vga_get_bg_color(tty_color);
		framebuffer_fill_rect(&framebuffer, 0, 0, framebuffer.width, framebuffer.height, bg_color.r, bg_color.g, bg_color.b);
		tty_cursor = 0;
		goto end;
	}
	tty_cursor = 0;
	for (uint32_t i = 0; i < TTY_RES_X * TTY_RES_Y; i++)
		tty_outc(c);
	tty_cursor = 0;
end:
	unlock_scheduler();
}

uint8_t tty_ansi_to_vga(uint8_t ansi_code) 
{
    switch (ansi_code) 
	{
        case 30: return FG_BLACK;
        case 31: return FG_RED;
        case 32: return FG_GREEN;
        case 33: return FG_BROWN;
        case 34: return FG_BLUE;
        case 35: return FG_MAGENTA;
        case 36: return FG_CYAN;
        case 37: return FG_LIGHTGRAY;

        case 40: return BG_BLACK;
        case 41: return BG_RED;
        case 42: return BG_GREEN;
        case 43: return BG_BROWN;
        case 44: return BG_BLUE;
        case 45: return BG_MAGENTA;
        case 46: return BG_CYAN;
        case 47: return BG_LIGHTGRAY;

        case 90: return FG_DARKGRAY;
        case 91: return FG_LIGHTRED;
        case 92: return FG_LIGHTGREEN;
        case 93: return FG_YELLOW;
        case 94: return FG_LIGHTBLUE;
        case 95: return FG_LIGHTMAGENTA;
        case 96: return FG_LIGHTCYAN;
        case 97: return FG_WHITE;

        case 100: return BG_DARKGRAY;
        case 101: return BG_LIGHTRED;
        case 102: return BG_LIGHTGREEN;
        case 103: return BG_YELLOW;
        case 104: return BG_LIGHTBLUE;
        case 105: return BG_LIGHTMAGENTA;
        case 106: return BG_LIGHTCYAN;
        case 107: return BG_WHITE;
    }

	return FG_WHITE | BG_BLACK;
}

uint8_t tty_ansi_to_vga_mask(uint8_t ansi_code) 
{
	if ((ansi_code >= 30 && ansi_code <= 37) || (ansi_code >= 90 && ansi_code <= 97))
		return 0x0f;
	if ((ansi_code >= 40 && ansi_code <= 47) || (ansi_code >= 100 && ansi_code <= 107))
		return 0xf0;
    return 0xff;
}

void tty_set_color(uint8_t fg_color, uint8_t bg_color)
{
	lock_scheduler();
	fflush(stdout);

#ifndef LOG_TO_TTY
	tty_color = (fg_color & 0x0f) | (bg_color & 0xf0);
#endif
	unlock_scheduler();
}

void tty_set_window_size(int sx, int sy)
{
	LOG(DEBUG, "tty_set_window_size(%d, %d)", sx, sy);
	if (sx >= framebuffer.width || sy >= framebuffer.height)
		return;
	lock_scheduler();
	TTY_RES_X = sx;
	TTY_RES_Y = sy;
	tty_data = realloc(tty_data, TTY_RES_X * TTY_RES_Y * sizeof(tty_data[0]));
	unlock_scheduler();
}

uint32_t tty_get_character_width()
{
	lock_scheduler();
	uint32_t ret = framebuffer.width / TTY_RES_X;
	unlock_scheduler();
	return ret;
}

uint32_t tty_get_character_height()
{
	lock_scheduler();
	uint32_t ret = framebuffer.height / TTY_RES_Y;
	unlock_scheduler();
	return ret;
}

uint32_t tty_get_character_pos_x(uint32_t index)
{
	lock_scheduler();
	uint32_t ret = tty_get_character_width() * (index % TTY_RES_X);
	unlock_scheduler();
	return ret;
}

uint32_t tty_get_character_pos_y(uint32_t index)
{
	lock_scheduler();
	uint32_t ret = tty_get_character_height() * (index / TTY_RES_X);
	unlock_scheduler();
	return ret;
}

void tty_render_character(uint32_t cursor, char c, uint8_t color)
{
	lock_scheduler();
	if (cursor >= TTY_RES_X * TTY_RES_Y) 
	{
		unlock_scheduler();
		return;
	}
	
	uint32_t width = tty_get_character_width();
	uint32_t height = tty_get_character_height();

	uint32_t x = tty_get_character_pos_x(cursor);
	uint32_t y = tty_get_character_pos_y(cursor);

	srgb_t bg_color = vga_get_bg_color(color);

	if (!is_printable_character(c))
	{
		framebuffer_fill_rect(&framebuffer, x, y, width, height, bg_color.r, bg_color.g, bg_color.b);
		unlock_scheduler();
		return;
	}

	srgb_t fg_color = vga_get_fg_color(color);
	framebuffer_render_psf2_char(&framebuffer, x, y, width, height, &tty_font, c,
		fg_color.r,
		fg_color.g,
		fg_color.b,
		false, bg_color.r, bg_color.g, bg_color.b);
	unlock_scheduler();
}

void tty_render_cursor(uint32_t cursor)
{
	lock_scheduler();
	if (cursor >= TTY_RES_X * TTY_RES_Y) 
	{
		unlock_scheduler();
		return;
	}

	uint32_t width = tty_get_character_width();
	uint32_t height = tty_get_character_height();

	uint32_t x = tty_get_character_pos_x(cursor);
	uint32_t y = tty_get_character_pos_y(cursor);

	framebuffer_fill_rect(&framebuffer, x, y + (8 * height / 10), width, height / 5, 255, 255, 255);

	unlock_scheduler();
}

void tty_refresh_screen()
{
	lock_scheduler();
	for (uint32_t i = 0; i < TTY_RES_X * TTY_RES_Y; i++)
		tty_render_character(i, tty_data[i], tty_data[i] >> 8);
	if (tty_cursor_blink)
		tty_render_cursor(tty_cursor);
	unlock_scheduler();
}

void tty_ansi_m_code(uint8_t code)
{
	if (code == 0)
	{
		tty_color = FG_WHITE | BG_BLACK;
		return;
	}

	if (code == 39)
	{
		tty_color = FG_WHITE | (tty_color & 0xf0);
		return;
	}

	if (code == 49)
	{
		tty_color = BG_BLACK | (tty_color & 0x0f);
		return;
	}
	
	uint8_t color_mask = tty_ansi_to_vga_mask(code);
	if (color_mask != 0xff) // * Color code
	{
		tty_color &= ~color_mask;
		tty_color |= tty_ansi_to_vga(code) & color_mask;
		return;
	}

	return;
}

void tty_ansi_J_code(uint8_t code)
{
	if (code == 2)
	{
		tty_color = FG_WHITE | BG_BLACK;
		tty_clear_screen(' ');
		return;
	}

	return;
}

void tty_ansi_H_code(uint8_t code)
{
	if (code == 0)
	{
		uint16_t old_data = tty_data[tty_cursor];
		tty_render_character(tty_cursor, (char)(old_data & 0xff), old_data >> 8);

		tty_cursor = 0;
		if (tty_cursor_blink)
			tty_render_cursor(tty_cursor);
			
		return;
	}

	return;
}

void tty_outc(char c)
{
	if (!tty_font.f)
		return;
	if (c == 0)
		return;

	lock_scheduler();

	if (tty_cursor >= TTY_RES_X * TTY_RES_Y)
	{
		tty_cursor++;
		unlock_scheduler();
		return;
	}

	if (c == '\x1b' )	// * Escape sequence
	{
		tty_reading_escape_sequence = true;
		tty_escape_sequence_index = 0;
		tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
		unlock_scheduler();
		return;
	}

	if (tty_reading_escape_sequence)
	{
		if (c == '[')
		{
			tty_reading_control_sequence = true;
			tty_reading_escape_sequence = false;
			unlock_scheduler();
			return;
		}
		tty_reading_escape_sequence = false;
		tty_outc('^');
		tty_outc(c);
		unlock_scheduler();
		return;
	}

	if (tty_reading_control_sequence)
	{
		if (c >= '0' && c <= '9')
		{
			tty_control_sequence_buffer[tty_escape_sequence_index] *= 10;
			tty_control_sequence_buffer[tty_escape_sequence_index] += c - '0';
			unlock_scheduler();
			return;
		}
		switch (c)
		{
		case ';':
			if (tty_escape_sequence_index >= TTY_ANSI_BUFFER - 1)
			{
				tty_escape_sequence_index = TTY_ANSI_BUFFER - 1;
				memmove(tty_control_sequence_buffer, (void*)((uintptr_t)tty_control_sequence_buffer + 1), TTY_ANSI_BUFFER - 1);
			}
			else
			{
				tty_escape_sequence_index++;
				tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			}
			break;
		case 'm':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				tty_ansi_m_code(tty_control_sequence_buffer[i]);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'J':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				tty_ansi_J_code(tty_control_sequence_buffer[i]);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'H':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				tty_ansi_H_code(tty_control_sequence_buffer[i]);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		default:
			tty_reading_control_sequence = false;
			tty_outc('^');
			tty_outc('[');
			if (!(tty_escape_sequence_index == 0 && tty_control_sequence_buffer[0] == 0))
				for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
					dprintf(STDOUT_FILENO, "%u%s", tty_control_sequence_buffer[i], i == tty_escape_sequence_index ? "" : ";");
			tty_outc(c);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
		}
		unlock_scheduler();
		return;
	}

	tty_data[tty_cursor] = c | ((uint16_t)tty_color << 8);

	tty_render_character(tty_cursor, tty_data[tty_cursor], tty_data[tty_cursor] >> 8);
	
	switch(c)
	{
	case '\n':
		tty_cursor += TTY_RES_X;
	case '\r':
		tty_cursor /= TTY_RES_X;
		tty_cursor *= TTY_RES_X;
		break;

	case '\t':
	{
		tty_render_character(tty_cursor, c, tty_color);
		tty_cursor = ((tty_cursor - (tty_cursor / TTY_RES_X) * TTY_RES_X + TAB_LENGTH) / TAB_LENGTH) * TAB_LENGTH + (tty_cursor / TTY_RES_X) * TTY_RES_X;
		if (tty_cursor_blink)
			tty_render_cursor(tty_cursor);
		break;
	}

	case '\b':
		tty_render_character(tty_cursor, c, tty_color);
		if (tty_cursor > 0)
			tty_cursor--;
		if (tty_cursor_blink)
			tty_render_cursor(tty_cursor);
		break;
	
	default:
		if (tty_cursor / TTY_RES_X >= TTY_RES_Y)
		{
			tty_cursor++;
			break;
		}

		tty_cursor++;
	}

	if (tty_cursor_blink)
		tty_render_cursor(tty_cursor);

	bool scrolled = false;
	while (tty_cursor / TTY_RES_X >= TTY_RES_Y)
	{
		scrolled = true;

		tty_cursor -= TTY_RES_X;

		for (uint32_t i = 0; i < TTY_RES_Y - 1; i++)
			memcpy(&tty_data[i * TTY_RES_X], &tty_data[(i + 1) * TTY_RES_X], TTY_RES_X * sizeof(uint16_t));

		memset(&tty_data[(TTY_RES_Y - 1) * TTY_RES_X], 0x0f, TTY_RES_X * sizeof(uint16_t));
	}
	if (scrolled)
		tty_refresh_screen();

	unlock_scheduler();
}