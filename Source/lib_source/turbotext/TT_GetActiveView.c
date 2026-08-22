/*
 * TT_GetActiveView - FuncTab LVO entry (SFD turbotext_lib.sfd)
 */

#include "private/tt_internal.h"
#include "tt_funcs.h"

struct TTView *
TT_LVO TT_GetActiveView(
	TT_REG(a6, struct Library *lib),
	TT_REG(a0, struct TTDocument *doc))
{
	(void)lib;

	return TT_GetActiveViewI(doc);
}
