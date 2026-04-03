#pragma once

#include <stdatomic.h>
#include <stdbool.h>

#include "../multitasking/multitasking.h"

static inline bool try_acquire_spinlock(atomic_flag* spinlock)
{
    if (multitasking_enabled)
        lock_scheduler();
    bool ret = atomic_flag_test_and_set_explicit(spinlock, memory_order_acquire);
	if (multitasking_enabled)
	{
	    if (!ret)
			current_task->lock_depth++;
		unlock_scheduler();
	}
	return ret;
}

static inline void acquire_spinlock(atomic_flag* spinlock)
{
	while (try_acquire_spinlock(spinlock))
        __builtin_ia32_pause();
}

static inline void release_spinlock(atomic_flag* spinlock)
{
	atomic_flag_clear_explicit(spinlock, memory_order_release);
    if (multitasking_enabled)
		current_task->lock_depth--;
}
