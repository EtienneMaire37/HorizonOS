#pragma once

#include "../files/psf.h"
#include "../boot/limine.h"

#include <stdlib.h>

typedef struct linear_framebuffer
{
    void* address;
    uint32_t width, height;
    uint32_t stride;
    uint8_t format;
} linear_framebuffer_t;

extern linear_framebuffer_t framebuffer;

static inline uint32_t framebuffer_encode_color_data(linear_framebuffer_t* buffer, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    if (!buffer)
        return 0;
    if (!buffer->address)
        return 0;

    abort();

    switch (buffer->format)
    {
    case LIMINE_FRAMEBUFFER_RGB:
        return ((uint32_t)red << 24) | ((uint32_t)green << 16) | ((uint32_t)blue << 8) | alpha;
    default:
        abort();
    }

    return 0;
}

void framebuffer_setpixel(linear_framebuffer_t* buffer, uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void framebuffer_fill_rect(linear_framebuffer_t* buffer, uint32_t x, uint32_t y, uint32_t size_x, uint32_t size_y, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void framebuffer_render_psf2_char(
    linear_framebuffer_t* buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height, 
    psf_font_t* font, char c,
    uint8_t r, uint8_t g, uint8_t b);