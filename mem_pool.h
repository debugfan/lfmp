#ifndef MEM_POOL_H_D3225843_3A1B_451f_A1E7_52AD6DC808F2
#define MEM_POOL_H_D3225843_3A1B_451f_A1E7_52AD6DC808F2

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include "thread_defs.h"
#include "event.h"

#define USE_FREE_THREAD

typedef struct _mp_entry
{
    struct _mp_entry *next;
    unsigned int size;
    volatile int ref_cnt;
    volatile int owned;
} mp_entry_t;

typedef struct
{
    mp_entry_t * volatile next;
    volatile int ref_cnt;
} mp_slist_t;

typedef struct _mp_bucket_t
{
	struct _mp_bucket_t *next;
    mp_slist_t usable;
#ifdef USE_FREE_THREAD
    mp_slist_t unusable;
#endif
    unsigned int block_size;
    volatile int entries;
    unsigned int threshold;
} mp_bucket_t;

#define MEMORY_POOL_BUCKETS_NUMBER  20

typedef struct
{
    // 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, ...
    mp_bucket_t buckets[MEMORY_POOL_BUCKETS_NUMBER];
	mp_bucket_t *next_register;
#ifdef USE_FREE_THREAD
    thread_handle_t free_thread;
    volatile int require_free;
    event_t termin_event;
#endif
} memory_pool_t;

void mp_init(int usable_percents, int min_usable);
void mp_register_bucket(mp_bucket_t *bucket, int block_size, unsigned int threshold);
void *mp_bucket_malloc(mp_bucket_t *bucket, size_t size, unsigned long tag);
void mp_bucket_free(mp_bucket_t *bucket, void *p);
void mp_clear();
void mp_print();

static __inline void *mp_malloc(size_t n) { return mp_bucket_malloc(NULL, n, 'pmfl'); }
static __inline void mp_free(void *p) { mp_bucket_free(NULL, p); }

#ifdef __cplusplus
}
#endif

#endif
