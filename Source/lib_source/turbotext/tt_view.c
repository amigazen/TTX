/*
 * turbotext.library - view paint model
 *
 * PrepareView builds TTView.paint from the document + folds so the driver
 * only blits rows. ScrollY remains the document line of the top visible row.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "private/tt_internal.h"

/****************************************************************************/

VOID
TT_EnsureCursorVisible(struct TTView *view, struct TTTextBuffer *buf)
{
	ULONG pageH;
	ULONG cursorVis;
	ULONG topVis;
	ULONG maxTopVis;
	ULONG visCount;

	if (!view || !buf)
		return;

	TT_FoldClampCursor(buf);
	view->cursorY = buf->cursorY;
	view->cursorX = buf->cursorX;

	pageH = buf->pageH;
	if (pageH < 1)
		pageH = 1;
	if (pageH > TT_MAX_PAINT_ROWS)
		pageH = TT_MAX_PAINT_ROWS;

	visCount = TT_FoldVisibleCount(buf);
	if (visCount < 1)
		visCount = 1;

	cursorVis = TT_FoldDocToVisible(buf, buf->cursorY);
	topVis = TT_FoldDocToVisible(buf, view->scrollY);

	/* If scrollY landed on a hidden line, snap to nearest visible. */
	if (view->scrollY < buf->lineCount &&
	    TT_FoldIsLineHidden(buf, view->scrollY)) {
		view->scrollY = TT_FoldPrevVisible(buf, view->scrollY);
		topVis = TT_FoldDocToVisible(buf, view->scrollY);
	}

	if (cursorVis < topVis) {
		topVis = cursorVis;
	} else if (cursorVis >= topVis + pageH) {
		topVis = cursorVis - pageH + 1;
	}

	if (visCount > pageH)
		maxTopVis = visCount - pageH;
	else
		maxTopVis = 0;
	if (topVis > maxTopVis)
		topVis = maxTopVis;

	view->scrollY = TT_FoldVisibleToDoc(buf, topVis);
	buf->scrollY = view->scrollY;

	if (visCount > pageH)
		buf->maxScrollY = TT_FoldVisibleToDoc(buf, visCount - pageH);
	else
		buf->maxScrollY = 0;
}

BOOL
TT_Cmd_PrepareView(
	struct TTDocument *doc,
	struct TTView *view,
	struct TTTextBuffer *buf)
{
	ULONG pageH;
	ULONG y;
	ULONG rows;
	ULONG cursorRow;
	struct TTViewPaint *paint;

	(void)doc;
	if (!view || !buf)
		return FALSE;

	paint = &view->paint;
	pageH = buf->pageH;
	if (pageH < 1)
		pageH = 1;
	if (pageH > TT_MAX_PAINT_ROWS)
		pageH = TT_MAX_PAINT_ROWS;

	TT_EnsureCursorVisible(view, buf);

	/* Reserve gutter columns when folds exist (marker / fold line). */
	if (TT_FoldHasAny(buf) && buf->leftMargin < 2)
		buf->leftMargin = 2;

	rows = 0;
	cursorRow = 0xFFFFFFFFUL;
	y = view->scrollY;
	while (y < buf->lineCount && TT_FoldIsLineHidden(buf, y))
		y++;

	while (y < buf->lineCount && rows < pageH) {
		if (!TT_FoldIsLineHidden(buf, y)) {
			paint->rows[rows].docY = y;
			paint->rows[rows].text = buf->lines[y].text;
			paint->rows[rows].length = buf->lines[y].length;
			paint->rows[rows].chrome = TT_FoldChromeForLine(buf, y);
			if (y == buf->cursorY)
				cursorRow = rows;
			rows++;
		}
		y++;
	}

	paint->rowCount = rows;
	paint->cursorRow = cursorRow;
	paint->cursorX = buf->cursorX;
	paint->visibleLineCount = TT_FoldVisibleCount(buf);

	view->cursorX = buf->cursorX;
	view->cursorY = buf->cursorY;
	view->scrollX = buf->scrollX;
	view->scrollY = buf->scrollY;
	return TRUE;
}
