/*
 * TTX driver - Intuition windowing and IDCMP event handling
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TTX_INTUI_H
#define TTX_INTUI_H

#include "ttx_driver.h"

/****************************************************************************/

BOOL TTX_IntuiOpenWindow(struct Session *session, struct Screen *screen);
VOID TTX_IntuiCloseWindow(struct TTXApplication *app, struct Session *session);
BOOL TTX_IntuiHandleMessage(struct TTXApplication *app,
	struct Session *portSession, struct IntuiMessage *imsg);
VOID TTX_IntuiRebuildSignalMask(struct TTXApplication *app);

/****************************************************************************/

#endif /* TTX_INTUI_H */
