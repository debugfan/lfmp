#include "mem_utils.h"
#include "kprint.h"

#ifdef NDIS_WDM
#include "Ntstrsafe.h"
#pragma warning(disable:4201)
#include <ndis.h>
#else
#include <stdio.h>
#endif

typedef struct {
	long volatile alloc_num;
	long volatile free_num;
} memory_counter_t;

memory_counter_t g_memory_counter[4][256] = { 0 };

#define USE_MEMORY_COUNTER
#ifdef USE_MEMORY_COUNTER
#define ALLOC_HEADER_SIZE sizeof(void *)*4
#else
#define ALLOC_HEADER_SIZE 0
#endif

void memory_count(unsigned long tag, int alloc_or_free)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		if (alloc_or_free != 0)
		{
			InterlockedIncrement(&g_memory_counter[i][(tag >> (i * 8)) & 0xFF].alloc_num);
		}
		else
		{
			InterlockedIncrement(&g_memory_counter[i][(tag >> (i * 8)) & 0xFF].free_num);
		}
	}
}

void *internal_memory_alloc(size_t size, unsigned int tag)
{
#ifdef NDIS_WDM
	NDIS_STATUS status;
	void *ptr;
	status = NdisAllocateMemoryWithTag(&ptr, (unsigned int)size, tag);
	if (NDIS_STATUS_SUCCESS == status)
	{
		return ptr;
	}
	else
	{
		log_errorf(("[%s]Memory alloc failed. NDIS status: 0x%x\n",
			__FUNCTION__,
			status));
		return NULL;
	}
#else
	return malloc(size);
#endif
}

void internal_memory_free(void *p)
{
#ifdef NDIS_WDM
	NdisFreeMemory(p, 0, 0);
#else
	free(p);
#endif
}

void *memory_alloc(size_t size, unsigned long tag)
{
	void *ptr;
	size_t total_size;
	total_size = size + ALLOC_HEADER_SIZE;
	ptr = internal_memory_alloc(total_size, tag);
	if (ptr != NULL)
	{
#ifdef USE_MEMORY_COUNTER
		memory_count(*(unsigned long *)ptr, 1);
#endif
		return (char *)ptr + ALLOC_HEADER_SIZE;
	}
	else
	{
		return NULL;
	}
}

void memory_free(void *ptr)
{
	void *actual = (char *)ptr - ALLOC_HEADER_SIZE;
	if (ptr != NULL)
	{
#ifdef USE_MEMORY_COUNTER
		memory_count(*(unsigned long *)actual, 0);
#endif
	}
	internal_memory_free(actual);
}

char get_printable(int c)
{
	if (c >= 20 && c < 127)
	{
		return (char)c;
	}
	else
	{
		return '?';
	}
}

void check_memory()
{
	char line[1024];
	int i, j;
	int leaked = 0;
	for (i = 0; i < 4; i++)
	{
		line[0] = '\0';
		for (j = 0; j < 256; j++)
		{
			if (g_memory_counter[i][j].alloc_num != g_memory_counter[i][j].free_num)
			{
				char record[64];
#ifdef NDIS_WDM
				if (STATUS_SUCCESS == RtlStringCbPrintfA(record,
					sizeof(record),
					" \t%c(%d, %d)",
					get_printable(j),
					g_memory_counter[i][j].alloc_num,
					g_memory_counter[i][j].free_num))
				{
					if (STATUS_SUCCESS != RtlStringCchCatA(line, sizeof(line), record))
					{
						log_errorf(("[%s]RtlStringCchCatA failed. record: %s\r\n", 
							__FUNCTION__,
							record));
					}
				}
				else
				{
					log_errorf(("[%s]RtlStringCbPrintfA failed\r\n", __FUNCTION__));
				}
#else
				if (0 < sprintf_s(record,
					sizeof(record),
					" \t%c(%d, %d)",
					get_printable(j),
					g_memory_counter[i][j].alloc_num,
					g_memory_counter[i][j].free_num))
				{
					strcat_s(line, sizeof(line), record);
				}
				
#endif
				leaked = 1;
			}
		}

		if (leaked != 0)
		{
			if (i == 0)
			{
				log_error(("---------- Warning: found potential memory leak ----------\r\n"));
			}
			log_errorf(("%s\r\n", line));
		}
	}
}
