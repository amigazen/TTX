/*
 * turbotext.library - document folds (range tree)
 *
 * Folds hide body lines under a header. Text stays in the buffer; visibility
 * drives PrepareView / motion. Nested = folds whose range lies inside another.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "private/tt_internal.h"

/****************************************************************************/

struct TTFold {
	struct TTFold *next;
	ULONG headerY;
	ULONG endY;   /* inclusive last body line; endY > headerY when valid */
	BOOL shown;
};

/****************************************************************************/

static ULONG
TT_FoldTabWidth(VOID)
{
	return TT_DEFAULT_TAB_WIDTH;
}

static ULONG
TT_FoldLineIndent(struct TTTextBuffer *buf, ULONG lineY)
{
	STRPTR text;
	ULONG col;
	ULONG i;
	ULONG tw;

	col = 0;
	i = 0;
	tw = TT_FoldTabWidth();
	if (!buf || lineY >= buf->lineCount || !buf->lines[lineY].text)
		return 0;
	text = buf->lines[lineY].text;
	while (text[i] == ' ' || text[i] == '\t') {
		if (text[i] == '\t')
			col += tw - (col % tw);
		else
			col++;
		i++;
	}
	return col;
}

static BOOL
TT_FoldLineIsBlank(struct TTTextBuffer *buf, ULONG lineY)
{
	STRPTR text;
	ULONG i;

	if (!buf || lineY >= buf->lineCount || !buf->lines[lineY].text)
		return TRUE;
	text = buf->lines[lineY].text;
	i = 0;
	while (text[i] == ' ' || text[i] == '\t')
		i++;
	return (BOOL)(text[i] == '\0');
}

static struct TTFold *
TT_FoldList(struct TTTextBuffer *buf)
{
	if (!buf)
		return NULL;
	return (struct TTFold *)buf->folds;
}

static VOID
TT_FoldSetList(struct TTTextBuffer *buf, struct TTFold *list)
{
	if (buf)
		buf->folds = (APTR)list;
}

static VOID
TT_FoldUnlink(struct TTTextBuffer *buf, struct TTFold *target)
{
	struct TTFold *f;
	struct TTFold *prev;

	prev = NULL;
	f = TT_FoldList(buf);
	while (f) {
		if (f == target) {
			if (prev)
				prev->next = f->next;
			else
				TT_FoldSetList(buf, f->next);
			TT_Free(f);
			return;
		}
		prev = f;
		f = f->next;
	}
}

/****************************************************************************/

VOID
TT_FoldFreeAll(struct TTTextBuffer *buf)
{
	struct TTFold *f;
	struct TTFold *n;

	if (!buf)
		return;
	f = TT_FoldList(buf);
	while (f) {
		n = f->next;
		TT_Free(f);
		f = n;
	}
	TT_FoldSetList(buf, NULL);
}

BOOL
TT_FoldHasAny(struct TTTextBuffer *buf)
{
	return (BOOL)(TT_FoldList(buf) != NULL);
}

BOOL
TT_FoldIsLineHidden(struct TTTextBuffer *buf, ULONG lineY)
{
	struct TTFold *f;

	f = TT_FoldList(buf);
	while (f) {
		if (!f->shown && lineY > f->headerY && lineY <= f->endY)
			return TRUE;
		f = f->next;
	}
	return FALSE;
}

UBYTE
TT_FoldChromeForLine(struct TTTextBuffer *buf, ULONG lineY)
{
	struct TTFold *f;
	UBYTE chrome;

	chrome = TTPAINT_NONE;
	f = TT_FoldList(buf);
	while (f) {
		if (f->headerY == lineY) {
			if (!f->shown)
				chrome = (UBYTE)(chrome | TTPAINT_MARKER);
		} else if (f->shown && lineY > f->headerY && lineY <= f->endY) {
			chrome = (UBYTE)(chrome | TTPAINT_GUTTER);
		}
		f = f->next;
	}
	return chrome;
}

ULONG
TT_FoldVisibleCount(struct TTTextBuffer *buf)
{
	ULONG y;
	ULONG n;

	n = 0;
	if (!buf)
		return 0;
	for (y = 0; y < buf->lineCount; y++) {
		if (!TT_FoldIsLineHidden(buf, y))
			n++;
	}
	return n;
}

ULONG
TT_FoldNextVisible(struct TTTextBuffer *buf, ULONG lineY)
{
	ULONG y;

	if (!buf || buf->lineCount == 0)
		return 0;
	y = lineY + 1;
	while (y < buf->lineCount && TT_FoldIsLineHidden(buf, y))
		y++;
	if (y >= buf->lineCount)
		return lineY;
	return y;
}

