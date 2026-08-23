/*
 * ttxsupport.library - buffered I/O and memory pools (LVO shared library)
 *
 * Public LVOs will be added via SDK/SFD/ttxsupport_lib.sfd as support
 * routines are ported from the original binary.
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/resident.h>
#include <dos/dos.h>

#include <proto/exec.h>

#include "compiler.h"
#include "private/ts_build.h"
#include "include/libraries/ttxsupport.h"

extern const char TS_LibName[];
extern const char TS_LibId[];

struct TTXSupportBase *TTXSupportBase;
struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *UtilityBase;
APTR SegList;

static VOID FreeLib(struct Library *lib);

LONG __ASM__ LibReserved(void);
struct Library *__ASM__ __SAVE_DS__ LibInit(
	__REG__(a6, struct ExecBase *sysbase),
	__REG__(a0, APTR seglist),
	__REG__(d0, struct Library *base));
struct Library *__ASM__ __SAVE_DS__ LibOpen(
	__REG__(a6, struct Library *base));
APTR __ASM__ __SAVE_DS__ LibClose(
	__REG__(a6, struct Library *base));
APTR __ASM__ __SAVE_DS__ LibExpunge(
	__REG__(a6, struct Library *base));

APTR FuncTab[];

struct InitTable InitTab = {
	(ULONG)sizeof(struct TTXSupportBase),
	(APTR *)FuncTab,
	(APTR)NULL,
	(APTR)LibInit
};

/* Standard vectors only; public LVOs follow ttxsupport_lib.sfd when added.
 * Keep FuncTab ↔ SFD ↔ include/pragmas/ttxsupport_pragmas.h via
 * SDK/tools/sfd_reconcile.py.
 */
APTR FuncTab[] = {
	(APTR)LibOpen,
	(APTR)LibClose,
	(APTR)LibExpunge,
	(APTR)LibReserved,
	(APTR)((LONG)-1)
};

LONG
__ASM__ LibReserved(void)
{
	return 0;
}

struct Library *
__ASM__ __SAVE_DS__ LibInit(
	__REG__(a6, struct ExecBase *sysbase),
	__REG__(a0, APTR seglist),
	__REG__(d0, struct Library *lib))
{
	struct TTXSupportBase *base = (struct TTXSupportBase *)lib;

	SysBase = sysbase;

	if ((DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37)) &&
	    (UtilityBase = OpenLibrary("utility.library", 37)))
	{
		base->lib.lib_Node.ln_Type = NT_LIBRARY;
		base->lib.lib_Node.ln_Pri = 0;
		base->lib.lib_Node.ln_Name = (STRPTR)TS_LibName;
		base->lib.lib_Flags = LIBF_CHANGED | LIBF_SUMUSED;
		base->lib.lib_Version = TS_LIB_VERSION;
		base->lib.lib_Revision = TS_LIB_REVISION;
		base->lib.lib_IdString = (STRPTR)TS_LibId;
		SegList = seglist;
		TTXSupportBase = base;
	}
	else
	{
		FreeLib(lib);
		lib = NULL;
	}

	return lib;
}

struct Library *
__ASM__ __SAVE_DS__ LibOpen(__REG__(a6, struct Library *lib))
{
	++lib->lib_OpenCnt;
	lib->lib_Flags &= ~LIBF_DELEXP;
	return lib;
}

APTR
__ASM__ __SAVE_DS__ LibClose(__REG__(a6, struct Library *lib))
{
	if (lib->lib_OpenCnt)
		--lib->lib_OpenCnt;

	if (lib->lib_OpenCnt)
		return NULL;

	lib->lib_Flags |= LIBF_DELEXP;
	return LibExpunge(lib);
}

APTR
__ASM__ __SAVE_DS__ LibExpunge(__REG__(a6, struct Library *lib))
{
	if (lib->lib_OpenCnt)
	{
		lib->lib_Flags |= LIBF_DELEXP;
		return NULL;
	}

	Remove(&lib->lib_Node);
	FreeLib(lib);

	return SegList;
}

static VOID
FreeLib(struct Library *lib)
{
	if (DOSBase)
	{
		CloseLibrary((struct Library *)DOSBase);
		DOSBase = NULL;
	}
	if (UtilityBase)
	{
		CloseLibrary(UtilityBase);
		UtilityBase = NULL;
	}
	TTXSupportBase = NULL;
	FreeMem((UBYTE *)lib - lib->lib_NegSize, lib->lib_NegSize + lib->lib_PosSize);
}
