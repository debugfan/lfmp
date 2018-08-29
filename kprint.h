#ifndef MPSC_KPRINT_H
#define MPSC_KPRINT_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef NDIS_WDM
#include "bool_type.h"
#include <wdm.h>
#else
#include <stdio.h>
#include <Windows.h>
#endif

#ifndef MIN
#define MIN(_a, _b) ((_a) < (_b)? (_a): (_b))
#endif

void init_kprint_ring();
void kprint_ring_read(void(*handle_data)(LARGE_INTEGER *time, void *data, int length, int entry_flag, void *user_data),
	void *user_data);
void kprint_ring_wait_data();
void clear_kprint_ring();
BOOL kprint_ring_is_active();
void kprint_ring_close();
long kprint_ring_get_pending_write();

#ifdef NDIS_WDM
	int kprint(const char *s);
	int kprintf(const char *fmt, ...);
#else
#define kprint puts
#define kprintf printf
#endif

#define kdPrintString(x) KdPrint(("%s", x))

#define log_error(x) kprint x
#define log_warn(x) kprint x
#define log_info(x) kprint x
#define log_debug(x) kdPrintString x
#define log_trace(x)

#define log_errorf(x) kprintf x
#define log_warnf(x) kprintf x
#define log_infof(x) kprintf x
#define log_debugf KdPrint
#define log_tracef(x)

#ifdef __cplusplus
}
#endif

#endif
