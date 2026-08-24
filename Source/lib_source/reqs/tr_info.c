/*
 * ttxreqs.library - non-modal Info window (OpenRequester Info)
 *
 * Panel frames match original TurboText Info: recessed BBFT_RIDGE groove
 * (recessed outer + raised inner lip). Live document stats from the driver.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "compiler.h"
#include "private/tr_internal.h"
#include "tr_funcs.h"

#include <exec/memory.h>
#include <libraries/gadtools.h>
#include <utility/tagitem.h>
#include <dos/datetime.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/utility.h>

/****************************************************************************/

#define TTX_INFO_MARGIN    8
#define TTX_INFO_GAP       6
#define TTX_INFO_PAD       6
#define TTX_INFO_GUTTER    10

static STRPTR s_monthNames[12] = {
	(STRPTR)"Jan", (STRPTR)"Feb", (STRPTR)"Mar", (STRPTR)"Apr",
	(STRPTR)"May", (STRPTR)"Jun", (STRPTR)"Jul", (STRPTR)"Aug",
	(STRPTR)"Sep", (STRPTR)"Oct", (STRPTR)"Nov", (STRPTR)"Dec"
};

static STRPTR s_infoLabels[9] = {
	(STRPTR)"ARexx port name:",
	(STRPTR)"Visible lines:",
	(STRPTR)"Folded lines:",
	(STRPTR)"Total lines:",
	(STRPTR)"Characters:",
	(STRPTR)"Average characters/line:",
	(STRPTR)"Available memory:",
	(STRPTR)"Date:",
	(STRPTR)"Time:"
};

/****************************************************************************/

static VOID
TR_InfoFormatCommas(STRPTR dst, ULONG dstLen, ULONG val)
{
	TEXT digits[16];
	ULONG n;
	ULONG pos;
	ULONG digLeft;

	if (!dst || dstLen == 0)
		return;
	dst[0] = '\0';
	n = 0;
	if (val == 0)
		digits[n++] = '0';
	else {
		while (val > 0 && n < 15) {
			digits[n++] = (TEXT)('0' + (val % 10));
			val /= 10;
		}
	}
	pos = 0;
	digLeft = n;
	while (digLeft > 0 && pos < dstLen - 1) {
		dst[pos++] = digits[--digLeft];
		if (digLeft > 0 && (digLeft % 3) == 0 && pos < dstLen - 1)
			dst[pos++] = ',';
	}
	dst[pos] = '\0';
}

static VOID
TR_InfoFormatULong(STRPTR dst, ULONG dstLen, ULONG val)
{
	TEXT tmp[16];
	ULONG digits;
	ULONG pos;

	if (!dst || dstLen == 0)
		return;
	dst[0] = '\0';
	digits = 0;
	pos = 0;
	if (val == 0)
		tmp[digits++] = '0';
	else {
		while (val > 0 && digits < 15) {
			tmp[digits++] = (TEXT)('0' + (val % 10));
			val /= 10;
		}
	}
	while (digits > 0 && pos < dstLen - 1)
		dst[pos++] = tmp[--digits];
	dst[pos] = '\0';
}

static VOID
TR_InfoDrawCentered(
	struct RastPort *rp,
	LONG left,
	LONG right,
	LONG baselineY,
	STRPTR text)
{
	LONG tw;
	LONG x;
	ULONG len;

	if (!rp || !text)
		return;
	len = 0;
	while (text[len] != '\0')
		len++;
	tw = TextLength(rp, text, (ULONG)len);
	x = left + ((right - left) - tw) / 2;
	if (x < left)
		x = left;
	Move(rp, x, baselineY);
	Text(rp, text, (ULONG)len);
}

static VOID
TR_InfoDrawRow(
	struct RastPort *rp,
	LONG labelRight,
	LONG valueLeft,
	LONG baselineY,
	STRPTR label,
	STRPTR value)
{
	LONG tw;
	ULONG len;

	if (!rp || !label || !value)
		return;
	len = 0;
	while (label[len] != '\0')
		len++;
	tw = TextLength(rp, label, (ULONG)len);
	Move(rp, labelRight - tw, baselineY);
	Text(rp, label, (ULONG)len);
	len = 0;
	while (value[len] != '\0')
		len++;
	Move(rp, valueLeft, baselineY);
	Text(rp, value, (ULONG)len);
}

