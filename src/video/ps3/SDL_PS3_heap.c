#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "SDL_PS3_heap.h"
#include "SDL_PS3_heap.inl"

#include <sys/spinlock.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif // MIN

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif // MAX

static char __heap_get_first_last_block(uintptr_t heap_begin, uintptr_t heap_size, uintptr_t page_size, uintptr_t min_block_size, heap_block **first_block, heap_block **last_block)
{
	uintptr_t const heap_end = heap_begin + heap_size;
	uintptr_t const alloc_begin = __heap_align_up(heap_begin + HEAP_BLOCK_HEADER_SIZE, page_size);
	uintptr_t const first_block_begin = alloc_begin - HEAP_BLOCK_HEADER_SIZE;
	uintptr_t const overhead = HEAP_BLOCK_HEADER_SIZE + (first_block_begin - heap_begin);
	uintptr_t const first_block_size = __heap_align_down(heap_size - overhead, page_size);
	heap_block *start_block = (heap_block*)first_block_begin;
	heap_block *end_block = __heap_block_at(start_block, first_block_size);

	if (heap_end < heap_begin || heap_size <= overhead || first_block_size < min_block_size)
		return 0;

	*first_block = start_block;
	*last_block = end_block;

	return 1;
}

static void __heap_block_split(heap_cntrl *theheap, heap_block *block, heap_block *next_block, heap_block *free_anchor, uintptr_t alloc_size)
{
	uintptr_t const page_size = theheap->page_size;
	uintptr_t const min_block_size = theheap->min_block_size;
	uintptr_t const min_alloc_size = min_block_size - HEAP_BLOCK_HEADER_SIZE;

	uintptr_t const block_size = __heap_block_size(block);
	uintptr_t const used_size = MAX(alloc_size, min_alloc_size) + HEAP_BLOCK_HEADER_SIZE;
	uintptr_t const used_block_size = __heap_align_up(used_size, page_size);

	uintptr_t const free_size = block_size + HEAP_BLOCK_ALLOC_BONUS - used_size;
	uintptr_t const free_size_limit = min_block_size + HEAP_BLOCK_ALLOC_BONUS;

	_HAssert(used_size <= block_size + HEAP_BLOCK_ALLOC_BONUS);
	_HAssert(used_size + free_size == block_size + HEAP_BLOCK_ALLOC_BONUS);

	if (free_size >= free_size_limit) {
		heap_block *const free_block = __heap_block_at(block, used_block_size);
		uintptr_t free_block_size = block_size - used_block_size;
		uintptr_t const next_block_size = __heap_block_size(next_block);
		heap_block *const next_next_block = __heap_block_at(next_block, next_block_size);

		_HAssert(used_block_size + free_block_size == block_size);

		__heap_block_set_size(block, used_block_size);

		if (__heap_prev_used(next_next_block))
			__heap_block_insert_after(free_anchor, free_block);
		else {
			__heap_block_replace(next_block, free_block);

			free_block_size += next_block_size;
			next_block = __heap_block_at(free_block, free_block_size);
		}

		free_block->size = free_block_size|HEAP_BLOCK_PREV_USED;

		next_block->prev_size = free_block_size;
		next_block->size &= ~HEAP_BLOCK_PREV_USED;
	} else
		next_block->size |= HEAP_BLOCK_PREV_USED;
}
 
static heap_block* __heap_block_allocate_from_begin(heap_cntrl *theheap, heap_block *block, heap_block *next_block, heap_block *free_anchor, uintptr_t alloc_size)
{
	__heap_block_split(theheap, block, next_block, free_anchor, alloc_size);
	return block;
} 
 
static heap_block* __heap_block_allocate_from_end(heap_cntrl *theheap, heap_block *block, heap_block *next_block, heap_block *free_anchor, uintptr_t alloc_begin, uintptr_t alloc_size)
{
	heap_block *const new_block = __heap_block_of_alloc_area(alloc_begin, theheap->page_size);
	uintptr_t const new_block_begin = (uintptr_t)new_block;
	uintptr_t const new_block_size = (uintptr_t)next_block - new_block_begin;
	uintptr_t block_size_adjusted = (uintptr_t) new_block - (uintptr_t) block;

	_HAssert(block_size_adjusted >= theheap->min_block_size);
	_HAssert(new_block_size >= theheap->min_block_size);

	if (__heap_prev_used(block)) {
		__heap_block_insert_after(free_anchor, block);
		free_anchor = block;
	} else {
		heap_block *const prev_block = __heap_block_prev(block);
		uintptr_t const prev_block_size = __heap_block_size(prev_block);

		block = prev_block;
		block_size_adjusted += prev_block_size;
	}

	block->size = block_size_adjusted|HEAP_BLOCK_PREV_USED;

	new_block->prev_size = block_size_adjusted;
	new_block->size = new_block_size;

	__heap_block_split(theheap, new_block, next_block, free_anchor, alloc_size);

	return new_block;
}

