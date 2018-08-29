// lfmp.cpp : Defines the entry point for the console application.
//
//#include "stdafx.h"
#include "mem_pool.h"
#include "thread_defs.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "interlocked_defs.h"

#define ALLOC_TIMES     100000

void *volatile ori_shared = NULL;
void *volatile mp_shared = NULL;

#define NZ_RAND_MAX 0x7fff

int nz_rand(int v)
{
    v = v * 214013 + 2531011;
    v = (v >> 16) & NZ_RAND_MAX;
    if (v <= 0)
    {
        v = 1;
    }
    return v;
}

#define INITIAL_ALLOC_SIZE  1
//#define FIXED_ALLOC_TEST
#define MALLOC_ARRAY_SIZE   8

int get_next_size(int v)
{
#ifdef FIXED_ALLOC_TEST
    return v;
#else
    return nz_rand(v);
#endif
}

void alloc_free_test(void *param, void *(*alloc_func)(size_t size), void(*free_func)(void *))
{
    int n = INITIAL_ALLOC_SIZE;
    void *p[MALLOC_ARRAY_SIZE];
    for (int i = 0; i < ALLOC_TIMES; i++)
    {
        for (int j = 0; j < MALLOC_ARRAY_SIZE; j++)
        {
            p[j] = alloc_func(n);
#ifdef HAVE_ASSIGN
            for (int k = 0; k < 1; k++)
            {
                ((unsigned char *)p[j])[k] = ((1 + n) & 0xFF);
            }
#endif
            n = get_next_size(n);
        }

        for (int j = 0; j < MALLOC_ARRAY_SIZE; j++)
        {
            if (p[j] != NULL)
            {
                free_func(p[j]);
            }
        }
    }
}

void vie_free_test(void *param, void *(*alloc_func)(size_t size), void(*free_func)(void *),
    void * volatile * shared)
{
    void *v;
    int n = INITIAL_ALLOC_SIZE;
    int id = (long)param;
    for (int i = 0; i < ALLOC_TIMES; i++)
    {
        v = InterlockedExchangePointer(shared, NULL);
        if (v != NULL)
        {
            free_func(v);
        }
        if (id == 0)
        {
            void *p = alloc_func(n);
#ifdef HAVE_ASSIGN
            for (int j = 0; j < n; j++)
            {
                ((unsigned char *)p)[j] = ((1 + n) & 0xFF);
            }
#endif
            InterlockedExchangePointer(shared, p);
            n = get_next_size(n);
        }
    }
    v = InterlockedExchangePointer(shared, NULL);
    if (v != NULL)
    {
        free_func(v);
    }
}

void original_alloc(void *param)
{
    alloc_free_test(param, malloc, free);
}

void original_vie_free(void *param)
{
    vie_free_test(param, malloc, free, &ori_shared);
}

void mem_pool_alloc(void *param)
{
    alloc_free_test(param, mp_malloc, mp_free);
}

void mem_pool_vie_free(void *param)
{
    vie_free_test(param, mp_malloc, mp_free, &mp_shared);
}

#ifndef WIN32
timespec diff_time(timespec end, timespec start)
{
    timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    }
    else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}
#endif

void calc_function_run_time(int thread_id, const char *func_name, void(*func)(void *), void *arg)
{
#ifdef WIN32
    LARGE_INTEGER start, end;
    LONGLONG sub;
#else
    timespec start, end;
    timespec sub;
#endif

#ifdef WIN32
    QueryPerformanceCounter(&start);
#else
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
#endif
    func(arg);
#ifdef WIN32
    QueryPerformanceCounter(&end);
    sub = end.QuadPart - start.QuadPart;
    printf("thread[%d] %s: %I64u\n",
        thread_id,
        func_name,
        sub);
#else
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end);
    sub = diff_time(end, start);
    printf("thread[%d] %s: %lld.%.9ld\n",
        thread_id,
        func_name,
        (long long)sub.tv_sec,
        sub.tv_nsec);
#endif
}

void *test_original_proc(void *param)
{
    int id;
    id = (int)(long)param;
    calc_function_run_time(id, "original_alloc", original_alloc, (void *)(long)id);
    return NULL;
}

void *test_original_free_proc(void *param)
{
    int id;
    id = (int)(long)param;
    calc_function_run_time(id, "original_vie_free", original_vie_free, (void *)(long)id);
    return NULL;
}

void *test_memory_pool_proc(void *param)
{
    int id;
    id = (int)(long)param;
    calc_function_run_time(id, "mem_pool_alloc", mem_pool_alloc, (void *)(long)id);
    return NULL;
}

void *test_memory_pool_free_proc(void *param)
{
    int id;
    id = (int)(long)param;
    calc_function_run_time(id, "mem_pool_vie_free", mem_pool_vie_free, (void *)(long)id);
    return NULL;
}

void test_performance(int num_threads,
    void *(*lpStartAddress)(void *))
{
    thread_handle_t *hThread;
    hThread = (thread_handle_t *)malloc(num_threads * sizeof(thread_handle_t));

    for (int i = 0; i < num_threads; i++)
    {    
        hThread[i] = create_thread(lpStartAddress, (void *)(long)i);
    }

    // Wait until all threads have terminated.
    wait_threads(hThread, num_threads);

    for (int i = 0; i < num_threads; i++)
    {
        if (hThread[i] != NULL)
        {
            close_thread_handle(hThread[i]);
        }
    }

    free(hThread);
}

void do_test(int usable_memory, int num_threads)
{
    printf("----------------------------------------\n");
    printf("num_threads: %d, usable_memory: %d\n", num_threads, usable_memory);
    mp_init(usable_memory, 65535 * num_threads);
    printf("alloc and free test.\n");
    test_performance(num_threads, test_memory_pool_proc);
    test_performance(num_threads, test_original_proc);
    test_performance(num_threads, test_memory_pool_proc);
    mp_print();
    printf("vie for free test.\n");
    test_performance(num_threads, test_memory_pool_free_proc);
    test_performance(num_threads, test_original_free_proc);
    test_performance(num_threads, test_memory_pool_free_proc);
    mp_print();
    mp_clear();
    mp_print();
}

int main(int argc, char* argv[])
{
    // test with sufficient memory;
    do_test(10, 1);
    do_test(10, 4);
    // test with insufficient memory;
    do_test(0, 1);
    do_test(0, 4);
	return 0;
}
