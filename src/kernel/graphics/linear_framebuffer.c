#include "linear_framebuffer.h"

linear_framebuffer_t framebuffer;

void framebuffer_setpixel(linear_framebuffer_t* buffer, uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    if (!buffer)
        return;
    if (!buffer->address)
        return;

    if (x >= buffer->width) return;
    if (y >= buffer->height) return;

    ((uint32_t*)(buffer->address + buffer->stride * y))[x] = framebuffer_encode_color_data(buffer, red, green, blue, alpha);
}

void framebuffer_fill_rect(linear_framebuffer_t* buffer, uint32_t x, uint32_t y, uint32_t size_x, uint32_t size_y, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    if (!buffer)
        return;
    if (!buffer->address)
        return;

    if (x >= buffer->width) return;
    if (y >= buffer->height) return;

    uint32_t right = x + size_x, bottom = y + size_y;

    if (right >= buffer->width)
        right = buffer->width;
    if (bottom >= buffer->height)
        bottom = buffer->height;

    uint32_t dword = framebuffer_encode_color_data(buffer, red, green, blue, alpha);

    if (red == green && red == blue)
    {
        if (x == 0 && y == 0 && size_x == buffer->width && size_y == buffer->height)
        {
            memset((void*)buffer->address, red, buffer->stride * size_y);
        }
        else
        {
            for (uint32_t i = y; i < bottom; i++)
            {
                memset((void*)(buffer->address + buffer->stride * i) + 4 * x, red, 4 * (right - x));
            }
        }
    }
    else
    {
        for (uint32_t i = y; i < bottom; i++)
        {
            uint32_t* row = (uint32_t*)(buffer->address + buffer->stride * i);
            for (uint32_t j = x; j < right; j++)
            {
                row[j] = dword;
            }
        }
    }
}

void framebuffer_render_psf2_char(
    linear_framebuffer_t* buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height, 
    psf_font_t* font, char c,
    uint8_t r, uint8_t g, uint8_t b)
{
    if (!font) return;
    if (!font->f) return;
    if (!font->f->data) return;

    void* glyph_data = (void*)psf_get_glyph_data(font);

    uint32_t glyph_index = (uint8_t)c;
    if (glyph_index >= psf_get_num_glyph(font)) return;

    const uint32_t  glyph_width = psf_get_glyph_width(font),
                    glyph_height = psf_get_glyph_height(font);

    const uint8_t bytes_per_row = (glyph_width + 7) / 8;
    uint8_t* glyph = (uint8_t*)psf_get_glyph_data(font) + psf_get_bytes_per_glyph(font) * glyph_index;

    for (uint32_t i = y; i < y + height; i++) 
    {
        uint32_t offset_y = (i - y) * glyph_height / height;
        uint8_t* glyph_row = glyph + bytes_per_row * offset_y;

        for (uint32_t j = x; j < x + width; j++) 
        {
            uint32_t offset_x = (j - x) * glyph_width / width;
            uint32_t byte_index = offset_x / 8;
            uint8_t bit_index = 7 - (offset_x % 8);
            bool put = (glyph_row[byte_index] >> bit_index) & 1;

            if (put)
                framebuffer_setpixel(buffer, j, i, r, g, b, 0);
        }
    }
}
