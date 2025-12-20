#pragma once

#include "../multicore/spinlock.h"
#include "task.h"

void acquire_mutex(mutex_t* mutex)
{
    // !! Horribly inefficient
    // TODO: Make it better (by skipping tasks which are blocked)
	__CURRENT_TASK.doing_io++;
	while (try_acquire_spinlock(mutex))
		switch_task();
}

void release_mutex(mutex_t* mutex)
{
	atomic_flag_clear_explicit(mutex, memory_order_release);
	__CURRENT_TASK.doing_io--;
}