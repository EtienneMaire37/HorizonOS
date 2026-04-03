#include "../multicore/spinlock.h"
#include "task.h"
#include "multitasking.h"
#include "mutex.h"
#include "futex.h"
#include "../paging/paging.h"
#include "../cpu/memory.h"

void acquire_mutex(mutex_t* mutex)
{
    while (true)
    {
 		if (!try_acquire_spinlock((atomic_flag*)mutex))
            return;

 		if (multitasking_enabled)
 			futex_wait((uint64_t)virtual_to_physical((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE), (virtual_address_t)mutex), *mutex);
 		else
 			__builtin_ia32_pause();
    }
}

void release_mutex(mutex_t* mutex)
{
	atomic_flag_clear_explicit(mutex, memory_order_release);
	if (multitasking_enabled)
	{
		uint64_t paddr = (uint64_t)virtual_to_physical((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE), (virtual_address_t)mutex);
		futex_wake(paddr, 1);
	}
}
