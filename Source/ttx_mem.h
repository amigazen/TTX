/*
 * TTX driver memory helpers (direct exec.library, no seiso)
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TTX_MEM_H
#define TTX_MEM_H

#include <exec/types.h>

/* Heap buffer for DOS path strings (NameFromLock, AddPart, ASL). */
#define TTX_PATH_BUF_LEN 512

APTR TTX_Alloc(ULONG size, ULONG flags);
VOID TTX_Free(APTR ptr);
STRPTR TTX_AllocPathBuf(VOID);

#endif /* TTX_MEM_H */
