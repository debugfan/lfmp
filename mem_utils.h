#ifndef MEM_UTILS_H
#define MEM_UTILS_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef NDIS_WDM
#include <wdm.h>
#else
#include <stddef.h>
#endif

void *memory_alloc(size_t size, unsigned long tag);
void memory_free(void *ptr);
void check_memory();

#ifdef __cplusplus
}
#endif

#endif
