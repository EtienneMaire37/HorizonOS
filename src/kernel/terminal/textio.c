#include "../files/psf.h"
#include <termios.h>
#include "../vga/constants.h"
#include "../multitasking/multitasking.h"
#include "textio.h"

tty_char_t tty_buffer[MAX_TTY_X * MAX_TTY_Y];
tty_char_t tty_alternate_buffer[MAX_TTY_X * MAX_TTY_Y];
tty_char_t* tty_data;

uint32_t tty_dirty = 0;

bool tty_application_cursor_mode = false;

uint32_t tty_res_x = 0, tty_res_y = 0;

int32_t tty_cursor = 0;
int32_t tty_saved_cursor = 0;
uint8_t tty_color;

bool tty_reversed = false;

struct termios tty_ts;

pid_t tty_foreground_pgrp = 1;

bool tty_cursor_blink = true;

psf_font_t tty_font, tty_font_bold;
bool tty_bold = false;

uint32_t tty_control_sequence_buffer[TTY_ESC_BUFFER] = {0};
uint8_t tty_escape_sequence_index = 0;
bool tty_reading_escape_sequence = false, tty_reading_control_sequence = false;
bool tty_sequence_question_mark = false, tty_sequence_exclamation_mark = false;
int tty_H_code_selector = 0;

int tty_ignore_chars = 0;

char tty_osc_buffer[TTY_OSC_BUFFER];
int tty_osc_index = 0;
bool tty_reading_operating_system_command = false, tty_reading_operating_system_command_string = false;

#include "textio.h"
#include "../vga/vga.h"
#include "../graphics/linear_framebuffer.h"

void __tty_soft_reset(bool refresh)
{
    tty_data = tty_buffer;
    tty_color = FG_WHITE | BG_BLACK;
    tty_reversed = false;
    tty_bold = false;

	tty_ignore_chars = 0;
	tty_escape_sequence_index = 0;
	tty_osc_index = 0;
	tty_reading_escape_sequence = tty_reading_control_sequence = false;
	tty_sequence_question_mark = tty_sequence_exclamation_mark = false;
	tty_reading_operating_system_command = tty_reading_operating_system_command_string = false;

	__tty_refresh_screen(refresh);
}

void __tty_full_reset(bool refresh)
{
	__tty_soft_reset(refresh);
	tty_clear_screen(' ', refresh);
}

void tty_init(bool refresh)
{
	__tty_full_reset(refresh);
	tty_set_window_size(framebuffer.width / 8, framebuffer.height / (8 * TTY_AR), true);
	tty_clear_screen(' ', refresh);
}

void tty_clear_screen(char c, bool refresh)
{
    lock_scheduler();
    __tty_clear_screen(c, refresh);
    unlock_scheduler();
}

void __tty_ignore_next_characters(int n)
{
    tty_ignore_chars += n;
}

void __tty_clear_screen(char c, bool refresh)
{
	for (uint32_t i = 0; i < MAX_TTY_X * tty_res_y; i++)
	{
		if (i % MAX_TTY_X < tty_res_x)
		{
		    tty_data[i] = ((tty_char_t)tty_color << 8) | ' ' | ((tty_char_t)tty_bold << 16);
			tty_set_dirty(tty_data[i]);
			__tty_render_character(i, tty_data[i], refresh);
		}
	}
	tty_cursor = 0;
	__tty_render_cursor(tty_cursor, refresh);
}

void __tty_clear_section(uint32_t start_char, uint32_t end_char, uint8_t clear_color, bool refresh)
{
    for (uint32_t i = start_char; i < end_char; i++)
	{
		if (i % MAX_TTY_X < tty_res_x)
		{
		    tty_data[i] = ((tty_char_t)tty_color << 8) | ' ' | ((tty_char_t)tty_bold << 16);
			tty_set_dirty(tty_data[i]);
			__tty_render_character(i, tty_data[i], refresh);
		}
	}
	if (tty_cursor_blink)
		__tty_render_cursor(tty_cursor, refresh);
}