static VOID
TR_InfoNormalizeDate(STRPTR dateBuf, ULONG bufLen)
{
	TEXT day[8];
	TEXT mon[8];
	TEXT year[8];
	TEXT out[16];
	ULONG di;
	ULONG p;
	ULONG m;
	ULONG oi;

	if (!dateBuf || bufLen < 9)
		return;

	day[0] = '\0';
	mon[0] = '\0';
	year[0] = '\0';
	di = 0;
	p = 0;
	while (dateBuf[p] != '\0' && dateBuf[p] != '-' && dateBuf[p] != '/' &&
	       di < 7) {
		day[di++] = dateBuf[p++];
	}
	day[di] = '\0';
	if (dateBuf[p] == '-' || dateBuf[p] == '/')
		p++;
	di = 0;
	while (dateBuf[p] != '\0' && dateBuf[p] != '-' && dateBuf[p] != '/' &&
	       di < 7) {
		mon[di++] = dateBuf[p++];
	}
	mon[di] = '\0';
	if (dateBuf[p] == '-' || dateBuf[p] == '/')
		p++;
	di = 0;
	while (dateBuf[p] != '\0' && di < 7) {
		year[di++] = dateBuf[p++];
	}
	year[di] = '\0';

	oi = 0;
	p = 0;
	while (day[p] != '\0' && oi < 15)
		out[oi++] = day[p++];
	if (oi < 15)
		out[oi++] = '/';

	if (mon[0] >= '0' && mon[0] <= '9') {
		p = 0;
		while (mon[p] != '\0' && oi < 15)
			out[oi++] = mon[p++];
	} else {
		for (m = 0; m < 12; m++) {
			if (Strnicmp(mon, s_monthNames[m], 3) == 0) {
				if (oi < 15)
					out[oi++] = (TEXT)('0' + ((m + 1) / 10));
				if (oi < 15)
					out[oi++] = (TEXT)('0' + ((m + 1) % 10));
				break;
			}
		}
		if (m >= 12) {
			p = 0;
			while (mon[p] != '\0' && oi < 15)
				out[oi++] = mon[p++];
		}
	}

	if (oi < 15)
		out[oi++] = '/';
	p = 0;
	while (year[p] != '\0' && oi < 15)
		out[oi++] = year[p++];
	out[oi] = '\0';

	p = 0;
	while (out[p] != '\0' && p < bufLen - 1) {
		dateBuf[p] = out[p];
		p++;
	}
	dateBuf[p] = '\0';
}

static VOID
TR_InfoCopyStats(struct TRInfoStats *dst, struct TRInfoStats *src)
{
	if (!dst)
		return;
	dst->arexxPortName = NULL;
	dst->visibleLines = 0;
	dst->foldedLines = 0;
	dst->totalLines = 0;
	dst->characters = 0;
	dst->avgCharsPerLine = 0;
	if (!src)
		return;
	dst->arexxPortName = src->arexxPortName;
	dst->visibleLines = src->visibleLines;
	dst->foldedLines = src->foldedLines;
	dst->totalLines = src->totalLines;
	dst->characters = src->characters;
	dst->avgCharsPerLine = src->avgCharsPerLine;
}

static struct TRInfoStats *
TR_InfoGetStored(struct Window *win)
{
	if (!win)
		return NULL;
	return (struct TRInfoStats *)win->UserData;
}

static VOID
TR_InfoDrawPanelBevel(
	struct RastPort *rp,
	APTR vi,
	LONG left,
	LONG top,
	LONG width,
	LONG height)
{
	struct TagItem tags[4];

	if (!rp || !vi || width < 6 || height < 6)
		return;

	/*
	 * BBFT_RIDGE + recessed = groove: recessed outer with a raised lip
	 * immediately inside (string-gadget / display-panel look). Nested
	 * BBFT_BUTTON frames at +1px cancel each other and look like a single
	 * bevel.
	 */
	tags[0].ti_Tag = GT_VisualInfo;
	tags[0].ti_Data = (ULONG)vi;
	tags[1].ti_Tag = GTBB_Recessed;
	tags[1].ti_Data = TRUE;
	tags[2].ti_Tag = GTBB_FrameType;
	tags[2].ti_Data = BBFT_RIDGE;
	tags[3].ti_Tag = TAG_DONE;
	tags[3].ti_Data = 0;
	DrawBevelBoxA(rp, left, top, width, height, tags);
}

