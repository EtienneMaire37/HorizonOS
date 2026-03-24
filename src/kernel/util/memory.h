#pragma once

#include <stddef.h>
#include "../debug/out.h"
#include "../multitasking/multitasking.h"

static inline void hexdump(void* addr, size_t bytes)
{
    if (bytes == 0)
        return;
    LOG(DEBUG, "Hexdump [%#" PRIx64 " - %#" PRIx64 "]:", (uint64_t)addr, (uint64_t)addr + bytes - 1);
    size_t addr_off = ((uint64_t)addr % 16);
    size_t offset = addr_off == 0 ? 0 : 16 - addr_off;
    bytes = (bytes + addr_off + 0xf) & ~0xfULL;
    for (size_t i = 0; i < bytes; i++)
    {
        if (i % 16 == 0)
            CONTINUE_LOG(DEBUG, "\n%016" PRIx64 ": ", (uint64_t)addr - addr_off + i);
        CONTINUE_LOG(DEBUG, "%02x ", ((uint8_t*)((uint64_t)addr - addr_off))[i]);

        if (i % 16 == 15)
        {
            CONTINUE_LOG(DEBUG, " |  ");
            for (size_t j = i - 15; j <= i; j++)
                CONTINUE_LOG(DEBUG, "%c ", ((uint8_t*)((uint64_t)addr - addr_off))[j]);
        }
    }
}