ULONG
TT_FoldPrevVisible(struct TTTextBuffer *buf, ULONG lineY)
{
	ULONG y;

	if (!buf || buf->lineCount == 0)
		return 0;
	if (lineY == 0)
		return 0;
	y = lineY - 1;
	while (y > 0 && TT_FoldIsLineHidden(buf, y))
		y--;
	if (TT_FoldIsLineHidden(buf, y))
		return 0;
	return y;
}

ULONG
TT_FoldDocToVisible(struct TTTextBuffer *buf, ULONG docY)
{
	ULONG y;
	ULONG vis;

	vis = 0;
	if (!buf)
		return 0;
	for (y = 0; y < buf->lineCount && y < docY; y++) {
		if (!TT_FoldIsLineHidden(buf, y))
			vis++;
	}
	return vis;
}

ULONG
TT_FoldVisibleToDoc(struct TTTextBuffer *buf, ULONG visIndex)
{
	ULONG y;
	ULONG seen;

	seen = 0;
	if (!buf || buf->lineCount == 0)
		return 0;
	for (y = 0; y < buf->lineCount; y++) {
		if (TT_FoldIsLineHidden(buf, y))
			continue;
		if (seen == visIndex)
			return y;
		seen++;
	}
	return buf->lineCount - 1;
}

VOID
TT_FoldClampCursor(struct TTTextBuffer *buf)
{
	if (!buf || buf->lineCount == 0)
		return;
	if (TT_FoldIsLineHidden(buf, buf->cursorY))
		buf->cursorY = TT_FoldPrevVisible(buf, buf->cursorY);
	if (buf->cursorY >= buf->lineCount)
		buf->cursorY = buf->lineCount - 1;
	if (buf->cursorX > buf->lines[buf->cursorY].length)
		buf->cursorX = buf->lines[buf->cursorY].length;
}

VOID
TT_FoldAdjustInsert(struct TTTextBuffer *buf, ULONG atY)
{
	struct TTFold *f;

	f = TT_FoldList(buf);
	while (f) {
		if (f->headerY >= atY)
			f->headerY++;
		if (f->endY >= atY)
			f->endY++;
		f = f->next;
	}
}

VOID
TT_FoldAdjustDelete(struct TTTextBuffer *buf, ULONG atY)
{
	struct TTFold *f;
	struct TTFold *n;
	struct TTFold *kept;

	kept = NULL;
	f = TT_FoldList(buf);
	TT_FoldSetList(buf, NULL);
	while (f) {
		n = f->next;
		f->next = NULL;
		if (f->headerY == atY) {
			TT_Free(f);
		} else {
			if (f->headerY > atY) {
				f->headerY--;
				if (f->endY > 0)
					f->endY--;
			} else if (f->endY >= atY) {
				if (f->endY > 0)
					f->endY--;
			}
			if (f->endY <= f->headerY) {
				TT_Free(f);
			} else {
				f->next = kept;
				kept = f;
			}
		}
		f = n;
	}
	while (kept) {
		n = kept->next;
		kept->next = TT_FoldList(buf);
		TT_FoldSetList(buf, kept);
		kept = n;
	}
}

/****************************************************************************/

static struct TTFold *
TT_FoldFindAtCursor(struct TTTextBuffer *buf)
{
	struct TTFold *f;
	struct TTFold *best;
	ULONG y;

	best = NULL;
	y = buf->cursorY;
	f = TT_FoldList(buf);
	while (f) {
		if (f->headerY == y) {
			best = f;
			break;
		}
		if (f->shown && y > f->headerY && y <= f->endY) {
			if (!best || f->headerY > best->headerY)
				best = f;
		}
		f = f->next;
	}
	return best;
}

static BOOL
TT_FoldIsNestedIn(struct TTFold *inner, struct TTFold *outer)
{
	if (!inner || !outer || inner == outer)
		return FALSE;
	return (BOOL)(inner->headerY > outer->headerY &&
		inner->endY <= outer->endY);
}

static ULONG
TT_FoldParseMode(STRPTR *args, ULONG argCount)
{
	ULONG i;

	/* 0=single, 1=nested, 2=all */
	for (i = 0; i < argCount; i++) {
		if (!args[i])
			continue;
		if (Stricmp(args[i], "All") == 0)
			return 2;
		if (Stricmp(args[i], "Nested") == 0)
			return 1;
	}
	return 0;
}

static VOID
TT_FoldSetShown(struct TTFold *f, BOOL shown)
{
	if (f)
		f->shown = shown;
}