static LONG
TR_InfoTextLen(struct RastPort *rp, STRPTR text)
{
	ULONG len;

	if (!rp || !text)
		return 0;
	len = 0;
	while (text[len] != '\0')
		len++;
	return TextLength(rp, text, (ULONG)len);
}

static VOID
TR_InfoPaint(struct Window *win, struct TRInfoStats *stats)
{
	struct RastPort *rp;
	struct DrawInfo *dri;
	APTR vi;
	ULONG penText;
	ULONG penBack;
	LONG clientL;
	LONG clientT;
	LONG clientR;
	LONG clientB;
	LONG aboutL;
	LONG aboutT;
	LONG aboutW;
	LONG aboutH;
	LONG metaL;
	LONG metaT;
	LONG metaW;
	LONG metaH;
	LONG lineH;
	LONG baseline;
	LONG base;
	LONG labelRight;
	LONG valueLeft;
	LONG labelColW;
	LONG i;
	LONG tw;
	ULONG mem;
	TEXT visBuf[32];
	TEXT foldBuf[32];
	TEXT totBuf[32];
	TEXT charBuf[32];
	TEXT avgBuf[32];
	TEXT memBuf[32];
	TEXT dateBuf[16];
	TEXT timeBuf[16];
	struct DateTime dt;
	struct DateStamp ds;
	STRPTR portName;
	STRPTR values[9];

	if (!win || !win->RPort || !win->WScreen)
		return;

	rp = win->RPort;
	penText = 1;
	penBack = 0;
	dri = GetScreenDrawInfo(win->WScreen);
	if (dri && dri->dri_Pens) {
		penBack = (ULONG)dri->dri_Pens[BACKGROUNDPEN];
		penText = (ULONG)dri->dri_Pens[TEXTPEN];
	}
	if (win->WScreen->RastPort.Font)
		SetFont(rp, win->WScreen->RastPort.Font);

	lineH = rp->Font ? rp->Font->tf_YSize : 8;
	baseline = rp->Font ? rp->Font->tf_Baseline : 6;
	if (lineH < 8)
		lineH = 8;

	vi = GetVisualInfo(win->WScreen, TAG_DONE);
	if (!vi) {
		if (dri)
			FreeScreenDrawInfo(win->WScreen, dri);
		return;
	}

	clientL = (LONG)win->BorderLeft;
	clientT = (LONG)win->BorderTop;
	clientR = (LONG)win->Width - (LONG)win->BorderRight - 1;
	clientB = (LONG)win->Height - (LONG)win->BorderBottom - 1;

	SetAPen(rp, (ULONG)penBack);
	SetBPen(rp, (ULONG)penBack);
	SetDrMd(rp, JAM2);
	RectFill(rp, clientL, clientT, clientR, clientB);

	/*
	 * Screenshot layout: two double-bevel panels on the window background —
	 * About (centered) above, then the stats list. No third outer frame.
	 */
	aboutL = clientL + TTX_INFO_MARGIN;
	aboutT = clientT + TTX_INFO_MARGIN;
	aboutW = (clientR - clientL + 1) - 2 * TTX_INFO_MARGIN;
	aboutH = 3 * lineH + 2 * TTX_INFO_PAD + 4;
	if (aboutW < 40)
		aboutW = 40;

	metaL = aboutL;
	metaT = aboutT + aboutH + TTX_INFO_GAP;
	metaW = aboutW;
	metaH = 9 * (lineH + 1) + 2 * TTX_INFO_PAD;
	if (metaT + metaH > clientB - TTX_INFO_MARGIN + 1)
		metaH = clientB - TTX_INFO_MARGIN + 1 - metaT;
	if (metaH < 40)
		metaH = 40;

	TR_InfoDrawPanelBevel(rp, vi, aboutL, aboutT, aboutW, aboutH);
	TR_InfoDrawPanelBevel(rp, vi, metaL, metaT, metaW, metaH);

	SetAPen(rp, (ULONG)penText);
	SetBPen(rp, (ULONG)penBack);
	SetDrMd(rp, JAM2);

	base = aboutT + TTX_INFO_PAD + baseline;
	TR_InfoDrawCentered(rp, aboutL + 4, aboutL + aboutW - 4, base,
		(STRPTR)"TTX 3.0");
	base += lineH + 1;
	TR_InfoDrawCentered(rp, aboutL + 4, aboutL + aboutW - 4, base,
		(STRPTR)"Copyright (c) 2025 amigazen project");
	base += lineH + 1;
	TR_InfoDrawCentered(rp, aboutL + 4, aboutL + aboutW - 4, base,
		(STRPTR)"All Rights Reserved");

	portName = (STRPTR)"(none)";
	if (stats && stats->arexxPortName && stats->arexxPortName[0])
		portName = stats->arexxPortName;

	mem = AvailMem(MEMF_ANY);
	DateStamp(&ds);
	dt.dat_Stamp = ds;
	dt.dat_Format = FORMAT_CDN;
	dt.dat_Flags = 0;
	dt.dat_StrDay = NULL;
	dt.dat_StrDate = dateBuf;
	dt.dat_StrTime = timeBuf;
	dateBuf[0] = '\0';
	timeBuf[0] = '\0';
	DateToStr(&dt);
	TR_InfoNormalizeDate(dateBuf, sizeof(dateBuf));

	TR_InfoFormatULong(visBuf, sizeof(visBuf),
		stats ? stats->visibleLines : 0);
	TR_InfoFormatULong(foldBuf, sizeof(foldBuf),
		stats ? stats->foldedLines : 0);
	TR_InfoFormatULong(totBuf, sizeof(totBuf),
		stats ? stats->totalLines : 0);
	TR_InfoFormatCommas(charBuf, sizeof(charBuf),
		stats ? stats->characters : 0);
	TR_InfoFormatULong(avgBuf, sizeof(avgBuf),
		stats ? stats->avgCharsPerLine : 0);
	TR_InfoFormatCommas(memBuf, sizeof(memBuf), mem);

	values[0] = portName;
	values[1] = visBuf;
	values[2] = foldBuf;
	values[3] = totBuf;
	values[4] = charBuf;
	values[5] = avgBuf;
	values[6] = memBuf;
	values[7] = dateBuf;
	values[8] = timeBuf;

	labelColW = 0;
	for (i = 0; i < 9; i++) {
		tw = TR_InfoTextLen(rp, s_infoLabels[i]);
		if (tw > labelColW)
			labelColW = tw;
	}
	labelRight = metaL + TTX_INFO_PAD + 2 + labelColW;
	valueLeft = labelRight + TTX_INFO_GUTTER;
	if (valueLeft > metaL + metaW - 8)
		valueLeft = metaL + metaW / 2 + 4;

	base = metaT + TTX_INFO_PAD + baseline;
	for (i = 0; i < 9; i++) {
		TR_InfoDrawRow(rp, labelRight, valueLeft, base,
			s_infoLabels[i], values[i]);
		base += lineH + 1;
	}

	FreeVisualInfo(vi);
	if (dri)
		FreeScreenDrawInfo(win->WScreen, dri);
}

