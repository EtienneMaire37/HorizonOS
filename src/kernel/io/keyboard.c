#include "../ps2/keyboard.h"
#include "../memalloc/page_frame_allocator.h"
#include <assert.h>
#include <string.h>
#include "../util/math.h"
#include "../multitasking/task.h"
#include "../vga/textio.h"

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

    buffer->put_index = imod(buffer->put_index - 1, buffer->size);
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

void keyboard_handle_character(utf32_char_t character, virtual_key_t vk, bool echo, bool raw, int noncanonical_read_minimum_count)
{
    if (!multitasking_enabled) return;
    
    char ascii = utf32_to_bios_oem(character);
    if (!is_printable_character(ascii)
        && vk != VK_UP && vk != VK_RIGHT && vk != VK_DOWN && vk != VK_LEFT
        && vk != VK_HOME && vk != VK_END
        && vk != VK_INSERT && vk != VK_DELETE
        && vk != VK_PAGEUP && vk != VK_PAGEDOWN
        && ascii != '\n' && ascii != '\t' && ascii != '\b'
        ) 
        return;
    lock_task_queue();
    if (task_reading_stdin != -1)
    {            
        thread_t* task = find_task_by_pid(task_reading_stdin);
        if (character == '\b')
        {
            if (raw)
            {
                utf32_buffer_putchar(&keyboard_input_buffer, '\b');
                if (echo) printf("\b \b");
            }
            else if (!no_buffered_characters(keyboard_input_buffer))
            {
                if (echo) printf("\b \b");
                utf32_buffer_unputchar(&keyboard_input_buffer);
            }
        }
        else
        {
            size_t num_characters = get_buffered_characters(keyboard_input_buffer);
            size_t max_characters = keyboard_input_buffer.size - 1;
            size_t character_len;

            switch (vk)
            {
            case VK_TAB:    character_len = TAB_LENGTH; break;

            case VK_UP:     character_len = 3; break;
            case VK_DOWN:   character_len = 3; break;
            case VK_RIGHT:  character_len = 3; break;
            case VK_LEFT:   character_len = 3; break;

            case VK_HOME:   character_len = 3; break;
            case VK_END:    character_len = 3; break;

            case VK_INSERT:   character_len = 4; break;
            case VK_DELETE:   character_len = 4; break;

            case VK_PAGEUP:   character_len = 4; break;
            case VK_PAGEDOWN: character_len = 4; break;

            default:        character_len = 1 + keyboard_is_key_pressed(VK_LALT);
            }

            if ((ssize_t)num_characters < (ssize_t)max_characters - character_len)
            {
                switch (vk)
                {
                case VK_TAB:
                    for (uint8_t j = 0; j < TAB_LENGTH; j++)
                    {
                        utf32_buffer_putchar(&keyboard_input_buffer, ' ');                    
                        if (echo) putchar(' ');
                    }
                    break;
                case VK_UP:
                    utf32_buffer_putchar(&keyboard_input_buffer, '\x1b');
                    utf32_buffer_putchar(&keyboard_input_buffer, '[');
                    utf32_buffer_putchar(&keyboard_input_buffer, 'A');
                    if (echo) printf("^[A");
                    break;
                case VK_DOWN:
                    utf32_buffer_putchar(&keyboard_input_buffer, '\x1b');
                    utf32_buffer_putchar(&keyboard_input_buffer, '[');
                    utf32_buffer_putchar(&keyboard_input_buffer, 'B');
                    if (echo) printf("^[B");
                    break;
                case VK_RIGHT:
                    utf32_buffer_putchar(&keyboard_input_buffer, '\x1b');
                    utf32_buffer_putchar(&keyboard_input_buffer, '[');
                    utf32_buffer_putchar(&keyboard_input_buffer, 'C');
                    if (echo) printf("^[C");
                    break;
                case VK_LEFT:
                    utf32_buffer_putchar(&keyboard_input_buffer, '\x1b');
                    utf32_buffer_putchar(&keyboard_input_buffer, '[');
                    utf32_buffer_putchar(&keyboard_input_buffer, 'D');
                    if (echo) printf("^[D");
                    break;
                case VK_HOME:
                    utf32_buffer_putchar(&keyboard_input_buffer, '\x1b');
                    utf32_buffer_putchar(&keyboard_input_buffer, '[');
                    utf32_buffer_putchar(&keyboard_input_buffer, 'H');
                    if (echo) printf("^[H");
                    break;
                case VK_END:
                    utf32_buffer_putchar(&keyboard_input_buffer, '\x1b');
                    utf32_buffer_putchar(&keyboard_input_buffer, '[');
                    utf32_buffer_putchar(&keyboard_input_buffer, 'F');
                    if (echo) printf("^[F");
                    break;
                case VK_INSERT:
                    utf32_buffer_putchar(&keyboard_input_buffer, '\x1b');
                    utf32_buffer_putchar(&keyboard_input_buffer, '[');
                    utf32_buffer_putchar(&keyboard_input_buffer, '2');
                    utf32_buffer_putchar(&keyboard_input_buffer, '~');
                    if (echo) printf("^[2~");
                    break;
                case VK_DELETE:
                    utf32_buffer_putchar(&keyboard_input_buffer, '\x1b');
                    utf32_buffer_putchar(&keyboard_input_buffer, '[');
                    utf32_buffer_putchar(&keyboard_input_buffer, '3');
                    utf32_buffer_putchar(&keyboard_input_buffer, '~');
                    if (echo) printf("^[3~");
                    break;
                case VK_PAGEUP:
                    utf32_buffer_putchar(&keyboard_input_buffer, '\x1b');
                    utf32_buffer_putchar(&keyboard_input_buffer, '[');
                    utf32_buffer_putchar(&keyboard_input_buffer, '5');
                    utf32_buffer_putchar(&keyboard_input_buffer, '~');
                    if (echo) printf("^[5~");
                    break;
                case VK_PAGEDOWN:
                    utf32_buffer_putchar(&keyboard_input_buffer, '\x1b');
                    utf32_buffer_putchar(&keyboard_input_buffer, '[');
                    utf32_buffer_putchar(&keyboard_input_buffer, '6');
                    utf32_buffer_putchar(&keyboard_input_buffer, '~');
                    if (echo) printf("^[6~");
                    break;
                default:
                    if (ascii)
                    {
                        if (keyboard_is_key_pressed(VK_LALT))
                        {
                            utf32_buffer_putchar(&keyboard_input_buffer, '\x1b');                    
                            if (echo) putchar('^');
                        }
                        utf32_buffer_putchar(&keyboard_input_buffer, character);                    
                        if (echo) putchar(ascii);
                    }
                }
            }
        }
        if (((character == '\n' || character == 4) && get_buffered_characters(keyboard_input_buffer) != 0) || (raw && get_buffered_characters(keyboard_input_buffer) >= noncanonical_read_minimum_count))   // * EOL or EOF
            task_reading_stdin = -1;
    }
    unlock_task_queue();

    fflush(stdout);
}