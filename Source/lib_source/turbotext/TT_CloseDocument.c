/*
 * TT_CloseDocument - FuncTab LVO entry (SFD turbotext_lib.sfd)
 */

#include "private/tt_internal.h"
#include "tt_funcs.h"

BOOL
TT_LVO TT_CloseDocument(
	TT_REG(a6, struct Library *lib),
	TT_REG(a0, struct TTDocument *doc))
{
	return TT_CloseDocumentI((struct TurboTextBase *)lib, doc);
}