/****************************************************************************/

struct Window *
TR_LVO TR_InfoOpen(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, struct TRInfoStats *stats))
{
	struct Screen *scr;
	struct Window *win;
	struct TRInfoStats *stored;
	struct RastPort *rp;
	LONG left;
	LONG top;
	LONG lineH;
	LONG labelColW;
	LONG valueColW;
	LONG innerW;
	LONG innerH;
	LONG aboutH;
	LONG metaH;
	LONG i;
	LONG tw;
	BOOL unlock;

	(void)lib;
	if (!parent)
		return NULL;

	scr = parent->WScreen;
	unlock = FALSE;
	if (!scr) {
		scr = LockPubScreen(NULL);
		unlock = TRUE;
	}
	if (!scr)
		return NULL;

	rp = &scr->RastPort;
	lineH = (rp->Font) ? rp->Font->tf_YSize : 8;
	if (lineH < 8)
		lineH = 8;

	labelColW = 0;
	for (i = 0; i < 9; i++) {
		tw = TR_InfoTextLen(rp, s_infoLabels[i]);
		if (tw > labelColW)
			labelColW = tw;
	}
	/* Value column: long ARexx name or comma memory figure. */
	valueColW = TR_InfoTextLen(rp, (STRPTR)"TURBOTEXT99");
	tw = TR_InfoTextLen(rp, (STRPTR)"999,999,999");
	if (tw > valueColW)
		valueColW = tw;

	innerW = 2 * TTX_INFO_MARGIN + 2 * TTX_INFO_PAD + 4 +
		labelColW + TTX_INFO_GUTTER + valueColW + 8;
	if (innerW < 280)
		innerW = 280;
	if (innerW > 400)
		innerW = 400;

	aboutH = 3 * lineH + 2 * TTX_INFO_PAD + 4;
	metaH = 9 * (lineH + 1) + 2 * TTX_INFO_PAD;
	innerH = 2 * TTX_INFO_MARGIN + aboutH + TTX_INFO_GAP + metaH;

	left = parent->LeftEdge + 40;
	top = parent->TopEdge + 40;
	if (left < 0)
		left = 0;
	if (top < 0)
		top = 0;

	win = OpenWindowTags(NULL,
		WA_Left, left,
		WA_Top, top,
		WA_InnerWidth, innerW,
		WA_InnerHeight, innerH,
		WA_Title, (ULONG)"TurboText Information",
		WA_DragBar, TRUE,
		WA_DepthGadget, TRUE,
		WA_CloseGadget, TRUE,
		WA_Activate, TRUE,
		WA_SimpleRefresh, TRUE,
		WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW,
		WA_PubScreen, (ULONG)scr,
		TAG_DONE);

	if (unlock)
		UnlockPubScreen(NULL, scr);

	if (!win)
		return NULL;

	/* Keep a private copy so SimpleRefresh can repaint without the driver. */
	stored = (struct TRInfoStats *)TR_Alloc(sizeof(struct TRInfoStats), 0);
	if (!stored) {
		CloseWindow(win);
		return NULL;
	}
	TR_InfoCopyStats(stored, stats);
	win->UserData = (APTR)stored;

	TR_InfoPaint(win, stored);
	return win;
}

