/*
 * TTX driver - ARexx host declarations
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TTX_AREXX_H
#define TTX_AREXX_H

#ifndef TTX_DRIVER_H
#include "ttx_driver.h"
#endif

BOOL TTX_ParseCommandLine(
	STRPTR line,
	STRPTR *outCommand,
	STRPTR *args,
	ULONG maxArgs,
	ULONG *outArgCount);

#endif /* TTX_AREXX_H */
