#pragma once

#include <stdint.h>

static inline __attribute__((always_inline)) __attribute__((const)) int64_t minint(int64_t a, int64_t b)
{
    return a < b ? a : b;
}
static inline __attribute__((always_inline)) __attribute__((const)) int64_t maxint(int64_t a, int64_t b)
{
    return a > b ? a : b;
}
static inline __attribute__((always_inline)) __attribute__((const)) int64_t absint(int64_t x)
{
    return x < 0 ? -x : x;
}
static inline __attribute__((always_inline)) __attribute__((const)) int imod(int a, int b)
{
    if (b <= 0)
        return 0;
    int ret = a - (a / b) * b;
    if (ret < 0) ret += b;
    return ret;
}

static inline __attribute__((always_inline)) __attribute__((const)) int hex_char_to_int(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return 0;
}
static inline __attribute__((always_inline)) __attribute__((const)) uint16_t bcd_to_binary(uint16_t bcd)
{
    return ((bcd) & 0x0f) + (((bcd) >> 4) & 0x0f) * 10 + (((bcd) >> 8) & 0x0f) * 100 + (((bcd) >> 12) & 0x0f) * 1000;
}
