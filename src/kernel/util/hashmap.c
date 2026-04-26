#include "hashmap.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "../random/pcg.h"
#include "../multitasking/multitasking.h"

hashmap_t* hashmap_create(size_t cap)
{
    cap /= sizeof(ll_t);
    cap *= sizeof(ll_t);

    hashmap_t* hmp = (hashmap_t*)malloc(sizeof(hashmap_t));
    assert(hmp);
    hmp->items = cap / sizeof(ll_t);
    hmp->data = (ll_t*)malloc(cap);
    assert(hmp->data);
    memset(hmp->data, 0, cap);

    return hmp;
}

void hashmap_destroy(hashmap_t* hmp)
{
    assert(hmp);
    for (size_t i = 0; i < hmp->items; i++)
    {
    	if (hmp->data[i])
        {
        	ll_item_t* it = hmp->data[i];
        	do
        	{
        		free(it->data);
        		it = it->next;
        	} while (it != hmp->data[i]);
        	ll_destroy(&hmp->data[i]);
        }
    }
    free(hmp->data);
    free(hmp);
}

static uint32_t hash64(uint64_t value)
{
	return pcg_hash(value & 0xffffffff) + pcg_hash((value & 0xffffffff) ^ (value >> 32));
}

void hashmap_set_item(hashmap_t* hmp, uint64_t key, void* value)
{
	assert(hmp);
	ll_t* ll = &hmp->data[hash64(key) % hmp->items];
	if (!*ll)
		goto new_item;
	else
	{
		ll_item_t* it = *ll;
		do
		{
			hashmap_item_t* item = (hashmap_item_t*)it->data;
			if (item->key == key)
			{
				item->value = value;
				return;
			}
			it = it->next;
		} while(it != *ll);
		goto new_item;
	}
	return;

new_item:
	hashmap_item_t* item = (hashmap_item_t*)malloc(sizeof(hashmap_item_t));
	item->key = key;
	item->value = value;

	ll_push_back(ll, item);
}

void hashmap_remove_item(hashmap_t* hmp, uint64_t key)
{
	assert(hmp);
	ll_t* ll = &hmp->data[hash64(key) % hmp->items];
	if (!*ll)
		return;
	ll_item_t* it = *ll;
	do
	{
		hashmap_item_t* item = (hashmap_item_t*)it->data;
		if (item->key == key)
		{
			ll_item_t* to_remove = it;
			ll_remove(ll, to_remove);
			return;
		}
		it = it->next;
	} while (it != *ll);
}

void* hashmap_get_item(hashmap_t* hmp, uint64_t key)
{
    assert(hmp);
	if (hmp->items == 0) return NULL;
	ll_t* ll = &hmp->data[hash64(key) % hmp->items];
	if (!*ll)
		return NULL;
	ll_item_t* it = *ll;
	do
	{
		hashmap_item_t* item = (hashmap_item_t*)it->data;
		if (!item) abort();
		if (item->key == key)
			return item->value;
		it = it->next;
	} while (it != *ll);

	return NULL;
}

void hashmap_log(hashmap_t* hmp)
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
				LOG(INFO, "\t%#" PRIx64 ": %p,", item->key, item->value);
				it = it->next;
			} while (it != hmp->data[i]);
		}
	}
end:
	LOG(INFO, "}");
}
