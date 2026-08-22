/*
 * TurboTextRun - FuncTab LVO entry (SFD turbotext_lib.sfd)
 */

#include "private/tt_internal.h"
#include "tt_funcs.h"

LONG
TT_LVO TurboTextRun(
	TT_REG(a6, struct Library *lib),
	TT_REG(a0, STRPTR cmdLine),
	TT_REG(a1, struct TTUIHooks *hooks),
	TT_REG(a2, APTR appCtx))
{
	(void)lib;

	TT_UIHooks = hooks;
	TT_AppCtx = appCtx;

	return TT_ParseAndRun(cmdLine);
}
