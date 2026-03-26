#pragma once

#include "../graphics/color.h"
#include <stdlib.h>
#include <stdint.h>
#include "../io/io.h"
#include "../terminal/themes.h"

// ! VGA Registers are not guaranteed to work under UEFI

#define VGA_REG_3C4_RESET                 0x00
#define VGA_REG_3C4_CLOCKING_MODE         0x01
#define VGA_REG_3C4_MAP_MASK              0x02
#define VGA_REG_3C4_CHARACTER_MAP_SELECT  0x03
#define VGA_REG_3C4_MEMORY_MODE           0x04

#define VGA_REG_3CE_SET_RESET             0x00
#define VGA_REG_3CE_ENABLE_SET_RESET      0x01
#define VGA_REG_3CE_COLOR_COMPARE         0x02
#define VGA_REG_3CE_DATA_ROTATE           0x03
#define VGA_REG_3CE_READ_MAP_SELECT       0x04
#define VGA_REG_3CE_GRAPHICS_MODE         0x05
#define VGA_REG_3CE_MISCELLANEOUS         0x06
#define VGA_REG_3CE_COLOR_DONT_CARE       0x07
#define VGA_REG_3CE_BIT_MASK              0x08

#define VGA_REG_3D4_HORIZONTAL_TOTAL          0x00
#define VGA_REG_3D4_HORIZONTAL_DISPLAY_END    0x01
#define VGA_REG_3D4_START_HORIZONTAL_BLANKING 0x02
#define VGA_REG_3D4_END_HORIZONTAL_BLANKING   0x03
#define VGA_REG_3D4_START_HORIZONTAL_RETRACE  0x04
#define VGA_REG_3D4_END_HORIZONTAL_RETRACE    0x05
#define VGA_REG_3D4_VERTICAL_TOTAL            0x06
#define VGA_REG_3D4_OVERFLOW                  0x07
#define VGA_REG_3D4_PRESET_ROW_SCAN           0x08
#define VGA_REG_3D4_MAXIMUM_SCAN_LINE         0x09
#define VGA_REG_3D4_CURSOR_START              0x0A
#define VGA_REG_3D4_CURSOR_END                0x0B
#define VGA_REG_3D4_START_ADDRESS_HIGH        0x0C
#define VGA_REG_3D4_START_ADDRESS_LOW         0x0D
#define VGA_REG_3D4_CURSOR_LOCATION_HIGH      0x0E
#define VGA_REG_3D4_CURSOR_LOCATION_LOW       0x0F
#define VGA_REG_3D4_VERTICAL_RETRACE_START    0x10
#define VGA_REG_3D4_VERTICAL_RETRACE_END      0x11
#define VGA_REG_3D4_VERTICAL_DISPLAY_END      0x12
#define VGA_REG_3D4_LOGICAL_WIDTH             0x13
#define VGA_REG_3D4_UNDERLINE_LOCATION        0x14
#define VGA_REG_3D4_VERTICAL_BLANK_START      0x15
#define VGA_REG_3D4_VERTICAL_BLANK_END        0x16
#define VGA_REG_3D4_MODE_CONTROL              0x17
#define VGA_REG_3D4_LINE_COMPARE              0x18

const srgb_t vga_colors[16] =
{
    {0x00, 0x00, 0x00}, // 0: Black
    {0x00, 0x00, 0xAA}, // 1: Blue
    {0x00, 0xAA, 0x00}, // 2: Green
    {0x00, 0xAA, 0xAA}, // 3: Cyan
    {0xAA, 0x00, 0x00}, // 4: Red
    {0xAA, 0x00, 0xAA}, // 5: Magenta
    {0xAA, 0x55, 0x00}, // 6: Brown (Dark Yellow)
    {0xAA, 0xAA, 0xAA}, // 7: Light Gray
    {0x55, 0x55, 0x55}, // 8: Dark Gray
    {0x55, 0x55, 0xFF}, // 9: Light Blue
    {0x55, 0xFF, 0x55}, // 10: Light Green
    {0x55, 0xFF, 0xFF}, // 11: Light Cyan
    {0xFF, 0x55, 0x55}, // 12: Light Red
    {0xFF, 0x55, 0xFF}, // 13: Light Magenta
    {0xFF, 0xFF, 0x55}, // 14: Yellow
    {0xFF, 0xFF, 0xFF}  // 15: White
};

static const uint8_t vga_to_term[16] =
{
    0,  // black
    4,  // blue
    2,  // green
    6,  // cyan
    1,  // red
    5,  // magenta
    3,  // brown -> yellow
    7,  // light gray -> white
    8,  // dark gray -> bright black
    12, // light blue
    10, // light green
    14, // light cyan
    9,  // light red
    13, // light magenta
    11, // yellow
    15  // white
};

srgb_t vga_get_color(uint8_t vga_color_code)
{
    return theme_colors[vga_to_term[vga_color_code & 0x0f]];
}

srgb_t vga_get_bg_color(uint8_t vga_color_code)
{
    // * For now don't add unnecessary data to tty_data
    if ((vga_color_code & 0xf0) == BG_BLACK)
        return theme_primary_background;
    return vga_get_color(vga_color_code >> 4);
}

srgb_t vga_get_fg_color(uint8_t vga_color_code)
{
    // * Same here
    if ((vga_color_code & 0x0f) == FG_WHITE)
        return theme_primary_foreground;
    return vga_get_color(vga_color_code);
}

void vga_write_port_3c0(uint8_t reg, uint8_t data)
{
    abort();
    outb(0x3c0, reg);
    outb(0x3c0, data);
}

uint8_t vga_read_port_3c0(uint8_t reg)
{
    abort();
    outb(0x3c0, reg);
    uint8_t data = inb(0x3c1);
    inb(0x3da);
    return data;
}

void vga_write_misc_output_register(uint8_t data)
{
    abort();
    outb(0x3c2, data);
}

uint8_t vga_read_misc_output_register()
{
    abort();
    return inb(0x3cc);
}

void vga_write_dac_mask_register(uint8_t data)
{
    abort();
    outb(0x3c6, data);
}

uint8_t vga_read_dac_mask_register()
{
    abort();
    return inb(0x3c6);
}

void vga_write_port_3c4(uint8_t reg, uint8_t data)
{
    abort();
    outb(0x3c4, reg);
    outb(0x3c5, data);
}

uint8_t vga_read_port_3c4(uint8_t reg)
{
    abort();
    outb(0x3c4, reg);
    return inb(0x3c5);
}

void vga_write_port_3ce(uint8_t reg, uint8_t data)
{
    abort();
    outb(0x3ce, reg);
    outb(0x3cf, data);
}

uint8_t vga_read_port_3ce(uint8_t reg)
{
    abort();
    outb(0x3ce, reg);
    return inb(0x3cf);
}

void vga_write_port_3d4(uint8_t reg, uint8_t data)
{
    abort();
    outb(0x3d4, reg);
    outb(0x3d5, data);
}

uint8_t vga_read_port_3d4(uint8_t reg)
{
    abort();
    outb(0x3d4, reg);
    return inb(0x3d5);
}

void vga_init()
{
    abort();
    vga_write_misc_output_register(vga_read_misc_output_register() | 1);    // Set registers to their default addresses
    inb(0x3da);    // Reset port 3c0
}
