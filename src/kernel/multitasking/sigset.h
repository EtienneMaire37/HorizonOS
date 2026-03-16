#pragma once

#include "task.h"

sigset_t sigset_bitwise_or(sigset_t s1, sigset_t s2);
sigset_t sigset_bitwise_and(sigset_t s1, sigset_t s2);
sigset_t sigset_bitwise_not(sigset_t s);
bool sigset_is_bit_set(sigset_t s, int bit);