VOID
TR_LVO TR_InfoClose(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct Window *infoWin))
{
	struct TRInfoStats *stored;

	(void)lib;
	if (!infoWin)
		return;
	stored = TR_InfoGetStored(infoWin);
	infoWin->UserData = NULL;
	if (stored)
		TR_Free(stored);
	CloseWindow(infoWin);
}

VOID
TR_LVO TR_InfoUpdate(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct Window *infoWin),
	TR_REG(a1, struct TRInfoStats *stats))
{
	struct TRInfoStats *stored;

	(void)lib;
	if (!infoWin)
		return;
	stored = TR_InfoGetStored(infoWin);
	if (!stored) {
		stored = (struct TRInfoStats *)TR_Alloc(sizeof(struct TRInfoStats), 0);
		if (!stored)
			return;
		infoWin->UserData = (APTR)stored;
	}
	TR_InfoCopyStats(stored, stats);
	TR_InfoPaint(infoWin, stored);
}

ULONG
TR_LVO TR_InfoProcessMsg(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct Window *infoWin),
	TR_REG(a1, struct IntuiMessage *imsg))
{
	struct TRInfoStats *stored;

	(void)lib;
	if (!infoWin || !imsg)
		return TRINFO_NOTMINE;
	if (imsg->IDCMPWindow != infoWin)
		return TRINFO_NOTMINE;

	if (imsg->Class == IDCMP_CLOSEWINDOW) {
		stored = TR_InfoGetStored(infoWin);
		infoWin->UserData = NULL;
		if (stored)
			TR_Free(stored);
		CloseWindow(infoWin);
		return TRINFO_CLOSED;
	}
	if (imsg->Class == IDCMP_REFRESHWINDOW) {
		stored = TR_InfoGetStored(infoWin);
		BeginRefresh(infoWin);
		TR_InfoPaint(infoWin, stored);
		EndRefresh(infoWin, TRUE);
		return TRINFO_HANDLED;
	}
	return TRINFO_HANDLED;
}
