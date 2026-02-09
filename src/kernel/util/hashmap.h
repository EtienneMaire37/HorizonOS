#pragma once

#include "linked_list.h"
#include "../debug/out.h"
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

typedef struct hashmap_item
{
	uint64_t key;
	void* value;
} hashmap_item_t;

typedef struct hashmap
{
    size_t items;
	ll_t* data;
} hashmap_t;

static void hashmap_log(hashmap_t* hmp)
{
	LOG(DEBUG, "{");
	if (!hmp) goto end;
	for (size_t i = 0; i < hmp->items; i++)
	{
		if (hmp->data[i])
		{
			ll_item_t* it = hmp->data[i];
			do
			{
				hashmap_item_t* item = (hashmap_item_t*)it->data;
				LOG(DEBUG, "%#" PRIx64 ": %p", item->key, item->value);
				it = it->next;
			} while (it != hmp->data[i]);
		}
	}
end:
	LOG(DEBUG, "}");
}

hashmap_t* hashmap_create(size_t cap);
void hashmap_destroy(hashmap_t* hmp);
void hashmap_set_item(hashmap_t* hmp, uint64_t key, void* value);
void hashmap_remove_item(hashmap_t* hmp, uint64_t key);
void* hashmap_get_item(hashmap_t* hmp, uint64_t key);
