#ifndef CLIB_TURBOTEXT_PROTOS_H
#define CLIB_TURBOTEXT_PROTOS_H

/*
** 'C' prototypes for turbotext.library public LVOs.
** Driver links via proto/pragmas and opens the shared library at runtime.
*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef LIBRARIES_TURBOTEXT_H
#include <libraries/turbotext.h>
#endif

LONG TurboTextRun(STRPTR cmdLine, struct TTUIHooks *hooks, APTR appCtx);
struct TTDocument *TT_OpenDocument(STRPTR fileName);
BOOL TT_CloseDocument(struct TTDocument *doc);
BOOL TT_DoCommand(
	struct TTDocument *doc,
	struct TTView *view,
	STRPTR command,
	STRPTR *args,
	ULONG argCount);
struct TTView *TT_GetActiveView(struct TTDocument *doc);
ULONG TT_GetLastError(VOID);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CLIB_TURBOTEXT_PROTOS_H */
