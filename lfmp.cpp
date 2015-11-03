// lfmp.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "memory_pool.h"
#include <Windows.h>

#define ALLOC_TIMES     2000000

void *volatile ori_shared = NULL;
void *volatile mp_shared = NULL;

int nz_rand(int v)
{
    v = v * 214013 + 2531011;
    v = (v >> 16) & RAND_MAX;
    if (v <= 0)
    {
        v = 1;
    }
    return v;
}

int get_next_size(int v)
{
    return nz_rand(v);
}

DWORD WINAPI test_original_proc(LPVOID lpParam)
{
    LARGE_INTEGER start, end;
    LONGLONG sub;
    int id;
    int n = 1;
    id = (int)lpParam;
    QueryPerformanceCounter(&start);
    for (int i = 0; i < ALLOC_TIMES; i++)
    {
        void *p = malloc(n);
        n = get_next_size(n);
        if (p != NULL)
        {
            free(p);
        }
    }
    QueryPerformanceCounter(&end);
    sub = end.QuadPart - start.QuadPart;
    printf("thread id: %d(origin pool): 0x%I64x(%I64u)\n", id, sub, sub);
    return NULL;
}


DWORD WINAPI test_original_free_proc(LPVOID lpParam)
{
    LARGE_INTEGER start, end;
    LONGLONG sub;
    int id;
    int n = 1;
    id = (int)lpParam;
    QueryPerformanceCounter(&start);
    for (int i = 0; i < ALLOC_TIMES; i++)
    {
        void *v = InterlockedExchangePointer(&ori_shared, NULL);
        if (v != NULL)
        {
            free(v);
        }
        if (id == 0)
        {
            InterlockedExchangePointer(&ori_shared, malloc(n));
            n = get_next_size(n);
        }
    }
    QueryPerformanceCounter(&end);
    sub = end.QuadPart - start.QuadPart;
    printf("thread id: %d(origin pool free): 0x%I64x(%I64u)\n", id, sub, sub);
    return NULL;
}

DWORD WINAPI test_memory_pool_proc(LPVOID lpParam)
{
    LARGE_INTEGER start, end;
    LONGLONG sub;
    int id;
    int n = 1;
    id = (int)lpParam;
    QueryPerformanceCounter(&start);
    for (int i = 0; i < ALLOC_TIMES; i++)
    {
        void *p = mp_malloc(n);
        n = get_next_size(n);
        if (p != NULL)
        {
            mp_free(p);
        }
    }
    QueryPerformanceCounter(&end);
    sub = end.QuadPart - start.QuadPart;
    printf("thread id: %d(memory pool): 0x%I64x(%I64u)\n", id, sub, sub);
    return NULL;
}

DWORD WINAPI test_memory_pool_free_proc(LPVOID lpParam)
{
    LARGE_INTEGER start, end;
    LONGLONG sub;
    int id;
    int n = 1;
    id = (int)lpParam;
    QueryPerformanceCounter(&start);
    for (int i = 0; i < ALLOC_TIMES; i++)
    {
        void *v = InterlockedExchangePointer(&mp_shared, NULL);
        if (v != NULL)
        {
            mp_free(v);
        }
        if (id == 0)
        {
            InterlockedExchangePointer(&mp_shared, mp_malloc(n));
            n = get_next_size(n);
        }
    }
    QueryPerformanceCounter(&end);
    sub = end.QuadPart - start.QuadPart;
    printf("thread id: %d(memory pool free): 0x%I64x(%I64u)\n", id, sub, sub);
    return NULL;
}

void test_performance(int num_threads, 
    LPTHREAD_START_ROUTINE lpStartAddress)
{
    DWORD *dwThreadId;
    HANDLE *hThread;
    dwThreadId = (DWORD *)malloc(num_threads * sizeof(dwThreadId));
    hThread = (HANDLE *)malloc(num_threads * sizeof(HANDLE));

    for (int i = 0; i < num_threads; i++)
    {
        hThread[i] = CreateThread(
            NULL,              // default security attributes
            0,                 // use default stack size  
            lpStartAddress,    // thread function 
            (LPVOID)i,         // argument to thread function 
            0,                 // use default creation flags 
            &dwThreadId[i]);   // returns the thread identifier 

        if (hThread[i] == NULL)
        {
            for (i = 0; i < num_threads; i++)
            {
                if (hThread[i] != NULL)
                {
                    CloseHandle(hThread[i]);
                }
            }
        }
    }

    // Wait until all threads have terminated.
    WaitForMultipleObjects(num_threads, hThread, TRUE, INFINITE);

    for (int i = 0; i < num_threads; i++)
    {
        if (hThread[i] != NULL)
        {
            CloseHandle(hThread[i]);
        }
    }

    free(dwThreadId);
    free(hThread);
}

int _tmain(int argc, _TCHAR* argv[])
{
    mp_init(65536*1024*8, 0);

    test_performance(4, test_memory_pool_proc);
    test_performance(4, test_original_proc);

    test_performance(4, test_memory_pool_free_proc);
    test_performance(4, test_original_free_proc);

    mp_clear();

    printf("small memory test.\n");

    mp_init(65536 * 8, 0);

    test_performance(4, test_memory_pool_proc);
    test_performance(4, test_original_proc);

    test_performance(4, test_memory_pool_free_proc);
    test_performance(4, test_original_free_proc);

    mp_clear();
	return 0;
}
