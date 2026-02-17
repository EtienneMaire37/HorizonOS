#include "linked_list.h"
#include <stdlib.h>

void ll_push_back(ll_t* ll, void* data)
{
    if (!data) return;
    if (!ll) return;

    ll_item_t* new_item = (ll_item_t*)malloc(sizeof(ll_item_t));
    if (!new_item) return;

    new_item->data = data;

    if (!(*ll))
    {
        new_item->prev = new_item->next = new_item;
        *ll = new_item;
    }
    else
    {
        new_item->prev = (*ll)->prev;
        new_item->next = (*ll);
        (*ll)->prev->next = new_item;
        (*ll)->prev = new_item;
    }
}

void ll_remove(ll_t* ll, ll_item_t* item)
{
    if (!item) return;
    if (!ll) return;
    if (!*ll) return;

    if (item->prev == item)
    {
    	*ll = NULL;
    	goto end;
    }
    else
    {
        item->prev->next = item->next;
        item->next->prev = item->prev;
    }

    if (item == *ll)
        *ll = item->next;

end:
    free(item);
}

void ll_destroy(ll_t* ll)
{
    if (!ll) return;
    if (!*ll) return;

    while ((*ll)->prev != *ll)
        ll_remove(ll, *ll);

    free(*ll);
    *ll = NULL;
}

ll_item_t* ll_find_item_by_data(ll_t* ll, void* data)
{
    if (!ll) return NULL;
    if (!*ll) return NULL;
    ll_item_t* it = *ll;
    do
    {
        if (it->data == data)
            return it;
        it = it->next;
    } while (it != *ll);
    return NULL;
}