#include "sched_lock.h"
#include "multitasking.h"
#include "task.h"

spinlock_noint_t sched_lock = SPINLOCK_NOINT_INIT;

void unlock_scheduler(uint32_t flags)
{
    atomic_flag_clear_explicit(&sched_lock.flag, memory_order_release);
    if (queued_ts && multitasking_enabled)
    {
        queued_ts = false;
        switch_task();
    }
	set_rflags_if(flags);
}
