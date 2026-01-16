#include "page_frame_allocator.h"
#include "../multicore/spinlock.h"

atomic_flag liballoc_spinlock_state = ATOMIC_FLAG_INIT;

int liballoc_lock()
{
	acquire_spinlock(&liballoc_spinlock_state);
	return 0;
}

int liballoc_unlock()
{
	release_spinlock(&liballoc_spinlock_state);
	return 0;
}

void* liballoc_alloc(size_t pages)
{
	return pfa_allocate_contiguous_pages(pages);
}

int liballoc_free(void* ptr, size_t pages)
{    
    pfa_free_contiguous_pages(ptr, pages);
    return 0;
}