#ifndef THREAD_DEFS_H
#define THREAD_DEFS_H

#ifdef __cplusplus
extern  "C"
{
#endif

#ifdef WIN32
#include <Windows.h>
#define thread_handle_t   HANDLE
#else
#include <pthread.h>
#define thread_handle_t   pthread_t 
#endif

thread_handle_t create_thread(void *(*thread_proc)(void *), void *arg);
void close_thread_handle(thread_handle_t handle);
void wait_thread(thread_handle_t handle);
void wait_threads(thread_handle_t *handles, int count);

#ifdef __cplusplus
}
#endif

#endif
