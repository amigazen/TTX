/*
 * turbotext.library startup vectors and function table
 *
 * FuncTab[] order matches public LVO exports.  Standard vectors first,
 * then TurboTextRun at bias -948 equivalent position in the table.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/resident.h>
#include <dos/dos.h>

#include <proto/exec.h>

#include "compiler.h"
#include "private/tt_build.h"
#include "include/libraries/turbotext.h"
#include "tt_funcs.h"

extern const char TT_LibName[];
extern const char TT_LibId[];

struct TurboTextBase *TurboTextBase;
struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *UtilityBase;
APTR SegList;

struct TTUIHooks *TT_UIHooks = NULL;
APTR TT_AppCtx = NULL;

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
	(ULONG)sizeof(struct TurboTextBase),
	(APTR *)FuncTab,
	(APTR)NULL,
	(APTR)LibInit
};

/*
 * FuncTab[] order MUST match ../SDK/SFD/turbotext_lib.sfd.
 */
APTR FuncTab[] = {
	(APTR)LibOpen,
	(APTR)LibClose,
	(APTR)LibExpunge,
	(APTR)LibReserved,
	(APTR)TurboTextRun,
	(APTR)TT_OpenDocument,
	(APTR)TT_CloseDocument,
	(APTR)TT_DoCommand,
	(APTR)TT_GetActiveView,
	(APTR)TT_GetLastError,
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
	struct TurboTextBase *base = (struct TurboTextBase *)lib;

	SysBase = sysbase;

	if ((DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37)) &&
	    (UtilityBase = OpenLibrary("utility.library", 37)))
	{
		base->lib.lib_Node.ln_Type = NT_LIBRARY;
		base->lib.lib_Node.ln_Pri = 0;
		base->lib.lib_Node.ln_Name = (STRPTR)TT_LibName;
		base->lib.lib_Flags = LIBF_CHANGED | LIBF_SUMUSED;
		base->lib.lib_Version = TT_LIB_VERSION;
		base->lib.lib_Revision = TT_LIB_REVISION;
		base->lib.lib_IdString = (STRPTR)TT_LibId;
		base->documents = NULL;
		base->docCount = 0;
		base->nextDocID = 1;
		base->nextViewID = 1;
		base->lastError = TTERR_NONE;
		SegList = seglist;
		TurboTextBase = base;
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
	if (lib->lib_OpenCnt && --lib->lib_OpenCnt)
	{
		return NULL;
	}

	if (lib->lib_Flags & LIBF_DELEXP)
	{
		return LibExpunge(lib);
	}

	return NULL;
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
	TurboTextBase = NULL;
	FreeMem((UBYTE *)lib - lib->lib_NegSize, lib->lib_NegSize + lib->lib_PosSize);
}
