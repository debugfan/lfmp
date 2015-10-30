#ifndef MEMORY_POOL_H_D3225843_3A1B_451f_A1E7_52AD6DC808F2
#define MEMORY_POOL_H_D3225843_3A1B_451f_A1E7_52AD6DC808F2

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _mp_entry
{
    struct _mp_entry *next;
    unsigned int size;
    volatile int ref_cnt;
} mp_entry_t;

typedef struct
{
    mp_entry_t * volatile next;
    volatile int ref_cnt;
    volatile int entries;
    unsigned int size;
} mp_slist_t;

#define MEMORY_POOL_BUCKETS_NUMBER  28

typedef struct
{
    // 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 ...
    mp_slist_t buckets[MEMORY_POOL_BUCKETS_NUMBER];
    unsigned int threshold;
} memory_pool_t;

#define USE_COLLECT_THREAD  0x01

void mp_init(int max_usable, int flags);
void *mp_malloc(unsigned int size);
void mp_free(void *p);
void mp_clear();

#ifdef __cplusplus
}
#endif

#endif
