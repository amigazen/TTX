/*
 * TT_DoCommand - FuncTab LVO entry (SFD turbotext_lib.sfd)
 */

#include "private/tt_internal.h"
#include "tt_funcs.h"

BOOL
TT_LVO TT_DoCommand(
	TT_REG(a6, struct Library *lib),
	TT_REG(a0, struct TTDocument *doc),
	TT_REG(a1, struct TTView *view),
	TT_REG(a2, STRPTR command),
	TT_REG(a3, STRPTR *args),
	TT_REG(d0, ULONG argCount))
{
	return TT_DoCommandI(
		(struct TurboTextBase *)lib,
		doc, view, command, args, argCount);
}
