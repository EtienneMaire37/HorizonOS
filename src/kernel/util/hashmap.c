#include "hashmap.h"
#include <string.h>
#include <stdlib.h>

hashmap_t* hashmap_create(size_t cap)
{
    cap /= sizeof(ll_t);
    cap *= sizeof(ll_t);

    hashmap_t* hmp = (hashmap_t*)malloc(sizeof(hashmap_t));
    hmp->items = cap / sizeof(ll_t);
    hmp->data = (ll_t*)malloc(cap);
    memset(hmp->data, 0, cap);
    
    return hmp;
}

void hashmap_destroy(hashmap_t* hmp)
{
    if (!hmp) return;
    for (size_t i = 0; i < hmp->items; i++)
        ll_destroy(&hmp->data[i]);
    free(hmp->data);
    free(hmp);
}