static heap_block* __heap_block_allocate(heap_cntrl *theheap, heap_block *block, uintptr_t alloc_begin, uintptr_t alloc_size)
{
	uintptr_t const alloc_area_begin = __heap_alloc_area_of_block(block);
	uintptr_t const alloc_area_offset = alloc_begin - alloc_area_begin;
	uintptr_t const block_size = __heap_block_size(block);
	heap_block *next_block = __heap_block_at(block, block_size);

	heap_block *free_anchor = NULL;

	_HAssert(alloc_area_begin <= alloc_begin);

	if (__heap_prev_used(next_block))
		free_anchor = __heap_head(theheap);
	else {
		free_anchor = block->prev;

		__heap_block_remove(block);
	}

	if (alloc_area_offset < theheap->page_size) {
		alloc_size += alloc_area_offset;
		block = __heap_block_allocate_from_begin(theheap, block, next_block, free_anchor, alloc_size);
	} else
		block = __heap_block_allocate_from_end(theheap, block, next_block, free_anchor, alloc_begin, alloc_size);
	
	return block;
}

static uintptr_t __heap_check_block(const heap_cntrl *theheap, const heap_block *block, uintptr_t alloc_size, uintptr_t alignment)
{
	uintptr_t const page_size = theheap->page_size;
	uintptr_t const min_block_size = theheap->min_block_size;

	uintptr_t const block_begin = (uintptr_t)block;
	uintptr_t const block_size = __heap_block_size(block);
	uintptr_t const block_end = block_begin + block_size;

	uintptr_t const alloc_begin_floor = __heap_alloc_area_of_block(block);
	uintptr_t const alloc_begin_ceiling = block_end - min_block_size + HEAP_BLOCK_HEADER_SIZE + page_size - 1;

	uintptr_t alloc_end = block_end + HEAP_BLOCK_ALLOC_BONUS;
	uintptr_t alloc_begin = alloc_end - alloc_size;

	alloc_begin = __heap_align_down(alloc_begin, alignment);
	if (alloc_begin > alloc_begin_ceiling)
		alloc_begin = __heap_align_down(alloc_begin_ceiling, alignment);

	alloc_end = alloc_begin + alloc_size;

	if (alloc_begin >= alloc_begin_floor) {
		uintptr_t const alloc_block_begin = (uintptr_t)__heap_block_of_alloc_area(alloc_begin, page_size);
		uintptr_t const free_size = alloc_block_begin - block_begin;

		if (free_size >= min_block_size || free_size == 0)
			return alloc_begin;
	}

	return 0;
}

uintptr_t heapInit(heap_cntrl *theheap, void *start_addr, uintptr_t size)
{
	uintptr_t const heap_begin = (uintptr_t)start_addr;
	uintptr_t const heap_end = heap_begin + size;
	uintptr_t first_block_begin = 0;
	uintptr_t first_block_size = 0;
	uintptr_t last_block_begin = 0;
	uintptr_t min_block_size = 0;
	uintptr_t page_size = CPU_ALIGNMENT;
	heap_block *first_block = NULL;
	heap_block *last_block = NULL;
	char area_ok = 0;

	min_block_size = __heap_min_block_size(page_size);
	area_ok = __heap_get_first_last_block(heap_begin, size, page_size, min_block_size, &first_block, &last_block);

	if (!area_ok)
		return 0;

	memset(theheap, 0, sizeof(heap_cntrl));

	first_block_begin = (uintptr_t)first_block;
	last_block_begin = (uintptr_t)last_block;
	first_block_size =  last_block_begin - first_block_begin;

	first_block->prev_size = heap_end;
	first_block->size = first_block_size|HEAP_BLOCK_PREV_USED;
	first_block->next = __heap_tail(theheap);
	first_block->prev = __heap_head(theheap);

	theheap->page_size = page_size;
	theheap->min_block_size = min_block_size;
	theheap->heap_begin = heap_begin;
	theheap->heap_end = heap_end;
	theheap->first_block = first_block;
	theheap->last_block = last_block;
	__heap_head(theheap)->next = first_block;
	__heap_tail(theheap)->prev = first_block;

	last_block->prev_size = first_block_size;
	last_block->size = 0;
	__heap_set_last_block_size(theheap);

	sys_spinlock_initialize(&theheap->lock);

	_HAssert(__heap_is_aligned(theheap->page_size, CPU_ALIGNMENT));
	_HAssert(__heap_is_aligned(theheap->min_block_size, page_size));
	_HAssert(__heap_is_aligned(__heap_alloc_area_of_block(first_block), page_size));
	_HAssert(__heap_is_aligned(__heap_alloc_area_of_block(last_block), page_size));

	return first_block_size;
}

