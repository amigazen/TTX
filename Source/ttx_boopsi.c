/*
 * TTX driver - BOOPSI scroll gadgets (propgclass + ICA live scroll)
 *
 * ICA_TARGET -> IDCMP_IDCMPUPDATE is safe now that super-bitmap/layer abuse
 * is removed; suppressIcmp guards programmatic PGA_Top updates.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_boopsi.h"

#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <proto/intuition.h>

/****************************************************************************/

/* Map GA_ID into IDCMP_IDCMPUPDATE Code field (TurboText / test.c pattern). */
static LONG TTX_ScrollIcMap[4] = {
	GA_ID, ICSPECIAL_CODE,
	TAG_DONE, 0
};

static VOID
TTX_ScaleScrollValues(
	ULONG total,
	ULONG visible,
	ULONG top,
	ULONG *outTotal,
	ULONG *outVisible,
	ULONG *outTop,
	SHORT *outShift)
{
	ULONG maxValue;
	ULONG scaledTotal;
	ULONG scaledVisible;
	ULONG scaledTop;
	SHORT shift;

	if (!outTotal || !outVisible || !outTop || !outShift)
		return;

	maxValue = 0xFFFF;
	scaledTotal = total;
	scaledVisible = visible;
	scaledTop = top;
	shift = 0;

	while (scaledTotal > maxValue && shift < 16) {
		scaledTotal >>= 1;
		if (scaledVisible > 1)
			scaledVisible >>= 1;
		scaledTop >>= 1;
		shift++;
	}

	if (scaledVisible < 1)
		scaledVisible = 1;
	if (scaledTotal < scaledVisible)
		scaledTotal = scaledVisible;
	if (scaledTop + scaledVisible > scaledTotal)
		scaledTop = scaledTotal - scaledVisible;

	*outTotal = scaledTotal;
	*outVisible = scaledVisible;
	*outTop = scaledTop;
	*outShift = shift;
}

/****************************************************************************/

BOOL
TTX_BoopsiCreateScrollGadgets(
	struct Session *session,
	struct Window *window,
	struct DrawInfo *drawInfo)
{
	struct Gadget *gadgetList;
	ULONG initialTotal;
	ULONG initialVisible;
	ULONG initialTop;
	ULONG maxLineLen;
	ULONG i;

	(void)drawInfo;

	if (!session || !window)
		return FALSE;

	if (session->scroll.gadgetsOnWindow)
		return TRUE;

	session->scroll.gadgetHead = NULL;
	session->scroll.gadgetsOnWindow = FALSE;
	session->scroll.suppressIcmp = FALSE;
	gadgetList = NULL;

	initialTotal = 100;
	initialVisible = 50;
	initialTop = 0;

	if (TT_SessionBuffer(session)) {
		initialVisible = TT_SessionBuffer(session)->pageH;
		initialTotal = TT_SessionBuffer(session)->lineCount;
		if (initialTotal < initialVisible)
			initialTotal = initialVisible;
		initialTop = TT_SessionBuffer(session)->scrollY;
	}
	if (initialVisible < 1)
		initialVisible = 1;
	if (initialTotal < 1)
		initialTotal = 1;

	/*
	 * In-window relative props (not border gadgets). OpenWindow autodoc:
	 * WA_InnerWidth/Height must not use GACT_RIGHTBORDER gadgets that steal
	 * drag/depth gadget space.
	 */
	session->scroll.vertProp = (struct Gadget *)NewObject(
		NULL, PROPGCLASS,
		GA_ID, GID_VERT_PROP,
		GA_RelRight, -window->BorderRight + 5,
		GA_Width, window->BorderRight - 8,
		GA_Top, window->BorderTop + 2,
		GA_RelHeight, -(window->BorderTop + window->BorderBottom + 4),
		PGA_Freedom, FREEVERT,
		PGA_NewLook, TRUE,
		PGA_Borderless, TRUE,
		PGA_VertBody, MAXBODY,
		PGA_Total, initialTotal,
		PGA_Visible, initialVisible,
		PGA_Top, initialTop,
		ICA_TARGET, ICTARGET_IDCMP,
		ICA_MAP, (ULONG)TTX_ScrollIcMap,
		GA_Next, (ULONG)gadgetList,
		TAG_DONE);
	if (session->scroll.vertProp) {
		session->scroll.vertProp->Flags |= GFLG_RELRIGHT | GFLG_RELHEIGHT;
		gadgetList = session->scroll.vertProp;
	}

	initialTotal = 100;
	initialVisible = 50;
	initialTop = 0;
	maxLineLen = 0;

