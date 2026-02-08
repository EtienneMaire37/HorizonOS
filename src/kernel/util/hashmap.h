#pragma once

#include "linked_list.h"
#include <stddef.h>

typedef struct hashmap
{
    size_t items;
	ll_t* data;
} hashmap_t;

hashmap_t* hashmap_create(size_t cap);
