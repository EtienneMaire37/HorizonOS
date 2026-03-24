#include "psf.h"
#include "../multitasking/multitasking.h"

uint32_t psf_get_glyph_width(psf_font_t* font)
{
    if (!font) return 0;
    if (!font->f) return 0;
    if (!font->f->data) return 0;

    if (*(uint16_t*)font->f->data == 0x0436)
        return 8;
    if (*(uint32_t*)font->f->data == 0x864ab572)
        return ((psf2_t*)font->f->data)->width;

    return 0;
}

uint32_t psf_get_glyph_height(psf_font_t* font)
{
    if (!font) return 0;
    if (!font->f) return 0;
    if (!font->f->data) return 0;

    if (*(uint16_t*)font->f->data == 0x0436)
        return ((psf_t*)font->f->data)->bytesperglyph;
    if (*(uint32_t*)font->f->data == 0x864ab572)
        return ((psf2_t*)font->f->data)->height;

    return 0;
}

uint32_t psf_get_bytes_per_glyph(psf_font_t* font)
{
    if (!font) return 0;
    if (!font->f) return 0;
    if (!font->f->data) return 0;

    if (*(uint16_t*)font->f->data == 0x0436)
        return ((psf_t*)font->f->data)->bytesperglyph;
    if (*(uint32_t*)font->f->data == 0x864ab572)
        return ((psf2_t*)font->f->data)->bytesperglyph;

    return 0;
}

uintptr_t psf_get_glyph_data(psf_font_t* font)
{
    if (!font) return 0;
    if (!font->f) return 0;
    if (!font->f->data) return 0;

    if (*(uint16_t*)font->f->data == 0x0436)
        return (uintptr_t)font->f->data + sizeof(psf_t);
    if (*(uint32_t*)font->f->data == 0x864ab572)
        return (uintptr_t)font->f->data + ((psf2_t*)font->f->data)->headersize;

    return 0;
}

uint32_t psf_get_num_glyph(psf_font_t* font)
{
    if (!font) return 0;
    if (!font->f) return 0;
    if (!font->f->data) return 0;

    if (*(uint16_t*)font->f->data == 0x0436)
        return (((psf_t*)font->f->data)->font_mode & PSF1_MODE512) ? 512 : 256;
    if (*(uint32_t*)font->f->data == 0x864ab572)
        return ((psf2_t*)font->f->data)->numglyph;

    return 0;
}

psf_font_t psf_font_load_from_initrd(const char* path)
{
    LOG(DEBUG, "Loading font \"%s\":", path);

    psf_font_t font;
    font.f = initrd_find_file(path);

    if (!font.f)
    {
        LOG(ERROR, "No such file or directory");
        return font;
    }
    if (!font.f->data)
    {
        LOG(ERROR, "No such file or directory");
        font.f = NULL;
        return font;
    }

    void* data = font.f->data;
    if (*(uint16_t*)data != 0x0436 && *(uint32_t*)data != 0x864ab572)    // * PSF or PSF2
    {
        LOG(ERROR, "Not a psf font");

        font.f = NULL;
        return font;
    }

    LOG(INFO, "Loaded psf font \"%s\":", path);
    LOG(DEBUG, "\tchar width: %u", psf_get_glyph_width(&font));
    LOG(DEBUG, "\tchar height: %u", psf_get_glyph_height(&font));
    LOG(DEBUG, "\tbytes per glyph: %u", psf_get_bytes_per_glyph(&font));
    LOG(DEBUG, "\tnum glyph: %u", psf_get_num_glyph(&font));

    return font;
}
