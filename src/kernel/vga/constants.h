#pragma once

// VGA text mode 3 colors
#define FG_BLACK         0x00
#define FG_BLUE          0x01
#define FG_GREEN         0x02
#define FG_CYAN          0x03
#define FG_RED           0x04
#define FG_MAGENTA       0x05
#define FG_BROWN         0x06
#define FG_LIGHTGRAY     0x07
#define FG_DARKGRAY      0x08
#define FG_LIGHTBLUE     0x09
#define FG_LIGHTGREEN    0x0a
#define FG_LIGHTCYAN     0x0b
#define FG_LIGHTRED      0x0c
#define FG_LIGHTMAGENTA  0x0d
#define FG_YELLOW        0x0e
#define FG_WHITE         0x0f
 
#define BG_BLACK         0x00
#define BG_BLUE          0x10
#define BG_GREEN         0x20
#define BG_CYAN          0x30
#define BG_RED           0x40
#define BG_MAGENTA       0x50
#define BG_BROWN         0x60
#define BG_LIGHTGRAY     0x70

// * Those have the blink attribute on (or high intensity if 3c0.10.3 (normal mode) = 1)
#define BG_DARKGRAY      0x80
#define BG_LIGHTBLUE     0x90
#define BG_LIGHTGREEN    0xa0
#define BG_LIGHTCYAN     0xb0
#define BG_LIGHTRED      0xc0
#define BG_LIGHTMAGENTA  0xd0
#define BG_YELLOW        0xe0
#define BG_WHITE         0xf0

#define TAB_LENGTH       4

#define TTY_RES_X   100
#define TTY_RES_Y   35
// Assume ar (probably should set a max value and dynamically scroll if below a certain threshold)

#define TTY_ANSI_BUFFER     32