	if (TT_SessionBuffer(session)) {
		if (TT_SessionBuffer(session)->lines &&
		    TT_SessionBuffer(session)->lineCount > 0) {
			for (i = 0; i < TT_SessionBuffer(session)->lineCount; i++) {
				if (TT_SessionBuffer(session)->lines[i].length > maxLineLen)
					maxLineLen = TT_SessionBuffer(session)->lines[i].length;
			}
		}
		initialVisible = TT_SessionBuffer(session)->pageW;
		initialTotal = maxLineLen;
		if (initialTotal < initialVisible)
			initialTotal = initialVisible;
		initialTop = TT_SessionBuffer(session)->scrollX;
	}
	if (initialVisible < 1)
		initialVisible = 1;
	if (initialTotal < 1)
		initialTotal = 1;

	session->scroll.horizProp = (struct Gadget *)NewObject(
		NULL, PROPGCLASS,
		GA_ID, GID_HORIZ_PROP,
		GA_Left, window->BorderLeft,
		GA_RelBottom, -window->BorderBottom + 3,
		GA_RelWidth, -(window->BorderLeft + window->BorderRight + 2),
		GA_Height, window->BorderBottom - 4,
		PGA_Freedom, FREEHORIZ,
		PGA_NewLook, TRUE,
		PGA_Borderless, TRUE,
		PGA_HorizBody, MAXBODY,
		PGA_Total, initialTotal,
		PGA_Visible, initialVisible,
		PGA_Top, initialTop,
		ICA_TARGET, ICTARGET_IDCMP,
		ICA_MAP, (ULONG)TTX_ScrollIcMap,
		GA_Next, (ULONG)gadgetList,
		TAG_DONE);
	if (session->scroll.horizProp) {
		struct Gadget *tail;

		session->scroll.horizProp->Flags |= GFLG_RELBOTTOM | GFLG_RELWIDTH;
		gadgetList = session->scroll.horizProp;

		if (session->textEditorGadget) {
			tail = gadgetList;
			while (tail->NextGadget)
				tail = tail->NextGadget;
			tail->NextGadget = session->textEditorGadget;
			session->textEditorGadget->NextGadget = NULL;
		}
	}

	if (gadgetList) {
		session->scroll.gadgetHead = gadgetList;
		AddGList(window, gadgetList, (UWORD)-1, (WORD)-1, NULL);
		RefreshGList(gadgetList, window, NULL, (WORD)-1);
		session->scroll.gadgetsOnWindow = TRUE;
		return TRUE;
	}

	/* Scroll props failed — still attach the text editor if we have one. */
	if (session->textEditorGadget) {
		AddGList(window, session->textEditorGadget, (UWORD)-1, (WORD)-1, NULL);
		RefreshGList(session->textEditorGadget, window, NULL, 1);
		return TRUE;
	}

	TTX_BoopsiDestroyScrollGadgets(session);
	return FALSE;
}

/****************************************************************************/

VOID
TTX_BoopsiDestroyScrollGadgets(struct Session *session)
{
	struct Window *window;

	if (!session)
		return;

	window = session->window;
	if (!window || window == INVALID_RESOURCE)
		return;

	if (session->scroll.gadgetsOnWindow && session->scroll.gadgetHead) {
		RemoveGList(window, session->scroll.gadgetHead, (LONG)-1);
		session->scroll.gadgetsOnWindow = FALSE;
		session->scroll.gadgetHead = NULL;
	}

	if (session->scroll.vertProp) {
		DisposeObject(session->scroll.vertProp);
		session->scroll.vertProp = NULL;
	}
	if (session->scroll.horizProp) {
		DisposeObject(session->scroll.horizProp);
		session->scroll.horizProp = NULL;
	}
}

/****************************************************************************/

VOID
TTX_BoopsiUpdateScrollGadgets(struct Session *session)
{
	struct Gadget *gadget;
	ULONG total;
	ULONG visible;
	ULONG top;
	ULONG scaledTotal;
	ULONG scaledVisible;
	ULONG scaledTop;
	SHORT shift;
	ULONG maxLineLen;
	ULONG idx;

	if (!session || !TT_SessionBuffer(session))
		return;

	gadget = session->scroll.vertProp;
	if (gadget) {
		total = TT_SessionBuffer(session)->lineCount;
		visible = TT_SessionBuffer(session)->pageH;
		top = TT_SessionBuffer(session)->scrollY;

		TTX_ScaleScrollValues(total, visible, top,
			&scaledTotal, &scaledVisible, &scaledTop, &shift);
		TT_SessionBuffer(session)->scrollYShift = shift;

		session->scroll.suppressIcmp = TRUE;
		SetGadgetAttrs(gadget, session->window, NULL,
			PGA_Total, scaledTotal,
			PGA_Visible, scaledVisible,
			PGA_Top, scaledTop,
			TAG_DONE);
		session->scroll.suppressIcmp = FALSE;
	}

	gadget = session->scroll.horizProp;
	if (gadget) {
		maxLineLen = 0;
		if (TT_SessionBuffer(session)->lines &&
		    TT_SessionBuffer(session)->lineCount > 0) {
			for (idx = 0; idx < TT_SessionBuffer(session)->lineCount; idx++) {
				if (TT_SessionBuffer(session)->lines[idx].length > maxLineLen)
					maxLineLen = TT_SessionBuffer(session)->lines[idx].length;
			}
		}

		total = maxLineLen;
		visible = TT_SessionBuffer(session)->pageW;
		top = TT_SessionBuffer(session)->scrollX;

		TTX_ScaleScrollValues(total, visible, top,
			&scaledTotal, &scaledVisible, &scaledTop, &shift);
		TT_SessionBuffer(session)->scrollXShift = shift;

		session->scroll.suppressIcmp = TRUE;
		SetGadgetAttrs(gadget, session->window, NULL,
			PGA_Total, scaledTotal,
			PGA_Visible, scaledVisible,
			PGA_Top, scaledTop,
			TAG_DONE);
		session->scroll.suppressIcmp = FALSE;
	}
}

