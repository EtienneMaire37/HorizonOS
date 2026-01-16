#pragma once

#include "../initrd/initrd.h"

#define PSF1_MODE512 0x01

typedef struct psf
{
    uint16_t magic;
    uint8_t font_mode;
    uint8_t bytesperglyph;
} __attribute__((packed)) psf_t;

typedef struct psf2
{
    uint32_t magic;
    uint32_t version;
    uint32_t headersize;
    uint32_t flags;
    uint32_t numglyph;
    uint32_t bytesperglyph;
    uint32_t height;
    uint32_t width;
    uint8_t glyphs;
} __attribute__((packed)) psf2_t;

typedef struct psf_font
{
    initrd_file_t* f;
} psf_font_t;

uint32_t psf_get_glyph_width(psf_font_t* font);
uint32_t psf_get_glyph_height(psf_font_t* font);
uint32_t psf_get_bytes_per_glyph(psf_font_t* font);
uintptr_t psf_get_glyph_data(psf_font_t* font);
uint32_t psf_get_num_glyph(psf_font_t* font);
psf_font_t psf_font_load_from_initrd(const char* path);