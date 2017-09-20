#include "event.h"
#include <stdio.h>
#include <stdlib.h>
#include "interlocked_defs.h"
#include <time.h>

#ifndef WIN32
#include <sys/time.h>
#endif

#define INFINITE 0xFFFFFFFF

void init_event(event_t *event)
{
#ifdef WIN32
    *event = CreateEvent(NULL, FALSE, FALSE, NULL);
#elif defined(__linux__)
    pthread_cond_init(&event->cond, NULL);
    pthread_mutex_init(&event->mutex, NULL);
#endif
}

void set_event(event_t *event)
{
#ifdef WIN32
    SetEvent(*event);
#elif defined(__linux__)
    InterlockedExchange(&event->flag, 1);

    pthread_mutex_lock(&event->mutex);
    pthread_cond_signal(&event->cond);
    pthread_mutex_unlock(&event->mutex);
#endif
}

wait_rc_t wait_event(event_t *event, unsigned int msec)
{
#ifdef WIN32
    return WaitForSingleObject(*event, msec);
#elif defined(__linux__)
    int rc;

    if (InterlockedRead(event->flag) == 0)
    {
        if (msec == INFINITE)
        {
            pthread_mutex_lock(&event->mutex);
            if (InterlockedRead(event->flag) == 0)
            {
                rc = pthread_cond_wait(&event->cond, &event->mutex);
            }
            else
            {
                InterlockedCompareExchange(&event->flag, 0, 1);
                rc = WAIT_OBJECT_0;
            }
            pthread_mutex_unlock(&event->mutex);
        }
        else
        {
            int sec;
            struct timeval now;
            struct timespec timeout;
            if (msec < 1000)
            {
                sec = 1;
            }
            else
            {
                sec = msec / 1000;
            }
            gettimeofday(&now, NULL);
            timeout.tv_sec = now.tv_sec + sec;
            timeout.tv_nsec = now.tv_usec * 1000;

            pthread_mutex_lock(&event->mutex);
            if (InterlockedRead(event->flag) == 0)
            {
                rc = pthread_cond_timedwait(&event->cond, &event->mutex, &timeout);
            }
            else
            {
                InterlockedCompareExchange(&event->flag, 0, 1);
                rc = WAIT_OBJECT_0;
            }
            pthread_mutex_unlock(&event->mutex);
        }
    }
    else
    {
        InterlockedCompareExchange(&event->flag, 0, 1);
        rc = WAIT_OBJECT_0;
    }
    return rc;
#endif
}

void reset_event(event_t *event)
{
#ifdef WIN32
    ResetEvent(*event);
#elif defined(__linux__)
    InterlockedExchange(&event->flag, 0);
#endif
}

void close_event(event_t *event)
{
#ifdef WIN32
    CloseHandle(*event);
#elif defined(__linux__)
#endif
}
