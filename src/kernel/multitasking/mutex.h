#pragma once

#include <stdatomic.h>

typedef atomic_flag mutex_t;

#define MUTEX_INIT ((mutex_t)ATOMIC_FLAG_INIT)

void acquire_mutex(mutex_t* mutex);
void release_mutex(mutex_t* mutex);