/*! \file heap.h
 \brief Heap management functions.
*/

#ifndef __SDL_PS3_HEAP_H__
#define __SDL_PS3_HEAP_H__

#include <sys/integertypes.h>

#define CPU_ALIGNMENT				8

#define HEAP_BLOCK_PREV_USED		((uintptr_t) 1)
#define HEAP_BLOCK_ALLOC_BONUS		(sizeof(uintptr_t))
#define HEAP_BLOCK_HEADER_SIZE		(2*sizeof(uintptr_t))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _heap_block_t heap_block;
typedef struct _heap_cntrl_t heap_cntrl;

struct _heap_block_t
{
	uintptr_t prev_size;
	uintptr_t size;
	heap_block *next;
	heap_block *prev;
} __attribute__((packed));

struct _heap_cntrl_t
{
	int32_t lock;
	
	heap_block free_list;
	
	uintptr_t page_size;
	uintptr_t min_block_size;

	uintptr_t heap_begin;
	uintptr_t heap_end;

	heap_block *first_block;
	heap_block *last_block;
};

uintptr_t heapInit(heap_cntrl *theheap, void *start_addr, uintptr_t size);
void* heapAllocate(heap_cntrl *theheap, uintptr_t size);
void* heapAllocateAligned(heap_cntrl *theheap, uintptr_t size, uintptr_t alignment);
char heapFree(heap_cntrl *theheap, void *ptr);

#ifdef __cplusplus
	}
#endif

#endif