void* heapAllocateAligned(heap_cntrl *theheap, uintptr_t size, uintptr_t alignment)
{
	uintptr_t const block_size_floor = size + HEAP_BLOCK_HEADER_SIZE - HEAP_BLOCK_ALLOC_BONUS;
	heap_block *const tail = __heap_tail(theheap);
	heap_block *block = NULL;
	uintptr_t alloc_begin = 0;

	if (block_size_floor < size)
		return NULL;

	sys_spinlock_lock(&theheap->lock);

	block =__heap_first(theheap);
	while (block != tail) {
		_HAssert(__heap_prev_used(block));

		if (block->size > block_size_floor) {
			if (alignment == 0)
				alloc_begin = __heap_alloc_area_of_block(block);
			else
				alloc_begin = __heap_check_block(theheap, block, size, alignment);
		}
		if (alloc_begin != 0)
			break;

		block = block->next;
	}

	if (alloc_begin != 0)
		block = __heap_block_allocate(theheap, block, alloc_begin, size);

	sys_spinlock_unlock(&theheap->lock);

	return (void*)alloc_begin;
}

void* heapAllocate(heap_cntrl *theheap, uintptr_t size)
{
	return heapAllocateAligned(theheap, size, 0);
}

char heapFree(heap_cntrl *theheap, void *ptr)
{
	uintptr_t alloc_begin;
	heap_block *block;
	heap_block *next_block = NULL;
	uintptr_t block_size = 0;
	uintptr_t next_block_size = 0;
	char next_free = 0;

	if (ptr == NULL)
		return 1;

	sys_spinlock_lock(&theheap->lock);

	alloc_begin = (uintptr_t)ptr;
	block = __heap_block_of_alloc_area(alloc_begin, theheap->page_size);
	if (!__heap_block_in(theheap, block)) {
		sys_spinlock_unlock(&theheap->lock);
		return 0;
	}

	block_size = __heap_block_size(block);
	next_block = __heap_block_at(block, block_size);
	if (!__heap_block_in(theheap, next_block)) {
		sys_spinlock_unlock(&theheap->lock);
		return 0;
	}

	if (!__heap_prev_used(next_block)) {
		sys_spinlock_unlock(&theheap->lock);
		return 0;
	}

	next_block_size = __heap_block_size(next_block);
	next_free = (next_block != theheap->last_block && !__heap_prev_used(__heap_block_at(next_block, next_block_size)));

	if (!__heap_prev_used(block)) {
		uintptr_t const prev_size = block->prev_size;
		heap_block *const prev_block = __heap_block_at(block, -prev_size);

		if (!__heap_block_in(theheap, prev_block)) {
			_HAssert(0);
			sys_spinlock_unlock(&theheap->lock);
			return 0;
		}

		if (!__heap_prev_used(prev_block)) {
			_HAssert(0);
			sys_spinlock_unlock(&theheap->lock);
			return 0;
		}

		if (next_free) {
			uintptr_t const size = block_size + prev_size + next_block_size;

			__heap_block_remove(next_block);

			prev_block->size = size|HEAP_BLOCK_PREV_USED;
			next_block = __heap_block_at(prev_block, size);
			_HAssert(!__heap_prev_used(next_block));
			next_block->prev_size = size;
		} else {
			uintptr_t const size = block_size + prev_size;
			prev_block->size = size|HEAP_BLOCK_PREV_USED;
			next_block->size &= ~HEAP_BLOCK_PREV_USED;
			next_block->prev_size = size;
		}
	} else if (next_free) {
		uintptr_t const size = block_size + next_block_size;

		__heap_block_replace(next_block, block);

		block->size = size|HEAP_BLOCK_PREV_USED;
		next_block = __heap_block_at(block, size);
		next_block->prev_size = size;
	} else {
		__heap_block_insert_after(__heap_head(theheap), block);

		block->size = block_size|HEAP_BLOCK_PREV_USED;
		next_block->size &= ~HEAP_BLOCK_PREV_USED;
		next_block->prev_size = block_size;
	}

	sys_spinlock_unlock(&theheap->lock);
	return 1;
}
