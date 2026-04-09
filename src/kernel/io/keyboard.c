#include "../ps2/keyboard.h"
#include "../memalloc/page_frame_allocator.h"
#include <assert.h>
#include <string.h>
#include "../util/math.h"
#include "../multitasking/task.h"
#include "../multitasking/multitasking.h"
#include "../terminal/textio.h"
#include "../util/lambda.h"
#include <assert.h>
#include "../vfs/table.h"
#include "keyboard.h"

#define bufpri(...) do { character_len = snprintf(buffer, sizeof(buffer), __VA_ARGS__); assert(character_len != sizeof(buffer)); \
    if ((ssize_t)num_characters < (ssize_t)max_characters - character_len) { for (int i = 0; i < character_len; i++) { if (buffer[i] != 3) utf32_buffer_putchar(&keyboard_input_buffer, buffer[i]); if (echo || buffer[i] == 3) { if (buffer[i] < 0x20) tty_outc('^'); tty_outc_ex((buffer[i] < 0x20) ? buffer[i] + 0x40 : buffer[i], buffer[i] < 0x20 ? TTY_CONTINUE_CHAR : 0); } } } } while (0)

const keyboard_layout_t* current_keyboard_layout = &us_qwerty;

void utf32_buffer_init(utf32_buffer_t* buffer)
{
    if (!buffer) return;
    buffer->characters = (utf32_char_t*)pfa_allocate_page();
    buffer->size = 4096 / sizeof(utf32_char_t);
    assert(buffer->size == 1024);
    buffer->put_index = buffer->get_index = 0;
}

void utf32_buffer_destroy(utf32_buffer_t* buffer)
{
    if (!buffer) return;
    if (buffer->characters)
        pfa_free_page(buffer->characters);
    buffer->characters = NULL;
    buffer->characters = 0;
    buffer->size = 0;
    buffer->put_index = buffer->get_index = 0;
}

void utf32_buffer_create_and_copy(const utf32_buffer_t* from, utf32_buffer_t* to)
{
    if (!from || !to) return;
    utf32_buffer_init(to);
    memcpy(to->characters, from->characters, 4096);
    to->put_index = from->put_index;
    to->get_index = from->get_index;
}

void utf32_buffer_putchar(utf32_buffer_t* buffer, utf32_char_t character)
{
    if (!buffer) return;
    if (!buffer->characters) return;
    if (get_buffered_characters(*buffer) >= buffer->size - 1) return;     // No space to put characters

    buffer->characters[buffer->put_index] = character;
    buffer->put_index = (buffer->put_index + 1) % buffer->size;
}

void utf32_buffer_unputchar(utf32_buffer_t* buffer)
{
    if (!buffer) return;
    if (!buffer->characters) return;
    if (no_buffered_characters(*buffer)) return;

    buffer->put_index = imod((ssize_t)buffer->put_index - 1, buffer->size);
}

utf32_char_t utf32_buffer_getchar(utf32_buffer_t* buffer)
{
    if (!buffer) return 0;
    if (!buffer->characters) return 0;
    if (no_buffered_characters(*buffer)) return 0;     // No characters to get

    utf32_char_t ch = buffer->characters[buffer->get_index];
    buffer->get_index = (buffer->get_index + 1) % buffer->size;
    return ch;
}

void utf32_buffer_clear(utf32_buffer_t* buffer)
{
    buffer->get_index = buffer->put_index = 0;
}

bool keyboard_is_key_pressed(virtual_address_t vk)
{
    return ps2_kb_is_key_pressed(vk);
}