void __tty_move_characters(uint32_t start, int offset, bool refresh)
{
    if (offset == 0)
        return;
    size_t items = (offset > 0) ? (MAX_TTY_X - ((start + offset) % MAX_TTY_X)) : (MAX_TTY_X - (start % MAX_TTY_X));
	if (offset < 0)
	{
    	for (int i = 0; i < items; i++)
        {
            tty_data[start + offset + i] = tty_data[start + i];
            tty_set_dirty(tty_data[start + offset + i]);
        }
	}
	else
	{
        for (int i = items - 1; i >= 0; i--)
        {
            tty_data[start + offset + i] = tty_data[start + i];
            tty_set_dirty(tty_data[start + offset + i]);
        }
	}
	uint32_t base = (offset > 0) ? start : (start + offset);
	for (uint32_t i = base; i < base + items; i++)
		if (i % MAX_TTY_X < tty_res_x) __tty_render_character(i, tty_data[i], refresh);
	if (tty_cursor_blink)
		__tty_render_cursor(tty_cursor, refresh);
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

void tty_set_window_size(int sx, int sy, bool refresh)
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
	__tty_refresh_screen(refresh);
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

void __tty_render_character(uint32_t cursor, tty_char_t c, bool refresh)
{
	if (cursor >= MAX_TTY_X * tty_res_y)
	    return;
	if (!refresh)
	{
		tty_set_dirty(tty_data[cursor]);
		return;
	}
	if (!(tty_data[cursor] & TTY_DIRTY))
	    return;
	tty_unset_dirty(tty_data[cursor]);

	bool bold = (c >> 16) & 1;
	uint8_t color = c >> 8;
	c &= 0x7f;

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
	framebuffer_render_psf2_char(&framebuffer, x, y, width, height, bold ? &tty_font_bold : &tty_font, c,
		fg_color.r,
		fg_color.g,
		fg_color.b,
		false, bg_color.r, bg_color.g, bg_color.b);
}

void __tty_render_cursor(uint32_t cursor, bool refresh)
{
	if (cursor >= MAX_TTY_X * tty_res_y)
		return;
	if (cursor % MAX_TTY_X >= tty_res_x)
		return;
	if (!refresh)
	    return;

	uint32_t width = __tty_get_character_width();
	uint32_t height = __tty_get_character_height();

	uint32_t x = __tty_get_character_pos_x(cursor);
	uint32_t y = __tty_get_character_pos_y(cursor);

	srgb_t cursor_col = theme_cursor_color;

	framebuffer_fill_rect(&framebuffer, x, y + (8 * height / 10), width, height / 5, cursor_col.r, cursor_col.g, cursor_col.b);
}

void __tty_refresh_screen(bool refresh)
{
    assert(tty_data);
	for (uint32_t i = 0; i < MAX_TTY_X * tty_res_y; i++)
		if (i % MAX_TTY_X < tty_res_x) __tty_render_character(i, tty_data[i], refresh);
	if (tty_cursor_blink)
		__tty_render_cursor(tty_cursor, refresh);
}

void __tty_ansi_m_code(uint32_t code)
{
	if (tty_sequence_question_mark || tty_sequence_exclamation_mark)
		return;

	if (code == 0)
	{
		tty_color = FG_WHITE | BG_BLACK;
		tty_bold = false;
		tty_reversed = false;
		return;
	}

	if (code == 1)
	{
	    tty_bold = true;
	    return;
	}
	if (code == 2 || code == 21 || code == 22)
	{
	    tty_bold = false;
		return;
	}

	if (code == 7)
	{
	    if (!tty_reversed)
			tty_color = ((tty_color & 0x0f) << 4) | ((tty_color & 0xf0) >> 4);
	    tty_reversed = true;
		return;
	}
	if (code == 27)
	{
	    if (tty_reversed)
			tty_color = ((tty_color & 0x0f) << 4) | ((tty_color & 0xf0) >> 4);
	    tty_reversed = false;
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
	else
	    return;
}

void __tty_ansi_J_code(uint32_t code, bool refresh)
{
	if (tty_sequence_question_mark || tty_sequence_exclamation_mark)
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
		(code == 3 ? MAX_TTY_X * tty_res_y : MAX_TTY_X * tty_res_y)))), FG_WHITE | BG_BLACK, refresh);
}

