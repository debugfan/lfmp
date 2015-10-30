#include "memory_pool.h"
#include <stdio.h>
#include <Windows.h>
#include <assert.h>

memory_pool_t g_memory_pool;

void mp_slist_push(mp_slist_t *li, mp_entry_t *entry)
{
    mp_entry_t *first;
    for (;;) {
        first = li->next;
        entry->next = first;
        if (first == InterlockedCompareExchangePointer(&li->next,
            entry,
            first))
        {
            break;
        }
    }
}

mp_entry_t *mp_slist_pop(mp_slist_t *li)
{
    int done;
    mp_entry_t *first;
    mp_entry_t *next;
    done = 0;
    while (done == 0)
    {
        first = li->next;
        if (first == NULL)
        {
            break;
        }
        InterlockedIncrement(&li->ref_cnt); // avoid the following first entry is removed
        if (first == li->next)
        {
            InterlockedIncrement(&first->ref_cnt); // avoid first entry is original first, but first->next isn't
            if (first == li->next)
            {
                next = first->next;
                if (first == InterlockedCompareExchangePointer(&li->next,
                    next,
                    first))
                {
                    assert(next == first->next);
                    done = 1;
                }
            }
            InterlockedDecrement(&first->ref_cnt);
        }
        InterlockedDecrement(&li->ref_cnt);
        if (done != 0)
        {
            break;
        }
    }
    return first;
}

int mp_lookup_bucket(unsigned int size)
{
    int idx = 0;
    if (size >= 1)
    {
        size--;
    }
    while (size > 0)
    {
        size = size >> 1;
        idx++;
    }
    return idx;
}

void mp_slist_init(mp_slist_t *li)
{
    li->ref_cnt = 0;
    li->next = NULL;
    li->entries = 0;
}

void mp_slist_clear(mp_slist_t *li)
{
    mp_entry_t *first;
    mp_entry_t *next;
    first = InterlockedExchangePointer(&li->next, NULL);
    while (li->ref_cnt != 0); // safe for free
    while (first != NULL)
    {
        next = first->next;
        InterlockedDecrement(&li->entries);
        free(first);
        first = next;
    }
}

void mp_init(int max_usable, int flags)
{
    for (int i = 0; i < MEMORY_POOL_BUCKETS_NUMBER; i++)
    {
        mp_slist_init(&g_memory_pool.buckets[i]);
        g_memory_pool.buckets[i].size = (1 << i);
    }
    g_memory_pool.threshold = max_usable / MEMORY_POOL_BUCKETS_NUMBER;
}

void mp_clear()
{
    for (int i = 0; i < MEMORY_POOL_BUCKETS_NUMBER; i++)
    {
        mp_slist_clear(&g_memory_pool.buckets[i]);
    }
}

void *mp_malloc(unsigned int size)
{
    int idx = 0;
    int block_size;
    mp_entry_t *entry;
    idx = mp_lookup_bucket(size);
    block_size = g_memory_pool.buckets[idx].size;
    entry = mp_slist_pop(&g_memory_pool.buckets[idx]);
    
    if (entry == NULL)
    {
        entry = malloc(sizeof(mp_entry_t) + block_size);
        if (entry == NULL)
        {
            return NULL;
        }
        ((mp_entry_t *)entry)->size = block_size;
        ((mp_entry_t *)entry)->ref_cnt = 0;
        InterlockedIncrement(&g_memory_pool.buckets[idx].entries);
    }
    
    return (void *)((unsigned char *)entry + sizeof(mp_entry_t));
}

void mp_free(void *p)
{
    unsigned size = 0;
    int idx = 0;
    mp_entry_t *entry = (mp_entry_t *)((unsigned char *)p - sizeof(mp_entry_t));
    size = entry->size;
    idx = mp_lookup_bucket(size);
    if (size * g_memory_pool.buckets[idx].entries > g_memory_pool.threshold)
    {
        while (g_memory_pool.buckets[idx].ref_cnt != 0); // safe for free
        InterlockedDecrement(&g_memory_pool.buckets[idx].entries);
        free(entry);
    }
    else
    {
        while (entry->ref_cnt != 0);
        mp_slist_push(&g_memory_pool.buckets[idx], entry);
    }
}