/*
 * ttxsupport.library ROMTag and version strings
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/resident.h>

#include "compiler.h"
#include "private/ts_build.h"
#include "Rev.h"

const char TS_LibName[] = "ttxsupport.library";
const char TS_LibId[] = "ttxsupport.library " VERSION " (" DATE ")\r\n";

extern struct InitTable InitTab;
extern APTR __ASM__ __SAVE_DS__ LibExpunge(__REG__(a6, struct Library *base));

struct Resident RomTag = {
	RTC_MATCHWORD,
	&RomTag,
	LibExpunge,
	RTF_AUTOINIT,
	VERNUM,
	NT_LIBRARY,
	0,
	(APTR)TS_LibName,
	(APTR)TS_LibId,
	(APTR)&InitTab
};

#ifdef __SASC
void __regargs __chkabort(void) { }
void __regargs _CXBRK(void)     { }
#endif
