// Copied from PSL1GHT since these are custom functions
// Copyright (c) 2011 PSL1GHT Development Team
// Distributed under the MIT license.

#include <stdlib.h>

#include <cell/gcm.h>
#include <sys/memory.h>
#include <sys/timer.h>
#include <mspace.h>

#define SAFE_AREA			4096

static CellGcmConfig __rsx_config;

static sys_addr_t __heap_area;
static mspace __rsx_heap;
static uint32_t __rsxheap_initialized = 0;

int64_t rsxHeapInit()
{
	if(!__rsxheap_initialized) {
		void *heapBuffer;
		uint32_t heapBufferSize;

		cellGcmGetConfiguration(&__rsx_config);
		
		heapBuffer = __rsx_config.localAddress;
		heapBufferSize = __rsx_config.localSize - SAFE_AREA;
		
		sys_memory_allocate(heapBufferSize, SYS_MEMORY_PAGE_SIZE_1M, (sys_addr_t *)(&__heap_area));
		__rsx_heap = mspace_create((void*)__heap_area, heapBufferSize);
		
		__rsxheap_initialized = 1;
	}

	return 1;
}

void* rsxMemalign(uint32_t alignment,uint32_t size)
{
	if(!__rsxheap_initialized) return NULL;
	return mspace_memalign(&__rsx_heap,alignment,size);
}

void* rsxMalloc(uint32_t size)
{
	if(!__rsxheap_initialized) return NULL;
	return mspace_malloc(&__rsx_heap,size);
}

void rsxFree(void *ptr)
{
	if(!__rsxheap_initialized) return;
	mspace_free(&__rsx_heap,ptr);
}

void rsxResetCommandBuffer(CellGcmContextData *context)
{
	uint32_t offset = 0x1000;			// init state offset;
	cellGcmSetJumpCommand(context,offset);

	// __sync();

	CellGcmControl volatile *ctrl = cellGcmGetControlRegister();
	ctrl->put = offset;
	while(ctrl->get!=offset) sys_timer_usleep(30);
}

void rsxFlushBuffer(CellGcmContextData *context)
{
	uint32_t offset = 0;
	CellGcmControl volatile *ctrl = cellGcmGetControlRegister();
	
	// __sync();
	cellGcmAddressToOffset(context->current,&offset);
	ctrl->put = offset;
}

void rsxFinish(CellGcmContextData *context,uint32_t ref_value)
{
	cellGcmSetReferenceCommand(context,ref_value);
	cellGcmFlush(context);

	CellGcmControl volatile *ctrl = cellGcmGetControlRegister();
	while(ctrl->ref!=ref_value) sys_timer_usleep(30);
}
