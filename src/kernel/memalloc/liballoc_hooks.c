#include "page_frame_allocator.h"
#include "virtual_memory_allocator.h"
#include "../multitasking/mutex.h"
#include "../multitasking/multitasking.h"

mutex_t liballoc_mutex = MUTEX_INIT;

int liballoc_lock()
{
    lock_scheduler();
	return 0;
}

int liballoc_unlock()
{
    unlock_scheduler();
	return 0;
}

void* liballoc_alloc(size_t pages)
{
    lock_scheduler();
	void* addr = vmm_find_free_kernel_space_pages(NULL, pages);
    // LOG(TRACE, "liballoc_alloc: %p", addr);
	if (unlikely(!addr)) return (unlock_scheduler(), NULL);
	allocate_range((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE), (uint64_t)addr, pages, PG_SUPERVISOR, PG_READ_WRITE, CACHE_WB);
    unlock_scheduler();
    return addr;
}

int liballoc_free(void* ptr, size_t pages)
{
	free_range((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE), (uint64_t)ptr, pages);
    return 0;
}
