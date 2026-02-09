#pragma once

#include <stdatomic.h>

typedef int mutex_t;

#define MUTEX_INIT 0

void acquire_mutex(mutex_t* mutex);
void release_mutex(mutex_t* mutex);