void keyboard_handle_character(utf32_char_t character, virtual_key_t vk, struct termios* ts, bool* sigint)
{
    assert(sigint);
    if (!multitasking_enabled) return;
    if (get_buffered_characters(keyboard_input_buffer) >= keyboard_input_buffer.size - 1) return;

    bool echo = (ts->c_lflag & ECHO) != 0;
    bool raw = (ts->c_lflag & ICANON) == 0;
    int noncanonical_read_minimum_count = ts->c_cc[VMIN];
    int raw_timeout = ts->c_cc[VTIME];

    bool shift = keyboard_is_key_pressed(VK_LSHIFT) || keyboard_is_key_pressed(VK_RSHIFT);
    bool ctrl = keyboard_is_key_pressed(VK_LCONTROL) || keyboard_is_key_pressed(VK_RCONTROL);
    bool lalt = keyboard_is_key_pressed(VK_LALT);
    bool meta = false;

    char ascii = utf32_to_bios_oem(character);
    if (!is_printable_character(ascii)
        && vk != VK_UP && vk != VK_RIGHT && vk != VK_DOWN && vk != VK_LEFT
        && vk != VK_HOME && vk != VK_END
        && vk != VK_INSERT && vk != VK_DELETE
        && vk != VK_PAGEUP && vk != VK_PAGEDOWN
        && ascii != '\n' && ascii != '\t' && ascii != '\b'
        )
        return;
    lock_scheduler();
    if (character == '\b')
    {
        if (raw)
        {
            utf32_buffer_putchar(&keyboard_input_buffer, character);
            if (echo) printf("\b \b");
        }
        else if (!no_buffered_characters(keyboard_input_buffer))
        {
            if (echo) printf("\b \b");
            utf32_buffer_unputchar(&keyboard_input_buffer);
        }
    }
    else if (character >= 0x07 && character <= 0x0f)
    {
        utf32_buffer_putchar(&keyboard_input_buffer, character);
        if (echo) printf("%c", character);
    }
    else
    {
        size_t num_characters = get_buffered_characters(keyboard_input_buffer);
        size_t max_characters = keyboard_input_buffer.size - 1;
        char buffer[16] = {0};
        size_t character_len = sizeof(buffer);
        char arrow_csi = tty_application_cursor_mode ? 'O' : '[';

        int modifier = 1 + shift + 2 * lalt + 4 * ctrl + 8 * meta;

        switch (vk)
        {
        case VK_TAB:
            if (!echo)
                bufpri("\t");
            else
            {
                for (uint8_t j = 0; j < TAB_LENGTH; j++)
                    bufpri(" ");
            }
            break;
        case VK_UP:
            if (modifier != 1)  bufpri("\x1b%c1;%dA", arrow_csi, modifier);
            else                bufpri("\x1b%cA", arrow_csi);
            break;
        case VK_DOWN:
            if (modifier != 1)  bufpri("\x1b%c1;%dB", arrow_csi, modifier);
            else                bufpri("\x1b%cB", arrow_csi);
            break;
        case VK_RIGHT:
            if (modifier != 1)  bufpri("\x1b%c1;%dC", arrow_csi, modifier);
            else                bufpri("\x1b%cC", arrow_csi);
            break;
        case VK_LEFT:
            if (modifier != 1)  bufpri("\x1b%c1;%dD", arrow_csi, modifier);
            else                bufpri("\x1b%cD", arrow_csi);
            break;
        case VK_HOME:
            if (modifier != 1)  bufpri("\x1b[1;%dH", modifier);
            else                bufpri("\x1b[H");
            break;
        case VK_END:
            if (modifier != 1)  bufpri("\x1b[1;%dF", modifier);
            else                bufpri("\x1b[F");
            break;
        case VK_INSERT:
            if (modifier != 1)  bufpri("\x1b[2;%d~", modifier);
            else                bufpri("\x1b[2~");
            break;
        case VK_DELETE:
            if (modifier != 1)  bufpri("\x1b[3;%d~", modifier);
            else                bufpri("\x1b[3~");
            break;
        case VK_PAGEUP:
            if (modifier != 1)  bufpri("\x1b[5;%d~", modifier);
            else                bufpri("\x1b[5~");
            break;
        case VK_PAGEDOWN:
            if (modifier != 1)  bufpri("\x1b[6;%d~", modifier);
            else                bufpri("\x1b[6~");
            break;
        default:
            if (ascii)
            {
                if (lalt)
                    bufpri("\x1b");
                bufpri("%c", ctrl && (character >= 0x20) ? character & 0x1f : character);
                if (ctrl && ((character & 0x1f) == 3))
                    *sigint = true;
            }
        }
    }
    if ((character == '\n' || character == tty_ts.c_cc[VEOF]) || (raw && get_buffered_characters(keyboard_input_buffer) >= noncanonical_read_minimum_count))   // * EOL or EOF
    {
        assert(keyboard_buffered_input_buffer.size == keyboard_input_buffer.size);
        memcpy(keyboard_buffered_input_buffer.characters, keyboard_input_buffer.characters, keyboard_input_buffer.size);
        keyboard_buffered_input_buffer.get_index = keyboard_input_buffer.get_index;
        keyboard_buffered_input_buffer.put_index = keyboard_input_buffer.put_index;
        keyboard_input_buffer.get_index = keyboard_input_buffer.put_index = 0;
        run_it_on_queue(&_waiting_for_stdin_tasks, lambda(void, (thread_t* task)
        {
            task_stop_polling(task);
        }));
    }
    unlock_scheduler();

    fflush(stdout);
}
