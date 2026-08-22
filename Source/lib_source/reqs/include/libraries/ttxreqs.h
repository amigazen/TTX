#ifndef LIBRARIES_TTXREQS_H
#define LIBRARIES_TTXREQS_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef EXEC_LIBRARIES_H
#include <exec/libraries.h>
#endif

#define TTXREQSNAME "ttxreqs.library"

struct TTXReqsBase {
	struct Library lib;
};

#endif /* LIBRARIES_TTXREQS_H */
