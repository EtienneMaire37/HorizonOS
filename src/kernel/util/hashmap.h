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

void hashmap_log(hashmap_t* hmp);

hashmap_t* hashmap_create(size_t cap);
void hashmap_destroy(hashmap_t* hmp);
void hashmap_set_item(hashmap_t* hmp, uint64_t key, void* value);
void hashmap_remove_item(hashmap_t* hmp, uint64_t key);
void* hashmap_get_item(hashmap_t* hmp, uint64_t key);
