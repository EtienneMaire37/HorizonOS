#pragma  once

#include "math.h"

#define ring_get_buffered_bytes(buffer) ((size_t)imod(((int)(buffer).put_index - (buffer).get_index), (buffer).size))
#define ring_no_buffered_bytes(buffer)  ((buffer).put_index == (buffer).get_index)
