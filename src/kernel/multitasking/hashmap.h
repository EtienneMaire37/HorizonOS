#pragma once

#include "../util/hashmap.h"
#include "queue.h"

#include <stdlib.h>
#include "multitasking.h"

static inline void tq_hashmap_push_back(hashmap_t* hmp, uint64_t key, thread_t* thread)
{
    if (!hmp) return;

    lock_scheduler();

    thread_queue_t* tq = hashmap_get_item(hmp, key);
    if (!tq)
    {
        tq = malloc(sizeof(thread_queue_t));
        *tq = TQ_INIT;
        hashmap_set_item(hmp, key, tq);
    }

    thread_queue_push_back(tq, thread);

    unlock_scheduler();
}

static inline void tq_hashmap_remove(hashmap_t* hmp, uint64_t key, thread_t* thread)
{
    if (!hmp) return;

    lock_scheduler();

    thread_queue_t* tq = hashmap_get_item(hmp, key);
    if (!tq || !*tq)
    {
        unlock_scheduler();
        return;
    }

    ll_remove(tq, ll_find_item_by_data(tq, thread));

    if (*tq == NULL)
    {
        hashmap_remove_item(hmp, key);
        free(tq);
    }
    unlock_scheduler();
}

static inline void tq_hashmap_log(hashmap_t* hmp)
{
    LOG(INFO, "{");
	if (!hmp) goto end;
	for (size_t i = 0; i < hmp->items; i++)
	{
		if (hmp->data[i])
		{
			ll_item_t* it = hmp->data[i];
			do
			{
				hashmap_item_t* item = (hashmap_item_t*)it->data;
				LOG(INFO, "\t%#" PRIx64 ": {", item->key);
                thread_queue_t* tq = item->value;
                thread_queue_item_t* tq_it = *tq;
                if (tq && *tq)
                {
                    do
                    {
                        thread_t* thread = tq_it->data;
                        CONTINUE_LOG(INFO, "%p", thread);
                        if (thread)
                            CONTINUE_LOG(INFO, " : { .name = \"%s\", .pid = %d, .ppid = %d, .pgid = %d }", thread->name, thread->pid, thread->ppid, thread->pgid);
                        CONTINUE_LOG(INFO, "%s", tq_it->next == *tq ? "" : ", ");
                        tq_it = tq_it->next;
                    } while (tq_it != *tq);
                }
                CONTINUE_LOG(INFO, "}, ");
				it = it->next;
			} while (it != hmp->data[i]);
		}
	}
end:
	LOG(INFO, "}");
}

static inline void thread_hashmap_log(hashmap_t* hmp)
{
    LOG(INFO, "{");
	if (!hmp) goto end;
	for (size_t i = 0; i < hmp->items; i++)
	{
		if (hmp->data[i])
		{
			ll_item_t* it = hmp->data[i];
			do
			{
				hashmap_item_t* item = (hashmap_item_t*)it->data;
				LOG(INFO, "\t%#" PRIx64, item->key);
                thread_t* thread = item->value;
                if (thread)
                    CONTINUE_LOG(INFO, ": { .name = \"%s\", .pid = %d, .ppid = %d, .pgid = %d }", thread->name, thread->pid, thread->ppid, thread->pgid);
                CONTINUE_LOG(INFO, ",");
				it = it->next;
			} while (it != hmp->data[i]);
		}
	}
end:
	LOG(INFO, "}");
}
