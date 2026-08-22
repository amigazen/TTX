/*
 * turbotext.library LVO declarations for FuncTab[] / SFD turbotext_lib.sfd
 *
 * Register layout matches SDK/SFD/turbotext_lib.sfd and StartUp.c FuncTab[].
 */

#ifndef TURBOTEXT_TT_FUNCS_H
#define TURBOTEXT_TT_FUNCS_H

#include "compiler.h"

#ifndef LIBRARIES_TURBOTEXT_H
#include "libraries/turbotext.h"
#endif

LONG TT_LVO TurboTextRun(
	TT_REG(a6, struct Library *TurboTextBase),
	TT_REG(a0, STRPTR cmdLine),
	TT_REG(a1, struct TTUIHooks *hooks),
	TT_REG(a2, APTR appCtx));

struct TTDocument * TT_LVO TT_OpenDocument(
	TT_REG(a6, struct Library *TurboTextBase),
	TT_REG(a0, STRPTR fileName));

BOOL TT_LVO TT_CloseDocument(
	TT_REG(a6, struct Library *TurboTextBase),
	TT_REG(a0, struct TTDocument *doc));

BOOL TT_LVO TT_DoCommand(
	TT_REG(a6, struct Library *TurboTextBase),
	TT_REG(a0, struct TTDocument *doc),
	TT_REG(a1, struct TTView *view),
	TT_REG(a2, STRPTR command),
	TT_REG(a3, STRPTR *args),
	TT_REG(d0, ULONG argCount));

struct TTView * TT_LVO TT_GetActiveView(
	TT_REG(a6, struct Library *TurboTextBase),
	TT_REG(a0, struct TTDocument *doc));

ULONG TT_LVO TT_GetLastError(
	TT_REG(a6, struct Library *TurboTextBase));

#endif /* TURBOTEXT_TT_FUNCS_H */
