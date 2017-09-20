#ifndef INTERLOCKED_H
#define INTERLOCKED_H

#ifdef _WIN32
#include <Windows.h>
#elif defined(__linux__)

#define InterlockedCompareExchange(d, e, c)         __sync_val_compare_and_swap(d, c, e)
#define InterlockedCompareExchangePointer(d, e, c)  __sync_val_compare_and_swap(d, c, e)
#define InterlockedExchange                         __sync_lock_test_and_set 
#define InterlockedIncrement(x)                     __sync_add_and_fetch(x, 1)
#define InterlockedDecrement(x)                     __sync_sub_and_fetch(x, 1)
#define InterlockedRead(x)                          x
#define InterlockedAdd(x, v)                        __sync_add_and_fetch(x, v)
#define InterlockedSub(x, v)                        __sync_sub_and_fetch(x, v)

//#ifdef NO_USE_WALL_FLAGS
#define InterlockedExchangePointer                  __sync_lock_test_and_set 
//#else
//static __inline void *InterlockedExchangePointer(void *volatile *ptr, void *value)
//{
//    return __sync_lock_test_and_set(ptr, value);
//}
//#endif

#endif

#endif
