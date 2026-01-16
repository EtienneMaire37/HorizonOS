#pragma once

#include "../files/psf.h"
#include <bootboot.h>

typedef struct linear_framebuffer
{
    uintptr_t address;
    uint32_t width, height;
    uint32_t stride;

    // * From bootboot.h
    uint8_t format;
} linear_framebuffer_t;

extern linear_framebuffer_t framebuffer;

static inline uint32_t framebuffer_encode_color_data(linear_framebuffer_t* buffer, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    if (!buffer)
        return 0;
    if (!buffer->address)
        return 0;

    switch (buffer->format)
    {
    case FB_ARGB:
        return ((uint32_t)alpha << 24) | ((uint32_t)red << 16) | ((uint32_t)green << 8) | blue;
    case FB_RGBA:
        return ((uint32_t)red << 24) | ((uint32_t)green << 16) | ((uint32_t)blue << 8) | alpha;
    case FB_ABGR:
        return ((uint32_t)alpha << 24) | ((uint32_t)blue << 16) | ((uint32_t)green << 8) | red;
    case FB_BGRA:
        return ((uint32_t)blue << 24) | ((uint32_t)green << 16) | ((uint32_t)red << 8) | alpha;
    }

    return 0;
}

void framebuffer_setpixel(linear_framebuffer_t* buffer, uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void framebuffer_fill_rect(linear_framebuffer_t* buffer, uint32_t x, uint32_t y, uint32_t size_x, uint32_t size_y, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void framebuffer_render_psf2_char(
    linear_framebuffer_t* buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height, 
    psf_font_t* font, char c,
    uint8_t r, uint8_t g, uint8_t b);