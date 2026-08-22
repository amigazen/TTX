/*
 * ttxreqs.library ROMTag and version strings
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/resident.h>

#include "compiler.h"
#include "private/tr_build.h"
#include "Rev.h"

const char TR_LibName[] = "ttxreqs.library";
const char TR_LibId[] = "ttxreqs.library " VERSION " (" DATE ")\r\n";

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
	(APTR)TR_LibName,
	(APTR)TR_LibId,
	(APTR)&InitTab
};

#ifdef __SASC
void __regargs __chkabort(void) { }
void __regargs _CXBRK(void)     { }
#endif
