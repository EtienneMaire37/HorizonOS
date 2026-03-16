#pragma once

struct mem_block
{
    physical_address_t address;
    uint64_t length;
} __attribute__((packed));

#define MAX_USABLE_MEMORY_BLOCKS 64