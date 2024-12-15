/*! \file mm.h
\brief RSX memory management.
*/

#ifndef __RSX_MM_H__
#define __RSX_MM_H__

// Copied from PSL1GHT since these are custom functions
// Copyright (c) 2011 PSL1GHT Development Team
// Distributed under the MIT license.

#include <stdint.h>
#include <cell/gcm.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief Initialize the RSX heap.
\return 0 if no error, nonzero otherwise.
*/
s32 rsxHeapInit();

/*! \brief Dynamic allocation of RSX memory.
\param size Size in bytes of buffer to be allocated.
\return Pointer to the allocated buffer, or \c NULL if an error occured.
*/
void* rsxMalloc(uint32_t size);

/*! \brief Dynamic allocation of aligned RSX memory.
\param alignment The required alignment value.
\param size Size in bytes of buffer to be allocated.
\return Pointer to the allocated buffer, or \c NULL if an error occured.
*/
void* rsxMemalign(uint32_t alignment, uint32_t size);

/*! \brief Deallocation of a previously dynamically allocated RSX buffer.

The buffer must have been allocated with \ref rsxMalloc or \ref rsxMemalign.
\param ptr Pointer to the allocated buffer.
*/
void rsxFree(void *ptr);

void rsxResetCommandBuffer(CellGcmContextData *context);
void rsxFlushBuffer(CellGcmContextData *context);
void rsxFinish(CellGcmContextData *context,uint32_t ref_value);

#ifdef __cplusplus
}
#endif

#endif
