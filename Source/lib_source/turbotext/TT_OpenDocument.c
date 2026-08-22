/*
 * TT_OpenDocument - FuncTab LVO entry (SFD turbotext_lib.sfd)
 */

#include "private/tt_internal.h"
#include "tt_funcs.h"

struct TTDocument *
TT_LVO TT_OpenDocument(
	TT_REG(a6, struct Library *lib),
	TT_REG(a0, STRPTR fileName))
{
	return TT_OpenDocumentI((struct TurboTextBase *)lib, fileName);
}
