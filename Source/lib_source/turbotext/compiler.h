/*
 * turbotext.library compiler shim (CLib39x / asyncio pattern)
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TURBOTEXT_COMPILER_H
#define TURBOTEXT_COMPILER_H

#include <exec/types.h>
#include <clib/compiler-specific.h>

#ifndef TT_INITTABLE_DEFINED
#define TT_INITTABLE_DEFINED 1
struct InitTable
{
	ULONG it_LibSize;
	APTR *it_FuncTable;
	APTR  it_DataTable;
	APTR  it_InitFunc;
};
#endif

#if defined(TT_SHARED_LIB) || defined(TT_REGARGS)
#define TT_REG(r, p) __REG__(r, p)
#else
#define TT_REG(r, p) p
#endif

#if defined(TT_SHARED_LIB)
#define TT_LVO __ASM__ __SAVE_DS__
#elif defined(TT_REGARGS)
#define TT_LVO __ASM__
#else
#define TT_LVO __STDARGS__
#endif

#endif /* TURBOTEXT_COMPILER_H */
