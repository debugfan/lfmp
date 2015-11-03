// implement for lockfree memory pool
// entry refer count:
//  0: freed.
//  1: first malloc or in list or pop from list uniquely
//  2: pop from list, and node is visitied possibly by others

#include "memory_pool.h"
#include <stdio.h>
#include <Windows.h>
#include <assert.h>

#define ENTRY_INITIAL_REFER_COUNT 1

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
    int li_rc;
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
            // avoid first entry is original first, but first->next isn't
            if (ENTRY_INITIAL_REFER_COUNT == InterlockedIncrement(&first->ref_cnt)) 
            {
                assert(first != li->next);
                InterlockedIncrement(&first->ref_cnt);
                done = 1;
            }
            else
            {
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
            }

            if (done == 0)
            {
                if (0 == InterlockedDecrement(&first->ref_cnt))
                {
                    if (ENTRY_INITIAL_REFER_COUNT == InterlockedIncrement(&first->ref_cnt))
                    {
                        InterlockedIncrement(&first->ref_cnt);
                        done = 1;
                    }
                    else
                    {
                        InterlockedDecrement(&first->ref_cnt);
                    }
                }
            }
        }

        li_rc = InterlockedDecrement(&li->ref_cnt);
        if (done != 0)
        {
            if (li_rc == 0)
            {
                // nobody else can read it from list, it's very safe now.
                InterlockedDecrement(&first->ref_cnt);
            }
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
}

int mp_slist_clear(mp_bucket_t *bucket, mp_slist_t *li)
{
    int n;
    mp_entry_t *first;
    mp_entry_t *next;
    first = InterlockedExchangePointer(&li->next, NULL);
    while (li->ref_cnt != 0); // safe for free
    n = 0;
    while (first != NULL)
    {
        next = first->next;
        InterlockedDecrement(&bucket->entries);
        free(first);
        first = next;
    }
    return n;
}

void mp_bucket_init(mp_bucket_t *bucket, 
    int block_size, 
    unsigned int threshold)
{
    mp_slist_init(&bucket->usable);
    mp_slist_init(&bucket->unusable);
    bucket->entries = 0;
    bucket->block_size = block_size;
    bucket->threshold = threshold;
}

void mp_bucket_clear(mp_bucket_t *bucket)
{
    mp_slist_clear(bucket, &bucket->usable);
    mp_slist_clear(bucket, &bucket->unusable);
}

void *mp_bucket_malloc(mp_bucket_t *bucket)
{
    int block_size;
    mp_entry_t *entry;
    block_size = bucket->block_size;
    entry = mp_slist_pop(&bucket->usable);
    if (entry == NULL)
    {
        entry = malloc(sizeof(mp_entry_t) + block_size);
        if (entry == NULL)
        {
            return NULL;
        }
        ((mp_entry_t *)entry)->size = block_size;
        ((mp_entry_t *)entry)->ref_cnt = ENTRY_INITIAL_REFER_COUNT;

        InterlockedIncrement(&bucket->entries);
    }

    return (void *)((unsigned char *)entry + sizeof(mp_entry_t));
}

void *mp_malloc(unsigned int size)
{
    int idx = mp_lookup_bucket(size);
    return mp_bucket_malloc(&g_memory_pool.buckets[idx]);
}

void mp_bucket_free(mp_bucket_t *bucket, mp_entry_t *entry)
{
    assert(entry->ref_cnt >= ENTRY_INITIAL_REFER_COUNT);
    if (entry->size * bucket->entries > bucket->threshold)
    {
        if (entry->ref_cnt == ENTRY_INITIAL_REFER_COUNT)
        {
            InterlockedDecrement(&bucket->entries);
            free(entry);
        }
        else
        {
            while (bucket->usable.ref_cnt != 0); // safe for free
            InterlockedDecrement(&bucket->entries);
            free(entry);
        }
    }
    else
    {
        if (entry->ref_cnt == ENTRY_INITIAL_REFER_COUNT)
        {
            mp_slist_push(&bucket->usable, entry);
        }
        else
        {
            InterlockedIncrement(&bucket->usable.ref_cnt);
            if (0 == InterlockedAdd(&entry->ref_cnt, -2))
            {
                if (ENTRY_INITIAL_REFER_COUNT == InterlockedIncrement(&entry->ref_cnt))
                {
                    mp_slist_push(&bucket->usable, entry);
                }
                else
                {
                    InterlockedDecrement(&entry->ref_cnt);
                }
            }
            InterlockedDecrement(&bucket->usable.ref_cnt);
        }
    }
}

void mp_free(void *p)
{
    mp_entry_t *entry;
    int idx;
    entry = (mp_entry_t *)((unsigned char *)p - sizeof(mp_entry_t));
    idx = mp_lookup_bucket(entry->size);
    mp_bucket_free(&g_memory_pool.buckets[idx], entry);
}

void mp_init(int max_usable, int flags)
{
    unsigned int threshold = max_usable / MEMORY_POOL_BUCKETS_NUMBER;;

    for (int i = 0; i < MEMORY_POOL_BUCKETS_NUMBER; i++)
    {
        mp_bucket_init(&g_memory_pool.buckets[i], 1 << i, threshold);
    }
}

void mp_clear()
{
    for (int i = 0; i < MEMORY_POOL_BUCKETS_NUMBER; i++)
    {
        mp_bucket_clear(&g_memory_pool.buckets[i]);
    }
}