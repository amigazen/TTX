/*
 * TTX driver - shared keyboard input (window IDCMP and BOOPSI gadget)
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TTX_INPUT_H
#define TTX_INPUT_H

#include "ttx_driver.h"

#include <devices/inputevent.h>
#include <intuition/intuition.h>

/****************************************************************************/

VOID TTX_InputRefreshSession(struct Session *session);

BOOL TTX_InputVanillaKey(
	struct TTXApplication *app,
	struct Session *session,
	UWORD code,
	ULONG qualifier,
	APTR iaddr);

BOOL TTX_InputRawKey(
	struct TTXApplication *app,
	struct Session *session,
	UBYTE rawCode,
	ULONG qualifier,
	APTR iaddr);

BOOL TTX_InputFromIntuiMessage(
	struct TTXApplication *app,
	struct Session *session,
	struct IntuiMessage *imsg);

BOOL TTX_InputFromInputEvent(
	struct TTXApplication *app,
	struct Session *session,
	struct InputEvent *ievent,
	APTR iaddr);

/****************************************************************************/

#endif /* TTX_INPUT_H */
