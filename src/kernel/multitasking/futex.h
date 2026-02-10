#pragma once

#include <stdint.h>

void futex_wait(uint64_t paddr, int expected);
void futex_wake(uint64_t paddr, int num);