static VOID
TT_FoldApplyScope(
	struct TTTextBuffer *buf,
	struct TTFold *anchor,
	ULONG mode,
	BOOL shown)
{
	struct TTFold *f;

	if (mode == 2) {
		f = TT_FoldList(buf);
		while (f) {
			TT_FoldSetShown(f, shown);
			f = f->next;
		}
		return;
	}
	if (!anchor)
		return;
	TT_FoldSetShown(anchor, shown);
	if (mode == 1) {
		f = TT_FoldList(buf);
		while (f) {
			if (TT_FoldIsNestedIn(f, anchor))
				TT_FoldSetShown(f, shown);
			f = f->next;
		}
	}
}

static BOOL
TT_FoldCreate(struct TTTextBuffer *buf, ULONG headerY, ULONG endY)
{
	struct TTFold *f;

	if (!buf || endY <= headerY || headerY >= buf->lineCount)
		return FALSE;
	if (endY >= buf->lineCount)
		endY = buf->lineCount - 1;
	f = (struct TTFold *)TT_Alloc(sizeof(struct TTFold), MEMF_CLEAR);
	if (!f)
		return FALSE;
	f->headerY = headerY;
	f->endY = endY;
	f->shown = FALSE;
	f->next = TT_FoldList(buf);
	TT_FoldSetList(buf, f);
	return TRUE;
}

BOOL
TT_Cmd_MakeFold(struct TTDocument *doc, struct TTTextBuffer *buf)
{
	ULONG headerY;
	ULONG endY;
	ULONG y;
	ULONG baseIndent;
	ULONG ind;
	ULONG y0;
	ULONG y1;

	(void)doc;
	if (!buf || buf->lineCount < 2)
		return FALSE;

	if (buf->marking.enabled) {
		y0 = buf->marking.startY;
		y1 = buf->marking.stopY;
		if (y1 < y0) {
			y = y0;
			y0 = y1;
			y1 = y;
		}
		if (y1 <= y0)
			return FALSE;
		headerY = y0;
		endY = y1;
		TT_ClearMarking(buf);
	} else {
		headerY = buf->cursorY;
		baseIndent = TT_FoldLineIndent(buf, headerY);
		endY = headerY;
		for (y = headerY + 1; y < buf->lineCount; y++) {
			if (TT_FoldLineIsBlank(buf, y)) {
				endY = y;
				continue;
			}
			ind = TT_FoldLineIndent(buf, y);
			if (ind <= baseIndent)
				break;
			endY = y;
		}
		if (endY <= headerY)
			return FALSE;
	}

	if (!TT_FoldCreate(buf, headerY, endY))
		return FALSE;
	buf->cursorY = headerY;
	TT_FoldClampCursor(buf);
	return TRUE;
}

BOOL
TT_Cmd_ShowFold(struct TTTextBuffer *buf, STRPTR *args, ULONG argCount)
{
	struct TTFold *anchor;
	ULONG mode;

	if (!buf)
		return FALSE;
	mode = TT_FoldParseMode(args, argCount);
	anchor = TT_FoldFindAtCursor(buf);
	if (mode != 2 && !anchor)
		return FALSE;
	TT_FoldApplyScope(buf, anchor, mode, TRUE);
	return TRUE;
}

BOOL
TT_Cmd_HideFold(struct TTTextBuffer *buf, STRPTR *args, ULONG argCount)
{
	struct TTFold *anchor;
	ULONG mode;

	if (!buf)
		return FALSE;
	mode = TT_FoldParseMode(args, argCount);
	anchor = TT_FoldFindAtCursor(buf);
	if (mode != 2 && !anchor)
		return FALSE;
	TT_FoldApplyScope(buf, anchor, mode, FALSE);
	TT_FoldClampCursor(buf);
	return TRUE;
}

BOOL
TT_Cmd_ToggleFold(struct TTTextBuffer *buf)
{
	struct TTFold *f;

	if (!buf)
		return FALSE;
	f = TT_FoldFindAtCursor(buf);
	if (!f)
		return FALSE;
	f->shown = (BOOL)(!f->shown);
	TT_FoldClampCursor(buf);
	return TRUE;
}

BOOL
TT_Cmd_UnmakeFold(struct TTTextBuffer *buf, STRPTR *args, ULONG argCount)
{
	struct TTFold *anchor;
	struct TTFold *f;
	struct TTFold *n;
	ULONG mode;

	if (!buf)
		return FALSE;
	mode = TT_FoldParseMode(args, argCount);
	if (mode == 2) {
		TT_FoldFreeAll(buf);
		return TRUE;
	}
	anchor = TT_FoldFindAtCursor(buf);
	if (!anchor)
		return FALSE;
	if (mode == 1) {
		f = TT_FoldList(buf);
		while (f) {
			n = f->next;
			if (TT_FoldIsNestedIn(f, anchor))
				TT_FoldUnlink(buf, f);
			f = n;
		}
	}
	TT_FoldUnlink(buf, anchor);
	return TRUE;
}
