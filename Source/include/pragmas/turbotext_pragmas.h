#ifndef PRAGMAS_TURBOTEXT_PRAGMAS_H
#define PRAGMAS_TURBOTEXT_PRAGMAS_H

/*
** SAS/C and Lattice pragma libcalls for turbotext.library.
** Offsets/registers from lib_source/SDK/SFD/turbotext_lib.sfd (regenerate with LibDescConverter).
*/

#ifndef CLIB_TURBOTEXT_PROTOS_H
#include <clib/turbotext_protos.h>
#endif /* CLIB_TURBOTEXT_PROTOS_H */

#pragma libcall TurboTextBase TurboTextRun 1E A9803
#pragma libcall TurboTextBase TT_OpenDocument 24 801
#pragma libcall TurboTextBase TT_CloseDocument 2A 801
#pragma libcall TurboTextBase TT_DoCommand 30 10A9805
#pragma libcall TurboTextBase TT_GetActiveView 36 801
#pragma libcall TurboTextBase TT_GetLastError 3C 000

#endif /* PRAGMAS_TURBOTEXT_PRAGMAS_H */
