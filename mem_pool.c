// implement for lockfree memory pool
// entry refer count:
//  0: reserve for being freed.
//  1: first malloc or in list or pop from list uniquely
//  2: pop from list, and node is tried to access possibly by others

#include "mem_pool.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "interlocked_defs.h"
#include "event.h"
#include "mem_utils.h"

#ifdef WIN32
#include <Windows.h>
#else
#include <sys/sysinfo.h>
#endif

#define MP_ENTRY_INITIAL_REFER_COUNT 1
#define MP_ALIGN_SIZE (sizeof(void *)*4)
#define MP_ENTRY_HEADER_SIZE ((sizeof(mp_entry_t) - 1)/MP_ALIGN_SIZE + 1)*MP_ALIGN_SIZE

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
        InterlockedIncrement(&li->ref_cnt); // avoid the following first entry is freed from memory
        if (first == li->next)
        {
            // avoid first entry is original first, but first->next isn't
            InterlockedIncrement(&first->ref_cnt);
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

            if (done == 0)
            {
                if (MP_ENTRY_INITIAL_REFER_COUNT == InterlockedDecrement(&first->ref_cnt))
                {
                    if (0 == InterlockedCompareExchange(&first->owned, 1, 0))
                    {
                        assert(first != li->next);
                        InterlockedIncrement(&first->ref_cnt);
                        done = 1;
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
        memory_free(first);
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
	bucket->next = NULL;
}

void mp_bucket_clear(mp_bucket_t *bucket)
{
    mp_slist_clear(bucket, &bucket->usable);
    mp_slist_clear(bucket, &bucket->unusable);
}

void *mp_bucket_malloc(mp_bucket_t *bucket, size_t size, unsigned long tag)
{
    int block_size;
    mp_entry_t *entry;
	if (bucket == NULL)
	{
		int idx = mp_lookup_bucket(size);
		bucket = &g_memory_pool.buckets[idx];
	}
    block_size = bucket->block_size;
	if (size > (size_t)block_size)
	{
		return NULL;
	}
    entry = mp_slist_pop(&bucket->usable);
    if (entry == NULL)
    {
		entry = memory_alloc(MP_ENTRY_HEADER_SIZE + block_size, tag);
        if (entry == NULL)
        {
            return NULL;
        }
        ((mp_entry_t *)entry)->size = block_size;
        ((mp_entry_t *)entry)->ref_cnt = MP_ENTRY_INITIAL_REFER_COUNT;
        ((mp_entry_t *)entry)->owned = 1;

        InterlockedIncrement(&bucket->entries);
    }

	return (void *)((unsigned char *)entry + MP_ENTRY_HEADER_SIZE);
}

void mp_bucket_free_entry(mp_bucket_t *bucket, mp_entry_t *entry)
{
    assert(entry->ref_cnt >= MP_ENTRY_INITIAL_REFER_COUNT);
    if (entry->size * (bucket->entries + 1) > bucket->threshold)
    {
        if (entry->ref_cnt == MP_ENTRY_INITIAL_REFER_COUNT)
        {
            InterlockedDecrement(&bucket->entries);
			memory_free(entry);
        }
        else
        {
#ifdef USE_FREE_THREAD
            if (bucket->usable.ref_cnt == 0)
            {
                InterlockedDecrement(&bucket->entries);
                memory_free(entry);
            }
            else
            {
                mp_slist_push(&bucket->unusable, entry);
                InterlockedExchange(&g_memory_pool.require_free, 1);
            }
#else
            while (bucket->usable.ref_cnt != 0); // safe for free
            InterlockedDecrement(&bucket->entries);
			memory_free(entry);
#endif
        }
    }
    else
    {
        if (entry->ref_cnt == MP_ENTRY_INITIAL_REFER_COUNT)
        {
            mp_slist_push(&bucket->usable, entry);
        }
        else
        {
            InterlockedExchange(&entry->owned, 0);
            InterlockedIncrement(&bucket->usable.ref_cnt);
            if (MP_ENTRY_INITIAL_REFER_COUNT == InterlockedDecrement(&entry->ref_cnt))
            {
                if (InterlockedCompareExchange(&entry->owned, 1, 0) == 0)
                {
                    mp_slist_push(&bucket->usable, entry);
                }
            }
            InterlockedDecrement(&bucket->usable.ref_cnt);
        }
    }
}

void mp_bucket_free(mp_bucket_t *bucket, void *p)
{
	mp_entry_t *entry;
	entry = (mp_entry_t *)((unsigned char *)p - MP_ENTRY_HEADER_SIZE);
	if (bucket == NULL)
	{
		int idx = mp_lookup_bucket(entry->size);
		bucket = &g_memory_pool.buckets[idx];
	}
	mp_bucket_free_entry(bucket, entry);
}

void *free_thread_proc(void *param)
{
    mp_entry_t *first;
    mp_entry_t *next;
    int i;
    for (;;)
    {
        while (0 != g_memory_pool.require_free)
        {
            InterlockedExchange(&g_memory_pool.require_free, 0);

            for (i = 0; i < MEMORY_POOL_BUCKETS_NUMBER; i++)
            {
                first = InterlockedExchangePointer(&g_memory_pool.buckets[i].unusable.next, NULL);
                if (first != NULL)
                {
                    while (g_memory_pool.buckets[i].usable.ref_cnt != 0);
                    while (first != NULL)
                    {
                        next = first->next;
                        InterlockedDecrement(&g_memory_pool.buckets[i].entries);
                        free(first);
                        first = next;
                    }
                }
            }
        }

        if (WAIT_TIMEOUT != wait_event(&g_memory_pool.termin_event, 1000))
        {
            break;
        }
    }
    printf("memory pool free thread exited.\n");
    return 0;
}

#ifdef WIN32
unsigned __int64 get_total_memroy()
#else
unsigned long get_total_memroy()
#endif
{
#ifdef WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    return statex.ullTotalPhys;
#else
    struct sysinfo info;
    sysinfo(&info);
    return info.totalram;
#endif
}

void mp_init(int usable_percents, int min_usable)
{
    int i;
#ifdef WIN32
    unsigned __int64 max_usable;
    unsigned __int64 threshold;
#else
    unsigned long max_usable;
    unsigned long threshold;
#endif
    max_usable = get_total_memroy() / 100 * usable_percents;
    if (max_usable < min_usable)
    {
        max_usable = min_usable;
    }
    threshold = max_usable / MEMORY_POOL_BUCKETS_NUMBER;
    if (threshold > 0x7fffffff)
    {
        threshold = 0x7fffffff;
    }
    for (i = 0; i < MEMORY_POOL_BUCKETS_NUMBER; i++)
    {
        mp_bucket_init(&g_memory_pool.buckets[i], 1 << i, (unsigned int)threshold);
    }
	g_memory_pool.next_register = NULL;
    g_memory_pool.require_free = 0;
    init_event(&g_memory_pool.termin_event);

    g_memory_pool.free_thread = create_thread(free_thread_proc, NULL);
}

void mp_register_bucket(mp_bucket_t *bucket, int block_size, unsigned int threshold)
{
	mp_bucket_init(bucket, block_size, threshold);
	mp_bucket_t *first;
	for (;;) {
		first = g_memory_pool.next_register;
		bucket->next = first;
		if (first == InterlockedCompareExchangePointer(&g_memory_pool.next_register,
			first,
			bucket))
		{
			break;
		}
	}
}

void mp_clear_register_bucket()
{
	mp_bucket_t *bucket;
	bucket = g_memory_pool.next_register;
	while (bucket != NULL)
	{
		mp_bucket_clear(bucket);
		bucket = bucket->next;
	}
}

void mp_clear()
{
    int i;
    set_event(&g_memory_pool.termin_event);
    wait_thread(g_memory_pool.free_thread);
    close_event(&g_memory_pool.termin_event);
    for (i = 0; i < MEMORY_POOL_BUCKETS_NUMBER; i++)
    {
        mp_bucket_clear(&g_memory_pool.buckets[i]);
    }
	mp_clear_register_bucket();
	check_memory();
}

void mp_print()
{
    int i;
    printf("memory pool bucket entries:\n");
    for (i = 0; i < MEMORY_POOL_BUCKETS_NUMBER; i++)
    {
        printf("[%2d] = %10d, ", 
            i,
            g_memory_pool.buckets[i].entries);
        if ((i+1) % 4 == 0)
        {
            printf("\n");
        }
    }
}
