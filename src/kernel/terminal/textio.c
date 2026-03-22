#include "../files/psf.h"
#include <termios.h>
#include "../vga/constants.h"
#include "../multitasking/multitasking.h"
#include "textio.h"

uint16_t tty_data[MAX_TTY_X * MAX_TTY_Y];

uint32_t tty_res_x = 0, tty_res_y = 0;

uint32_t tty_cursor = 0;
uint8_t tty_color;

struct termios tty_ts;

pid_t tty_foreground_pgrp = 1;

bool tty_cursor_blink = true;

psf_font_t tty_font;

uint32_t tty_control_sequence_buffer[TTY_ESC_BUFFER] = {0};
uint8_t tty_escape_sequence_index = 0;
bool tty_reading_escape_sequence = false, tty_reading_control_sequence = false;
bool tty_sequence_question_mark = false;

char tty_osc_buffer[TTY_OSC_BUFFER];
int tty_osc_index = 0;
bool tty_reading_operating_system_command = false, tty_reading_operating_system_command_string = false;

mutex_t termout_mutex = MUTEX_INIT;

#include "textio.h"
#include "../vga/vga.h"
#include "../graphics/linear_framebuffer.h"

void tty_init()
{
	tty_set_window_size(framebuffer.width / 8, framebuffer.height / (8 * TTY_AR));
    tty_color = FG_WHITE | BG_BLACK;
	tty_clear_screen(' ');
}

void tty_clear_screen(char c)
{
    lock_scheduler();
    __tty_clear_screen(c);
    unlock_scheduler();
}

void __tty_clear_screen(char c)
{
	for (uint32_t i = 0; i < MAX_TTY_X * tty_res_y; i++)
		if (i % MAX_TTY_X < tty_res_x) tty_data[i] = ((uint16_t)tty_color << 8) | ' ';
	if (c == 0 || c == ' ')
	{
		srgb_t bg_color = vga_get_bg_color(tty_color);
		framebuffer_fill_rect(&framebuffer, 0, 0, framebuffer.width, framebuffer.height, bg_color.r, bg_color.g, bg_color.b);
		tty_cursor = 0;
		return;
	}
	for (uint32_t i = 0; i < MAX_TTY_X * tty_res_y; i++)
		if (i % MAX_TTY_X < tty_res_x) __tty_render_character(i, c, tty_color);
	tty_cursor = 0;
}

void __tty_clear_section(uint32_t start_char, uint32_t end_char, uint8_t clear_color)
{
	for (uint32_t i = start_char; i < end_char; i++)
		if (i % MAX_TTY_X < tty_res_x) tty_data[i] = ((uint16_t)clear_color << 8) | ' ';
	for (uint32_t i = start_char; i < end_char; i++)
		if (i % MAX_TTY_X < tty_res_x) __tty_render_character(i, ' ', clear_color);
	if (tty_cursor_blink)
		__tty_render_cursor(tty_cursor);
}

void __tty_move_characters(uint32_t start, int offset)
{
	size_t bytes = offset > 0 ? MAX_TTY_X - ((start + offset) % MAX_TTY_X) : MAX_TTY_X - (start % MAX_TTY_X);
	memmove(&tty_data[start + offset], &tty_data[start], bytes);
	uint32_t base = offset > 0 ? start : start + offset;
	for (uint32_t i = base; i < base + bytes; i++)
		if (i % MAX_TTY_X < tty_res_x) __tty_render_character(i, tty_data[i], tty_data[i] >> 8);
	if (tty_cursor_blink)
		__tty_render_cursor(tty_cursor);
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
	fflush(stdout);
	lock_scheduler();

#ifndef LOG_TO_TTY
	tty_color = (fg_color & 0x0f) | (bg_color & 0xf0);
#endif
    unlock_scheduler();
}

