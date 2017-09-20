#include "thread_defs.h"
#include <stdio.h>
#include <string.h>

#ifdef WIN32
typedef struct
{
    void *(*work_proc)(void *);
    void *arg;
} thread_proc_t;

DWORD WINAPI thread_agent(void *arg)
{
    thread_proc_t *proc = arg;
    if (proc != NULL);
    {
        proc->work_proc(proc->arg);
        free(proc);
        return 0;
    }
    return -1;
}
#endif

thread_handle_t create_thread(void *(*work_proc)(void *), void *arg)
{
#ifdef WIN32
    DWORD thread_id;
    thread_proc_t *proc;
    proc = malloc(sizeof(thread_proc_t));
    if (proc != NULL)
    {
        proc->work_proc = work_proc;
        proc->arg = arg;

        return CreateThread(
            NULL,                   // default security attributes
            0,                      // use default stack size  
            thread_agent,           // thread function name
            proc,                   // argument to thread function 
            0,                      // use default creation flags 
            &thread_id);            // returns the thread identifier
    }
    else
    {
        return NULL;
    }
#else
    int err;
    pthread_t ntid;
    err = pthread_create(&ntid, NULL, work_proc, arg);
    if (err != 0)
    {
        fprintf(stdout,
            "can't create thread: %s\n",
            strerror(err));
    }
    return ntid;
#endif
}

void wait_thread(thread_handle_t handle)
{
#ifdef WIN32
    WaitForSingleObject(handle, INFINITE);
#else
    pthread_join(handle, NULL);
#endif
}

void wait_threads(thread_handle_t *handles, int count)
{
#ifdef WIN32
    WaitForMultipleObjects(count, handles, TRUE, INFINITE);
#else
    int i;
    for (i = 0; i < count; i++)
    {
        pthread_join(handles[i], NULL);
    }
#endif
}

void close_thread_handle(thread_handle_t handle)
{
#ifdef WIN32
    CloseHandle(handle);
#else
#endif
}