void __tty_ansi_H_code(uint32_t code, bool refresh)
{
	if (tty_sequence_question_mark || tty_sequence_exclamation_mark)
		return;

	static int32_t n = 1, m = 1;
	if (tty_H_code_selector == 0)
        n = code == 0 ? 1 : code;
	else
	    m = code == 0 ? 1 : code;

	tty_H_code_selector++;
	tty_H_code_selector %= 2;

	tty_set_dirty(tty_data[tty_cursor]);
	__tty_render_character(tty_cursor, tty_data[tty_cursor], refresh);

	tty_cursor = (n - 1) * MAX_TTY_X + (m - 1);
	if (tty_cursor_blink)
	{
        tty_set_dirty(tty_data[tty_cursor]);
	    __tty_render_cursor(tty_cursor, refresh);
		__tty_render_character(tty_cursor, tty_data[tty_cursor], refresh);
	}

	return;
}

void __tty_ansi_h_code(uint32_t code, bool refresh)
{
	if (tty_sequence_question_mark)
	{
		switch (code)
		{
		case 1:
		    tty_application_cursor_mode = true;
		    return;
		case 2004:
			// ? bracketed paste on
			return;
		case 1049:
		    // ? switch to alternate screen buffer
			tty_data = tty_alternate_buffer;
			tty_saved_cursor = tty_cursor;
			for (int i = 0; i < MAX_TTY_X * tty_res_y; i++)
			    tty_data[i] = ' ' | (tty_color << 8) | TTY_DIRTY;
			tty_dirty = MAX_TTY_X * tty_res_y;
			__tty_refresh_screen(refresh);
			return;
		}

		return;
	}

	return;
}

void __tty_ansi_l_code(uint32_t code, bool refresh)
{
	if (tty_sequence_question_mark)
	{
    	switch (code)
    	{
        case 1:
		    tty_application_cursor_mode = false;
		    return;
    	case 2004:
    		// ? bracketed paste off
    		return;
    	case 1049:
    	    // ? switch to main screen buffer
    		tty_data = tty_buffer;
            tty_cursor = tty_saved_cursor;
            for (int i = 0; i < MAX_TTY_X * tty_res_y; i++)
			    tty_data[i] |= TTY_DIRTY;
            tty_dirty = MAX_TTY_X * tty_res_y;
            __tty_refresh_screen(refresh);
    		return;
    	}

		return;
	}

	return;
}

void __tty_ansi_K_code(uint32_t code, bool refresh)
{
	if (tty_sequence_question_mark || tty_sequence_exclamation_mark)
		return;

	if (code != 0 && code != 1 && code != 2)
		return;

	__tty_clear_section( code == 0 ? tty_cursor :
						(code == 1 ? (tty_cursor / MAX_TTY_X) * MAX_TTY_X :
						(code == 2 ? (tty_cursor / MAX_TTY_X) * MAX_TTY_X : 0)),
						 code == 0 ? ((tty_cursor + MAX_TTY_X) / MAX_TTY_X) * MAX_TTY_X :
						(code == 1 ? tty_cursor :
						(code == 2 ? ((tty_cursor + MAX_TTY_X) / MAX_TTY_X) * MAX_TTY_X : 0)),
						FG_WHITE | BG_BLACK, refresh);
}

void __tty_ansi_P_code(uint32_t code, bool refresh)
{
	if (tty_sequence_question_mark || tty_sequence_exclamation_mark)
		return;

	if (code == 0)
		code = 1;

	__tty_move_characters(tty_cursor + code, -code, refresh);
}

