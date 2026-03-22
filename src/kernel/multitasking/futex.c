#include "futex.h"
#include "multitasking.h"
#include "queue.h"
#include "../paging/paging.h"
#include <stdlib.h>

void futex_wait(uint64_t paddr, int expected)
{
	lock_scheduler();
	if (*(int*)(paddr + PHYS_MAP_BASE) != expected) goto end;
	thread_queue_t* fqueue = hashmap_get_item(futex_tq_hashmap, paddr);
	if (!fqueue)
	{
		fqueue = (thread_queue_t*)malloc(sizeof(thread_queue_t));
		*fqueue = TQ_INIT;
		hashmap_set_item(futex_tq_hashmap, paddr, fqueue);
	}
	move_running_task_to_thread_queue(fqueue, current_task);

end:
	unlock_scheduler();
}

void futex_wake(uint64_t paddr, int num)
{
	lock_scheduler();
	thread_queue_t* fqueue = (thread_queue_t*)hashmap_get_item(futex_tq_hashmap, paddr);
	if (!fqueue)
		goto end;
	thread_queue_item_t* it = *fqueue;
	int i = 0;
	do
	{
		thread_queue_item_t* to_move = it;
		it = it->next;
		move_task_to_running_queue(fqueue, to_move);
		i++;
	} while (*fqueue != NULL && i < num);
end:
	unlock_scheduler();
}
