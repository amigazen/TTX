#ifndef PROTO_TURBOTEXT_H
#define PROTO_TURBOTEXT_H

/*
** Proto header for turbotext.library (asyncio.library pattern).
** TTX opens turbotext.library at runtime; slink does not link the .library file.
*/

#ifdef _NO_INLINE

#include <clib/turbotext_protos.h>

#else

#ifndef __NOLIBBASE__

#ifndef EXEC_LIBRARIES_H
#include <exec/libraries.h>
#endif /* EXEC_LIBRARIES_H */

extern struct Library *TurboTextBase;

#endif /* __NOLIBBASE__ */

#if defined(LATTICE) || defined(__SASC) || defined(_DCC)

#ifndef PRAGMAS_TURBOTEXT_PRAGMAS_H
#include <pragmas/turbotext_pragmas.h>
#endif /* PRAGMAS_TURBOTEXT_PRAGMAS_H */

#elif defined(AZTEC_C) || defined(__MAXON__) || defined(__STORM__)

#error turbotext proto pragmas not defined for this compiler

#elif defined(__VBCC__)

#include <clib/turbotext_protos.h>

#else

#include <clib/turbotext_protos.h>

#endif

#endif /* _NO_INLINE */

#endif /* PROTO_TURBOTEXT_H */