void __tty_ansi_t_code(uint32_t code)
{
	if (tty_sequence_question_mark || tty_sequence_exclamation_mark)
		return;

	// TODO: Push/pop title
	return;
}

void __tty_ansi_at_code(uint32_t code, bool refresh)
{
	if (tty_sequence_question_mark || tty_sequence_exclamation_mark)
		return;

	if (code == 0)
		code = 1;

	__tty_move_characters(tty_cursor, code, refresh);
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

void __tty_scroll(bool refresh)
{
    bool scrolled = false;
	while (tty_cursor / MAX_TTY_X >= tty_res_y || tty_cursor < 0)
	{
		scrolled = true;

		if (tty_cursor < 0)
		{
    		for (uint32_t i = tty_res_y - 1; i > 0; i--)
            {
                for (uint32_t j = 0; j < tty_res_x; j++)
                {
                    tty_char_t* cur = &tty_data[i * MAX_TTY_X + j];
                    tty_char_t* new = &tty_data[(i - 1) * MAX_TTY_X + j];
                    bool are_different = (*cur & 0xffff) != (*new & 0xffff);
                    *cur = *new;
                    if (are_different)
                        tty_set_dirty(*cur);
                }
            }
    		for (uint32_t j = 0; j < tty_res_x; j++)
            {
                tty_char_t* cur = &tty_data[j];
                tty_char_t new = ' ' | ((uint16_t)tty_color << 8);
                bool are_different = (*cur & 0xffff) != (new & 0xffff);
                *cur = new;
                if (are_different)
                    tty_set_dirty(*cur);
            }
		}
		else
		{
		    for (uint32_t i = 0; i < tty_res_y - 1; i++)
            {
                for (uint32_t j = 0; j < tty_res_x; j++)
				{
    				tty_char_t* cur = &tty_data[i * MAX_TTY_X + j];
                    tty_char_t* new = &tty_data[(i + 1) * MAX_TTY_X + j];
                    bool are_different = (*cur & 0xffff) != (*new & 0xffff);
                    *cur = *new;
                    if (are_different)
                        tty_set_dirty(*cur);
				}
            }
			for (uint32_t j = 0; j < tty_res_x; j++)
            {
                tty_char_t* cur = &tty_data[(tty_res_y - 1) * MAX_TTY_X + j];
                tty_char_t new = ' ' | ((uint16_t)tty_color << 8);
                bool are_different = (*cur & 0xffff) != (new & 0xffff);
                *cur = new;
                if (are_different)
                    tty_set_dirty(*cur);
            }
		}

		if (tty_cursor < 0)
		    tty_cursor += MAX_TTY_X;
		else
		    tty_cursor -= MAX_TTY_X;
	}
   #ifndef DEBUG_SCREEN
	if (scrolled)
   #endif
		__tty_refresh_screen(refresh);
}

void tty_outc_ex(char c, int flags, bool refresh)
{
	if (!tty_font.f)
		return;
	if (c == 0)
		return;

	lock_scheduler();

	if (tty_ignore_chars > 0)
	{
	    tty_ignore_chars--;
		unlock_scheduler();
		return;
	}

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
		tty_control_sequence_buffer[0] = 0;
		unlock_scheduler();
		return;
	#else
        unlock_scheduler();
    #ifdef PRINT_UNRECOGNIZED_ANSI
		tty_outc('^');
	#endif
		return;
	#endif
	}

	if (tty_reading_escape_sequence)
	{
	    tty_reading_escape_sequence = false;
    	switch (c)
    	{
    	case '[':
            tty_reading_control_sequence = true;
           	tty_sequence_question_mark = tty_sequence_exclamation_mark = false;
           	unlock_scheduler();
           	return;
        case ']':
            tty_reading_operating_system_command = true;
            unlock_scheduler();
			return;
		case 'M':
			tty_cursor -= MAX_TTY_X;
            __tty_scroll(refresh);
		    unlock_scheduler();
		    return;
        case '(':
        // * Designate G0 Character Set
       	    __tty_ignore_next_characters(1);
            unlock_scheduler();
            return;

		case '=':
		    // * Alternate keypad notation
		    unlock_scheduler();
		    return;
    	case '>':
    	    // * Normal keypad notation
    	    unlock_scheduler();
    	    return;

         case 'c':
            // * Full reset
            __tty_full_reset(refresh);
            unlock_scheduler();
            return;

    	default:
           	unlock_scheduler();
        #ifdef PRINT_UNRECOGNIZED_ANSI
           	tty_outc('^');
           	tty_outc(c);
        #endif
           	return;
    	}
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
		case '!':
			tty_sequence_exclamation_mark = true;
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
				__tty_ansi_J_code(tty_control_sequence_buffer[i], refresh);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'H':
		    tty_H_code_selector = 0;
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_H_code(tty_control_sequence_buffer[i], refresh);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'h':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_h_code(tty_control_sequence_buffer[i], refresh);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'A':
		{
    		int n = tty_control_sequence_buffer[tty_escape_sequence_index];
            if (n == 0)
                n = 1;
            tty_escape_sequence_index = 0;
    		tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
    		tty_reading_control_sequence = false;
            tty_set_dirty(tty_data[tty_cursor]);
  		    __tty_render_character(tty_cursor, tty_data[tty_cursor], refresh);
    		if (tty_cursor >= n * MAX_TTY_X)
    		    tty_cursor -= n * MAX_TTY_X;
            else
                tty_cursor -= (tty_cursor / MAX_TTY_X) * MAX_TTY_X;
   			if (tty_cursor_blink)
  				__tty_render_cursor(tty_cursor, refresh);
    		unlock_scheduler();
    		return;
		}
		case 'B':
		{
    		int n = tty_control_sequence_buffer[tty_escape_sequence_index];
            if (n == 0)
                n = 1;
            tty_escape_sequence_index = 0;
    		tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
    		tty_reading_control_sequence = false;
            tty_set_dirty(tty_data[tty_cursor]);
  		    __tty_render_character(tty_cursor, tty_data[tty_cursor], refresh);
    		if (tty_cursor < (tty_res_y - n) * MAX_TTY_X)
    		    tty_cursor += n * MAX_TTY_X;
            else
                tty_cursor = (tty_cursor % MAX_TTY_X) + MAX_TTY_X * (tty_res_y - 1);
   			if (tty_cursor_blink)
  				__tty_render_cursor(tty_cursor, refresh);
    		unlock_scheduler();
    		return;
		}
		case 'C':
		{
    		int n = tty_control_sequence_buffer[tty_escape_sequence_index];
            if (n == 0)
                n = 1;
            tty_escape_sequence_index = 0;
    		tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
    		tty_reading_control_sequence = false;
            tty_set_dirty(tty_data[tty_cursor]);
  		    __tty_render_character(tty_cursor, tty_data[tty_cursor], refresh);
    		if ((tty_cursor % MAX_TTY_X) < tty_res_x - n)
    		    tty_cursor += n;
            else
                tty_cursor = tty_res_x - 1 + MAX_TTY_X * (tty_res_x / MAX_TTY_X);
   			if (tty_cursor_blink)
  				__tty_render_cursor(tty_cursor, refresh);
    		unlock_scheduler();
    		return;
		}
		case 'D':
		{
    		int n = tty_control_sequence_buffer[tty_escape_sequence_index];
            if (n == 0)
                n = 1;
            tty_escape_sequence_index = 0;
    		tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
    		tty_reading_control_sequence = false;
            tty_set_dirty(tty_data[tty_cursor]);
  		    __tty_render_character(tty_cursor, tty_data[tty_cursor], refresh);
    		if ((tty_cursor % MAX_TTY_X) >= n)
    		    tty_cursor -= n;
            else
                tty_cursor = MAX_TTY_X * (tty_res_x / MAX_TTY_X);
   			if (tty_cursor_blink)
  				__tty_render_cursor(tty_cursor, refresh);
    		unlock_scheduler();
    		return;
		}
		case 'l':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_l_code(tty_control_sequence_buffer[i], refresh);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'K':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_K_code(tty_control_sequence_buffer[i], refresh);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'P':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_P_code(tty_control_sequence_buffer[i], refresh);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case '@':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_at_code(tty_control_sequence_buffer[i], refresh);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 't':
			for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
				__tty_ansi_t_code(tty_control_sequence_buffer[i]);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		case 'p':
		    if (tty_sequence_exclamation_mark)
				__tty_soft_reset(refresh);
			tty_escape_sequence_index = 0;
			tty_control_sequence_buffer[tty_escape_sequence_index] = 0;
			tty_reading_control_sequence = false;
			break;
		default:
			tty_reading_control_sequence = false;
			tty_reading_operating_system_command = false;
			tty_reading_operating_system_command_string = false;
			tty_osc_index = 0;
		#ifdef PRINT_UNRECOGNIZED_ANSI
			tty_outc('^');
			tty_outc('[');
			if (!(tty_escape_sequence_index == 0 && tty_control_sequence_buffer[0] == 0))
				for (uint8_t i = 0; i <= tty_escape_sequence_index; i++)
					dprintf(STDOUT_FILENO, "%u%s", tty_control_sequence_buffer[i], i == tty_escape_sequence_index ? "" : ";");
			tty_outc(c);
		#endif
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

	if (c != '\b' && c != 7 && c != '\n' && c != '\r')
	{
	    tty_data[tty_cursor] = c | ((tty_char_t)tty_color << 8) | (tty_bold ? TTY_BOLD : 0) | (flags & ~0xffff);
		tty_set_dirty(tty_data[tty_cursor]);
		int i = tty_cursor + 1;
		if (i % MAX_TTY_X >= tty_res_x)
		{
		    i -= i % MAX_TTY_X;
			i += MAX_TTY_X;
		}
		while (i < MAX_TTY_X * tty_res_y && (tty_data[i] & TTY_CONTINUE_CHAR))
		{
		    tty_data[i] = ' ' | ((tty_char_t)tty_color << 8);
			tty_set_dirty(tty_data[i]);
            __tty_render_character(i, tty_data[i], refresh);
		    i++;
			if (i % MAX_TTY_X >= tty_res_x)
			{
			    i -= i % MAX_TTY_X;
				i += MAX_TTY_X;
			}
		}
	}

	tty_set_dirty(tty_data[tty_cursor]);
	__tty_render_character(tty_cursor, tty_data[tty_cursor], refresh);

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
	    int32_t new_cursor = ((tty_cursor - (tty_cursor / MAX_TTY_X) * MAX_TTY_X + TAB_LENGTH) / TAB_LENGTH) * TAB_LENGTH + (tty_cursor / MAX_TTY_X) * MAX_TTY_X;
		for (int32_t i = tty_cursor; i < new_cursor; i++)
		    __tty_render_character(tty_cursor, ' ' | ((tty_char_t)tty_color << 8), refresh);
		tty_cursor = new_cursor;
		break;
	}

	case '\b':
	{
	    do
    	{
    	    bool scroll = false;
    		if (tty_cursor <= 0)
    		    scroll = true;
    	    if ((tty_cursor % MAX_TTY_X) == 0)
    			tty_cursor = tty_cursor - MAX_TTY_X + tty_res_x - 1;
    	    else
    			tty_cursor--;
    		if (scroll)
    			__tty_scroll(refresh);
    	}
        while ((tty_data[tty_cursor] & TTY_CONTINUE_CHAR) && (tty_cursor % MAX_TTY_X) < tty_res_x);
		break;
	}
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
		__tty_render_cursor(tty_cursor, refresh);

	__tty_scroll(refresh);

	unlock_scheduler();
}

void tty_outc(char c)
{
    tty_outc_ex(c, 0, true);
}