void tty_set_window_size(int sx, int sy)
{
	LOG(DEBUG, "tty_set_window_size(%d, %d)", sx, sy);
	if (sx > MAX_TTY_X || sy > MAX_TTY_Y || sx <= 0 || sy <= 0)
	{
		sx = MAX_TTY_X;
		sy = sx / TTY_AR;
	}
	lock_scheduler();
	tty_res_x = sx;
	tty_res_y = sy;
	unlock_scheduler();
	__tty_refresh_screen();
}

uint32_t __tty_get_character_width()
{
	uint32_t ret = framebuffer.width / tty_res_x;
	return ret;
}

uint32_t __tty_get_character_height()
{
	uint32_t ret = framebuffer.height / tty_res_y;
	return ret;
}

uint32_t __tty_get_character_pos_x(uint32_t index)
{
	uint32_t ret = __tty_get_character_width() * (index % MAX_TTY_X);
	return ret;
}

uint32_t __tty_get_character_pos_y(uint32_t index)
{
	uint32_t ret = __tty_get_character_height() * (index / MAX_TTY_X);
	return ret;
}

void __tty_render_character(uint32_t cursor, char c, uint8_t color)
{
	if (cursor >= MAX_TTY_X * tty_res_y)
	    return;

	uint32_t width = __tty_get_character_width();
	uint32_t height = __tty_get_character_height();

	uint32_t x = __tty_get_character_pos_x(cursor);
	uint32_t y = __tty_get_character_pos_y(cursor);

	srgb_t bg_color = vga_get_bg_color(color);

	if (!is_printable_character(c))
	{
		framebuffer_fill_rect(&framebuffer, x, y, width, height, bg_color.r, bg_color.g, bg_color.b);
		return;
	}

	srgb_t fg_color = vga_get_fg_color(color);
	framebuffer_render_psf2_char(&framebuffer, x, y, width, height, &tty_font, c,
		fg_color.r,
		fg_color.g,
		fg_color.b,
		false, bg_color.r, bg_color.g, bg_color.b);
}

void __tty_render_cursor(uint32_t cursor)
{
	if (cursor >= MAX_TTY_X * tty_res_y)
		return;
	if (cursor % MAX_TTY_X >= tty_res_x)
		return;

	uint32_t width = __tty_get_character_width();
	uint32_t height = __tty_get_character_height();

	uint32_t x = __tty_get_character_pos_x(cursor);
	uint32_t y = __tty_get_character_pos_y(cursor);

	srgb_t cursor_col = theme_cursor_color;

	framebuffer_fill_rect(&framebuffer, x, y + (8 * height / 10), width, height / 5, cursor_col.r, cursor_col.g, cursor_col.b);
}

void __tty_refresh_screen()
{
	for (uint32_t i = 0; i < MAX_TTY_X * tty_res_y; i++)
		if (i % MAX_TTY_X < tty_res_x) __tty_render_character(i, tty_data[i] & 0x7f, tty_data[i] >> 8);
	if (tty_cursor_blink)
		__tty_render_cursor(tty_cursor);
}

