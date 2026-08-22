#ifndef LIBRARIES_TTXSUPPORT_H
#define LIBRARIES_TTXSUPPORT_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef EXEC_LIBRARIES_H
#include <exec/libraries.h>
#endif

#define TTXSUPPORTNAME "ttxsupport.library"

struct TTXSupportBase {
	struct Library lib;
};

#endif /* LIBRARIES_TTXSUPPORT_H */
