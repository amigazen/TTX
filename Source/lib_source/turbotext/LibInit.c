/*
 * turbotext.library ROMTag and version strings
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/resident.h>

#include "compiler.h"
#include "private/tt_build.h"
#include "Rev.h"

const char TT_LibName[] = "turbotext.library";
const char TT_LibId[] = "turbotext.library " VERSION " (" DATE ")\r\n";

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
	(APTR)TT_LibName,
	(APTR)TT_LibId,
	(APTR)&InitTab
};

#ifdef __SASC
void __regargs __chkabort(void) { }
void __regargs _CXBRK(void)     { }
#endif
