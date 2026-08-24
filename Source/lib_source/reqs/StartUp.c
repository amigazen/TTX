/*
 * ttxreqs.library - BOOPSI requesters (LVO shared library)
 *
 * Public LVOs: SDK/SFD/ttxreqs_lib.sfd (prefs + find/request UI).
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/resident.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <graphics/gfxbase.h>
#include <intuition/intuitionbase.h>

#include "compiler.h"
#include "private/tr_build.h"
#include "include/libraries/ttxreqs.h"
#include "tr_funcs.h"

extern const char TR_LibName[];
extern const char TR_LibId[];

struct TTXReqsBase *TTXReqsBase;
struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *UtilityBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
struct Library *AslBase;
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
	(ULONG)sizeof(struct TTXReqsBase),
	(APTR *)FuncTab,
	(APTR)NULL,
	(APTR)LibInit
};

/*
 * FuncTab[] MUST match SDK/SFD/ttxreqs_lib.sfd (sfd_reconcile.py).
 * Public LVOs start at bias -30.
 */
APTR FuncTab[] = {
	(APTR)LibOpen,
	(APTR)LibClose,
	(APTR)LibExpunge,
	(APTR)LibReserved,
	(APTR)TR_RequestBool,
	(APTR)TR_RequestChoice,
	(APTR)TR_RequestStr,
	(APTR)TR_RequestNum,
	(APTR)TR_RequestFile,
	(APTR)TR_RequestFind,
	(APTR)TR_RequestFindChange,
	(APTR)TR_PrefsSetDefaults,
	(APTR)TR_PrefsGet,
	(APTR)TR_PrefsSet,
	(APTR)TR_PrefsLoad,
	(APTR)TR_PrefsSave,
	(APTR)TR_PrefsRequester,
	(APTR)TR_InfoOpen,
	(APTR)TR_InfoClose,
	(APTR)TR_InfoUpdate,
	(APTR)TR_InfoProcessMsg,
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
	struct TTXReqsBase *base = (struct TTXReqsBase *)lib;

	SysBase = sysbase;
	GfxBase = NULL;
	GadToolsBase = NULL;
	AslBase = NULL;

	if ((DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37)) &&
	    (UtilityBase = OpenLibrary("utility.library", 37)) &&
	    (IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37)) &&
	    (GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37)) &&
	    (GadToolsBase = OpenLibrary("gadtools.library", 37)))
	{
		/* ASL optional — TR_RequestFile fails soft if absent. */
		AslBase = OpenLibrary("asl.library", 37);

		base->lib.lib_Node.ln_Type = NT_LIBRARY;
		base->lib.lib_Node.ln_Pri = 0;
		base->lib.lib_Node.ln_Name = (STRPTR)TR_LibName;
		base->lib.lib_Flags = LIBF_CHANGED | LIBF_SUMUSED;
		base->lib.lib_Version = TR_LIB_VERSION;
		base->lib.lib_Revision = TR_LIB_REVISION;
		base->lib.lib_IdString = (STRPTR)TR_LibId;
		SegList = seglist;
		TTXReqsBase = base;
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
	if (AslBase)
	{
		CloseLibrary(AslBase);
		AslBase = NULL;
	}
	if (GadToolsBase)
	{
		CloseLibrary(GadToolsBase);
		GadToolsBase = NULL;
	}
	if (GfxBase)
	{
		CloseLibrary((struct Library *)GfxBase);
		GfxBase = NULL;
	}
	if (IntuitionBase)
	{
		CloseLibrary((struct Library *)IntuitionBase);
		IntuitionBase = NULL;
	}
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
	TTXReqsBase = NULL;
	FreeMem((UBYTE *)lib - lib->lib_NegSize, lib->lib_NegSize + lib->lib_PosSize);
}
