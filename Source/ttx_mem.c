/*
 * TTX driver memory helpers (direct exec.library, no seiso)
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_mem.h"

#include <proto/exec.h>

APTR
TTX_Alloc(ULONG size, ULONG flags)
{
	if (size == 0)
		return NULL;

	return AllocVec(size, flags | MEMF_CLEAR);
}

VOID
TTX_Free(APTR ptr)
{
	if (ptr)
		FreeVec(ptr);
}

STRPTR
TTX_AllocPathBuf(VOID)
{
	return (STRPTR)TTX_Alloc(TTX_PATH_BUF_LEN, MEMF_CLEAR);
}
