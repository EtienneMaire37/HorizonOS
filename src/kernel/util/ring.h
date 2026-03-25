#pragma  once

#include "math.h"
#include <stddef.h>

struct ring_buffer
{
    void* buffer;
    size_t size;
    size_t put_index;
    size_t get_index;
};

// #define ring_get_buffered_bytes(buffer) ((size_t)imod((int64_t)(buffer).put_index - (buffer).get_index, (buffer).size))
#define ring_get_buffered_bytes(buffer) (((buffer).put_index - (buffer).get_index + (buffer).size) % (buffer).size)
#define ring_no_buffered_bytes(buffer)  ((buffer).put_index == (buffer).get_index)
