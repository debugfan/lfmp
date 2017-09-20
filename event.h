#ifndef EVENT_H
#define EVENT_H

#ifdef __cplusplus
extern  "C"
{
#endif

#include <errno.h>

#ifdef _WIN32
#include <Windows.h>
#define event_t HANDLE
#define wait_rc_t DWORD

#elif __linux__
#include <pthread.h>

typedef struct
{
    volatile int flag;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} event_t;

#define wait_rc_t       int
#define WAIT_TIMEOUT    ETIMEDOUT
#define INFINITE        0xFFFFFFFF
#define WAIT_OBJECT_0   0

#endif

void init_event(event_t *event);
void set_event(event_t *event);
wait_rc_t wait_event(event_t *event, unsigned int msec);
void reset_event(event_t *event);
void close_event(event_t *event);

#ifdef __cplusplus
}
#endif

#endif
