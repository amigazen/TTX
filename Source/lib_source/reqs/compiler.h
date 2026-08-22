#ifndef TTXREQS_COMPILER_H
#define TTXREQS_COMPILER_H

#include <exec/types.h>
#include <clib/compiler-specific.h>

#ifndef TR_INITTABLE_DEFINED
#define TR_INITTABLE_DEFINED 1
struct InitTable
{
	ULONG it_LibSize;
	APTR *it_FuncTable;
	APTR  it_DataTable;
	APTR  it_InitFunc;
};
#endif

#if defined(TR_SHARED_LIB) || defined(TR_REGARGS)
#define TR_REG(r, p) __REG__(r, p)
#else
#define TR_REG(r, p) p
#endif

#if defined(TR_SHARED_LIB)
#define TR_LVO __ASM__ __SAVE_DS__
#elif defined(TR_REGARGS)
#define TR_LVO __ASM__
#else
#define TR_LVO __STDARGS__
#endif

#endif /* TTXREQS_COMPILER_H */
