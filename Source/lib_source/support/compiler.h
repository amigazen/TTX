#ifndef TTXSUPPORT_COMPILER_H
#define TTXSUPPORT_COMPILER_H

#include <exec/types.h>
#include <clib/compiler-specific.h>

#ifndef TS_INITTABLE_DEFINED
#define TS_INITTABLE_DEFINED 1
struct InitTable
{
	ULONG it_LibSize;
	APTR *it_FuncTable;
	APTR  it_DataTable;
	APTR  it_InitFunc;
};
#endif

#if defined(TS_SHARED_LIB) || defined(TS_REGARGS)
#define TS_REG(r, p) __REG__(r, p)
#else
#define TS_REG(r, p) p
#endif

#if defined(TS_SHARED_LIB)
#define TS_LVO __ASM__ __SAVE_DS__
#elif defined(TS_REGARGS)
#define TS_LVO __ASM__
#else
#define TS_LVO __STDARGS__
#endif

#endif /* TTXSUPPORT_COMPILER_H */