/****************************************************************************/

BOOL
TTX_BoopsiHandleScrollGadgetUp(
	struct Session *session,
	ULONG gadgetID)
{
	struct Gadget *gadget;
	ULONG scaledValue;
	ULONG newScroll;

	if (!session || !TT_SessionBuffer(session))
		return FALSE;

	if (gadgetID == GID_VERT_PROP) {
		gadget = session->scroll.vertProp;
		if (!gadget)
			return FALSE;
		GetAttr(PGA_Top, gadget, &scaledValue);
		newScroll = scaledValue;
		if (TT_SessionBuffer(session)->scrollYShift > 0)
			newScroll <<= TT_SessionBuffer(session)->scrollYShift;
		if (newScroll > TT_SessionBuffer(session)->maxScrollY)
			newScroll = TT_SessionBuffer(session)->maxScrollY;
		TT_SessionBuffer(session)->scrollY = newScroll;
		return TRUE;
	}

	if (gadgetID == GID_HORIZ_PROP) {
		gadget = session->scroll.horizProp;
		if (!gadget)
			return FALSE;
		GetAttr(PGA_Top, gadget, &scaledValue);
		newScroll = scaledValue;
		if (TT_SessionBuffer(session)->scrollXShift > 0)
			newScroll <<= TT_SessionBuffer(session)->scrollXShift;
		if (newScroll > TT_SessionBuffer(session)->maxScrollX)
			newScroll = TT_SessionBuffer(session)->maxScrollX;
		TT_SessionBuffer(session)->scrollX = newScroll;
		return TRUE;
	}

	return FALSE;
}

/****************************************************************************/

BOOL
TTX_BoopsiHandleIdcmpUpdate(
	struct Session *session,
	ULONG gadgetID)
{
	struct Gadget *gadget;
	ULONG scaledValue;
	ULONG newScroll;
	BOOL changed;

	if (!session || !TT_SessionBuffer(session))
		return FALSE;
	if (session->scroll.suppressIcmp)
		return FALSE;

	changed = FALSE;

	if (gadgetID == GID_VERT_PROP) {
		gadget = session->scroll.vertProp;
		if (!gadget)
			return FALSE;
		GetAttr(PGA_Top, gadget, &scaledValue);
		newScroll = scaledValue;
		if (TT_SessionBuffer(session)->scrollYShift > 0)
			newScroll <<= TT_SessionBuffer(session)->scrollYShift;
		if (newScroll > TT_SessionBuffer(session)->maxScrollY)
			newScroll = TT_SessionBuffer(session)->maxScrollY;
		if (newScroll != TT_SessionBuffer(session)->scrollY) {
			TT_SessionBuffer(session)->scrollY = newScroll;
			changed = TRUE;
		}
	} else if (gadgetID == GID_HORIZ_PROP) {
		gadget = session->scroll.horizProp;
		if (!gadget)
			return FALSE;
		GetAttr(PGA_Top, gadget, &scaledValue);
		newScroll = scaledValue;
		if (TT_SessionBuffer(session)->scrollXShift > 0)
			newScroll <<= TT_SessionBuffer(session)->scrollXShift;
		if (newScroll > TT_SessionBuffer(session)->maxScrollX)
			newScroll = TT_SessionBuffer(session)->maxScrollX;
		if (newScroll != TT_SessionBuffer(session)->scrollX) {
			TT_SessionBuffer(session)->scrollX = newScroll;
			changed = TRUE;
		}
	} else {
		return FALSE;
	}

	return changed;
}

/****************************************************************************/

VOID
UpdateScrollBars(struct Session *session)
{
	TTX_BoopsiUpdateScrollGadgets(session);
}

/****************************************************************************/
