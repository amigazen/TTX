/*
 * TTX driver - BOOPSI gadget construction (TurboText scroll gadget model)
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TTX_BOOPSI_H
#define TTX_BOOPSI_H

#include "ttx_driver.h"

/****************************************************************************/
/* Gadget IDs — delivered in IDCMP_GADGETUP Code */

#define GID_VERT_UP           3
#define GID_VERT_DOWN         4
#define GID_HORIZ_LEFT        5
#define GID_HORIZ_RIGHT       6

#define TTX_ARROW_SIZE        11

/****************************************************************************/

BOOL TTX_BoopsiCreateScrollGadgets(
	struct Session *session,
	struct Window *window,
	struct DrawInfo *drawInfo);

VOID TTX_BoopsiDestroyScrollGadgets(struct Session *session);

VOID TTX_BoopsiUpdateScrollGadgets(struct Session *session);

BOOL TTX_BoopsiHandleScrollGadgetUp(
	struct Session *session,
	ULONG gadgetID);

BOOL TTX_BoopsiHandleIdcmpUpdate(
	struct Session *session,
	ULONG gadgetID);

#endif /* TTX_BOOPSI_H */
