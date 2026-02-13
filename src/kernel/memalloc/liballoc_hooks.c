#include "page_frame_allocator.h"
#include "virtual_memory_allocator.h"
#include "../multitasking/mutex.h"

mutex_t liballoc_mutex = MUTEX_INIT;

int liballoc_lock()
{
	acquire_mutex(&liballoc_mutex);
	return 0;
}

int liballoc_unlock()
{
	release_mutex(&liballoc_mutex);
	return 0;
}

void* liballoc_alloc(size_t pages)
{
	void* addr = vmm_find_free_kernel_space_pages(NULL, pages);
	if (!addr) return NULL;
	allocate_range((uint64_t*)(get_cr3() + PHYS_MAP_BASE), (uint64_t)addr, pages, PG_SUPERVISOR, PG_READ_WRITE, CACHE_WB);
	return addr;
}

int liballoc_free(void* ptr, size_t pages)
{    
	free_range((uint64_t*)(get_cr3() + PHYS_MAP_BASE), (uint64_t)ptr, pages);
    return 0;
}