/*
 * TT_GetLastError - FuncTab LVO entry (SFD turbotext_lib.sfd)
 */

#include "private/tt_internal.h"
#include "tt_funcs.h"

ULONG
TT_LVO TT_GetLastError(
	TT_REG(a6, struct Library *lib))
{
	return TT_GetLastErrorI((struct TurboTextBase *)lib);
}