void __tty_ansi_m_code(uint32_t code)
{
	if (tty_sequence_question_mark)
		return;

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

void __tty_ansi_J_code(uint32_t code)
{
	if (tty_sequence_question_mark)
		return;

	if (code != 0 && code != 2 && code != 3)
		return;

	__tty_clear_section(
		(code == 0 ? tty_cursor :
		(code == 1 ? 0 :
		(code == 2 ? 0 :
		(code == 3 ? 0 : 0)))),

		(code == 0 ? MAX_TTY_X * tty_res_y :
		(code == 1 ? tty_cursor :
		(code == 2 ? MAX_TTY_X * tty_res_y :
		(code == 3 ? MAX_TTY_X * tty_res_y : MAX_TTY_X * tty_res_y)))), FG_WHITE | BG_BLACK);
}

void __tty_ansi_H_code(uint32_t code)
{
	if (tty_sequence_question_mark)
		return;

	if (code == 0)
	{
		uint16_t old_data = tty_data[tty_cursor];
		__tty_render_character(tty_cursor, old_data & 0x7f, old_data >> 8);

		tty_cursor = 0;
		if (tty_cursor_blink)
			__tty_render_cursor(tty_cursor);

		return;
	}

	return;
}

void __tty_ansi_h_code(uint32_t code)
{
	if (tty_sequence_question_mark)
	{
		if (code == 2004)
		{
			// ? bracketed paste on
			return;
		}

		return;
	}

	return;
}

void __tty_ansi_l_code(uint32_t code)
{
	if (tty_sequence_question_mark)
	{
		if (code == 2004)
		{
			// ? bracketed paste off
			return;
		}

		return;
	}

	return;
}

void __tty_ansi_K_code(uint32_t code)
{
	if (tty_sequence_question_mark)
		return;

	if (code != 0 && code != 1 && code != 2)
		return;

	__tty_clear_section(	 code == 0 ? tty_cursor :
						(code == 1 ? (tty_cursor / MAX_TTY_X) * MAX_TTY_X :
						(code == 2 ? (tty_cursor / MAX_TTY_X) * MAX_TTY_X : 0)),
						code == 0 ? ((tty_cursor + MAX_TTY_X) / MAX_TTY_X) * MAX_TTY_X :
						(code == 1 ? tty_cursor :
						(code == 2 ? ((tty_cursor + MAX_TTY_X) / MAX_TTY_X) * MAX_TTY_X : 0)),
						FG_WHITE | BG_BLACK);
}

void __tty_ansi_P_code(uint32_t code)
{
	if (tty_sequence_question_mark)
		return;

	if (code == 0)
		code = 1;

	__tty_move_characters(tty_cursor + code, -code);
}

void __tty_ansi_at_code(uint32_t code)
{
	if (tty_sequence_question_mark)
		return;

	if (code == 0)
		code = 1;

	__tty_move_characters(tty_cursor, code);
}

void __tty_osc_put(char c)
{
    if (tty_osc_index > TTY_OSC_BUFFER - 1)
    {
    handle_osc:
        tty_reading_operating_system_command_string = false;
    }
    else
    {
        if (tty_osc_index >= 1)
        {
            if (tty_osc_buffer[tty_osc_index - 1] == '\x1b' && c == '\\')
            {
                tty_osc_buffer[tty_osc_index - 1] = 0;
                goto handle_osc;
            }
        }
        tty_osc_buffer[tty_osc_index++] = c;
    }
}

void tty_outc(char c)
{
	if (!tty_font.f)
		return;
	if (c == 0)
		return;

	lock_scheduler();

	if (tty_cursor >= MAX_TTY_X * tty_res_y)
	{
		tty_cursor++;
		unlock_scheduler();
		return;
	}

	if (c == '\x1b' && !tty_reading_operating_system_command_string)	// * Escape sequence
	{
	#ifndef IGNORE_ANSI
		tty_reading_escape_sequence = true;
		tty_escape_sequence_index = 0;
		tty_osc_index = 0;
		tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
		unlock_scheduler();
		return;
	#else
        unlock_scheduler();
		tty_outc('^');
		return;
	#endif
	}

	if (tty_reading_escape_sequence)
	{
		if (c == '[')
		{
			tty_reading_control_sequence = true;
			tty_reading_escape_sequence = false;
			tty_sequence_question_mark = false;
			unlock_scheduler();
			return;
		}
		if (c == ']')
		{
    		tty_reading_operating_system_command = true;
    		tty_reading_escape_sequence = false;
            unlock_scheduler();
			return;
		}
		tty_reading_escape_sequence = false;
		unlock_scheduler();
		tty_outc('^');
		tty_outc(c);
		return;
	}

	if (tty_reading_control_sequence || tty_reading_operating_system_command)
	{
		if (c >= '0' && c <= '9')
		{
			tty_control_sequence_buffer[tty_escape_sequence_index] *= 10;
			tty_control_sequence_buffer[tty_escape_sequence_index] += c - '0';
			unlock_scheduler();
			return;
		}
		if (c == ';' && tty_reading_operating_system_command)
		{
		    tty_reading_operating_system_command_string = true;
			tty_reading_operating_system_command = false;
			unlock_scheduler();
			return;
		}
		switch (c)
		{
		case '?':
			tty_sequence_question_mark = true;
			break;
		case ';':
			if (tty_escape_sequence_index >= TTY_ESC_BUFFER - 1)
			{
				tty_escape_sequence_index = TTY_ESC_BUFFER - 1;
				memmove(tty_control_sequence_buffer, (void*)((uintptr_t)tty_control_sequence_buffer + 1), TTY_ESC_BUFFER - 1);
			}
			else
			{
				tty_escape_sequence_index++;
				tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			}
			break;
		case 'm':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_m_code(tty_control_sequence_buffer[i]);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'J':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_J_code(tty_control_sequence_buffer[i]);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'H':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_H_code(tty_control_sequence_buffer[i]);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'h':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_h_code(tty_control_sequence_buffer[i]);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'D':
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			tty_outc('\b');
			break;
		case 'C':
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			__tty_render_character(tty_cursor, tty_data[tty_cursor] & 0x7f, tty_data[tty_cursor] >> 8);
			tty_cursor++;
			if (tty_cursor_blink)
				__tty_render_cursor(tty_cursor);
			break;
		case 'l':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_l_code(tty_control_sequence_buffer[i]);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'K':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_K_code(tty_control_sequence_buffer[i]);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'P':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_P_code(tty_control_sequence_buffer[i]);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case '@':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_at_code(tty_control_sequence_buffer[i]);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		default:
			tty_reading_control_sequence = false;
			tty_reading_operating_system_command = false;
			tty_reading_operating_system_command_string = false;
			tty_osc_index = 0;
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

	if (tty_reading_operating_system_command_string)
	{
	    __tty_osc_put(c);
        unlock_scheduler();
        return;
	}

	if (c != '\b' && c != 7 && c != '\n' && c != '\r') tty_data[tty_cursor] = c | ((uint16_t)tty_color << 8);

	__tty_render_character(tty_cursor, tty_data[tty_cursor] & 0x7f, tty_data[tty_cursor] >> 8);

	switch (c)
	{
	case '\n':
		tty_cursor += MAX_TTY_X;
	case '\r':
		tty_cursor /= MAX_TTY_X;
		tty_cursor *= MAX_TTY_X;
		break;

	case '\t':
	{
		__tty_render_character(tty_cursor, c, tty_color);
		tty_cursor = ((tty_cursor - (tty_cursor / MAX_TTY_X) * MAX_TTY_X + TAB_LENGTH) / TAB_LENGTH) * TAB_LENGTH + (tty_cursor / MAX_TTY_X) * MAX_TTY_X;
		break;
	}

	case '\b':
		if (tty_cursor > 0)
			tty_cursor--;
		break;
	case 7: // * DEL
		break;

	default:
		assert(tty_cursor / MAX_TTY_X < tty_res_y);

		tty_cursor++;
		if (tty_cursor % MAX_TTY_X >= tty_res_x)
		{
			tty_cursor -= tty_cursor % MAX_TTY_X;
			tty_cursor += MAX_TTY_X;
		}
	}

	if (tty_cursor_blink)
		__tty_render_cursor(tty_cursor);

	bool scrolled = false;
	while (tty_cursor / MAX_TTY_X >= tty_res_y)
	{
		scrolled = true;

		tty_cursor -= MAX_TTY_X;

		for (uint32_t i = 0; i < tty_res_y - 1; i++)
			memcpy(&tty_data[i * MAX_TTY_X], &tty_data[(i + 1) * MAX_TTY_X], MAX_TTY_X * sizeof(uint16_t));
		memset(&tty_data[(tty_res_y - 1) * MAX_TTY_X], 0x0f, MAX_TTY_X * sizeof(tty_data[0]));
	}
#ifndef DEBUG_SCREEN
	if (scrolled)
#endif
		__tty_refresh_screen();

	unlock_scheduler();
}
