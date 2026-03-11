#pragma once

#include <stddef.h>
#include "../debug/out.h"

static inline void hexdump(void* addr, size_t bytes)
{
    LOG(DEBUG, "Hexdump [%#" PRIx64 " - %#" PRIx64 "]:", (uint64_t)addr, (uint64_t)addr + bytes);
    for (size_t i = 0; i < bytes; i++)
    {
        if (i % 16 == 0)
            CONTINUE_LOG(DEBUG, "\n%016" PRIx64 ": ", (uint64_t)addr + i);
        CONTINUE_LOG(DEBUG, "%02x ", ((uint8_t*)addr)[i]);
    }
}