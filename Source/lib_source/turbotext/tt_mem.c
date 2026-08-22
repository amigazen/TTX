/*
 * turbotext.library memory helpers
 *
 * Explicit AllocVec/FreeVec wrappers for engine allocations.
 * The driver uses Seiso; the engine does not depend on it.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "private/tt_internal.h"

APTR
TT_Alloc(ULONG size, ULONG flags)
{
	APTR ptr = NULL;

	if (size > 0)
		ptr = AllocVec(size, flags | MEMF_CLEAR);

	return ptr;
}

VOID
TT_Free(APTR ptr)
{
	if (ptr)
		FreeVec(ptr);
}

STRPTR
TT_DupStr(STRPTR src)
{
	STRPTR dst = NULL;
	ULONG len = 0;

	if (src)
	{
		len = 0;
		while (src[len] != '\0')
			len++;

		dst = (STRPTR)TT_Alloc(len + 1, MEMF_ANY);
		if (dst)
		{
			CopyMem(src, dst, len);
			dst[len] = '\0';
		}
	}

	return dst;
}

struct TurboTextBase *
TT_GetBase(VOID)
{
	return TurboTextBase;
}

VOID
TT_SetLastError(ULONG code)
{
	if (TurboTextBase)
		TurboTextBase->lastError = code;
}

ULONG
TT_GetLastErrorI(struct TurboTextBase *base)
{
	if (!base)
		return TTERR_NO_DOCUMENT;

	return base->lastError;
}
