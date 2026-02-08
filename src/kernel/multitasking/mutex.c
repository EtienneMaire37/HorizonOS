#include "../multicore/spinlock.h"
#include "task.h"

void acquire_mutex(mutex_t* mutex)
{
    // !! Horribly inefficient
    // TODO: Make it better (by implementing proper futexes)
	if (current_task && multitasking_enabled) current_task->doing_io++;
	while (try_acquire_spinlock(mutex))
		switch_task();
}

void release_mutex(mutex_t* mutex)
{
	atomic_flag_clear_explicit(mutex, memory_order_release);
	if (current_task && multitasking_enabled) current_task->doing_io--;
}