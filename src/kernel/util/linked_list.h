#pragma once

typedef struct ll_item ll_item_t;

typedef struct ll_item
{
    void* data;
    ll_item_t *prev, *next;
} ll_item_t;

typedef ll_item_t* ll_t;

#define LL_INIT     NULL

void ll_push_back(ll_t* ll, void* data);
void ll_remove(ll_t* ll, ll_item_t* data);
void ll_destroy(ll_t* ll);
ll_item_t* ll_find_item_by_data(ll_t* ll, void* data);