#pragma once

#include "../multicore/spinlock.h"

typedef struct
{
    spinlock_noint_t lock;
} page_table_data_t;

extern page_table_data_t* global_page_table;

static const page_table_data_t page_data_init = {.lock = SPINLOCK_NOINT_INIT};

void page_data_table_init();
uint32_t lock_page_table(uint64_t* addr);
void unlock_page_table(uint64_t* addr, uint32_t eflags);
