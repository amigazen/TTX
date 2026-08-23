/*
 * ttxreqs.library memory helpers
 */

#include "private/tr_internal.h"

#include <proto/exec.h>

APTR
TR_Alloc(ULONG size, ULONG flags)
{
	if (size == 0)
		return NULL;
	return AllocVec(size, flags | MEMF_CLEAR);
}

VOID
TR_Free(APTR ptr)
{
	if (ptr)
		FreeVec(ptr);
}
