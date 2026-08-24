/*
 * ttxreqs.library - font-sensitive GadTools modal requesters
 *
 * Find / Find & Change use the original TurboText two-column layout:
 * left (~60%) string fields + default Find Next (recessed outer frame),
 * right options column, separated by a single vertical recessed bevel.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 *
 * C89: declare locals at the start of each block.
 */

#include "private/tr_internal.h"
#include "tr_funcs.h"

#include <exec/memory.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <graphics/gfxmacros.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/asl.h>
#include <proto/dos.h>

#define GID_OK        1
#define GID_CANCEL    2
#define GID_EDIT      3
#define GID_FINDNEXT  4
#define GID_CHANGE    5
#define GID_CHANGEALL 6
#define GID_FINDSTR   10
#define GID_CHGSTR    11
#define GID_CHK0      20

/****************************************************************************/

static ULONG
TR_UILen(STRPTR s)
{
	ULONG n;

	n = 0;
	if (!s)
		return 0;
	while (s[n] != '\0')
		n++;
	return n;
}

static VOID
TR_UICopy(STRPTR dst, ULONG dstLen, STRPTR src)
{
	ULONG i;

	if (!dst || dstLen == 0)
		return;
	dst[0] = '\0';
	if (!src)
		return;
	i = 0;
	while (src[i] != '\0' && (i + 1UL) < dstLen) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

static VOID
TR_UIFormatLong(STRPTR dst, ULONG dstLen, LONG val)
{
	TEXT tmp[16];
	ULONG digits = 0;
	ULONG pos = 0;
	ULONG v = 0;
	BOOL neg = FALSE;

	if (!dst || dstLen == 0)
		return;
	dst[0] = '\0';
	if (val < 0) {
		neg = TRUE;
		v = (ULONG)(-val);
	} else {
		v = (ULONG)val;
	}
	if (v == 0)
		tmp[digits++] = '0';
	else {
		while (v > 0 && digits < 15) {
			tmp[digits++] = (TEXT)('0' + (v % 10));
			v /= 10;
		}
	}
	if (neg && pos < dstLen - 1)
		dst[pos++] = '-';
	while (digits > 0 && pos < dstLen - 1)
		dst[pos++] = tmp[--digits];
	dst[pos] = '\0';
}

/****************************************************************************/

BOOL
TR_GetFontMetrics(struct Window *parent, struct TRFontMetrics *m)
{
	struct Screen *scr;
	struct DrawInfo *dri;
	struct RastPort rp;
	BOOL unlock;
	static UBYTE topazName[] = "topaz.font";

	if (!m)
		return FALSE;

	unlock = FALSE;
	dri = NULL;
	scr = NULL;

	m->font = NULL;
	m->attr.ta_Name = topazName;
	m->attr.ta_YSize = 8;
	m->attr.ta_Style = FS_NORMAL;
	m->attr.ta_Flags = FPF_ROMFONT;
	m->fontY = 8;
	m->fontX = 8;
	m->rowH = 14;
	m->gap = 4;
	m->margin = 8;

	if (parent && parent->WScreen)
		scr = parent->WScreen;
	else {
		scr = LockPubScreen(NULL);
		unlock = TRUE;
	}
	if (!scr)
		return TRUE;

	dri = GetScreenDrawInfo(scr);
	if (dri && dri->dri_Font) {
		m->attr.ta_Name = dri->dri_Font->tf_Message.mn_Node.ln_Name;
		m->attr.ta_YSize = dri->dri_Font->tf_YSize;
		m->attr.ta_Style = dri->dri_Font->tf_Style;
		m->attr.ta_Flags = dri->dri_Font->tf_Flags;
	} else if (scr->Font) {
		m->attr = *scr->Font;
	}

	m->font = OpenFont(&m->attr);
	if (m->font)
		m->fontY = m->font->tf_YSize;

	InitRastPort(&rp);
	if (m->font)
		SetFont(&rp, m->font);
	m->fontX = (WORD)TextLength(&rp, "M", 1);
	if (m->fontX < 4)
		m->fontX = 8;

	m->rowH = (WORD)(m->fontY + 6);
	if (m->rowH < 12)
		m->rowH = 12;
	m->gap = (WORD)(m->fontY / 4);
	if (m->gap < 2)
		m->gap = 2;
	m->margin = m->fontX;
	if (m->margin < 6)
		m->margin = 6;

	if (dri)
		FreeScreenDrawInfo(scr, dri);
	if (unlock)
		UnlockPubScreen(NULL, scr);

	return TRUE;
}

VOID
TR_FreeFontMetrics(struct TRFontMetrics *m)
{
	if (m && m->font) {
		CloseFont(m->font);
		m->font = NULL;
	}
}

LONG
TR_TextWidth(struct TRFontMetrics *m, STRPTR text)
{
	struct RastPort rp;

	if (!text)
		return 0;
	if (!m || !m->font)
		return (LONG)(TR_UILen(text) * 8L);
	InitRastPort(&rp);
	SetFont(&rp, m->font);
	return TextLength(&rp, text, TR_UILen(text));
}

/****************************************************************************/

BOOL
TR_LVO TR_RequestBool(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, STRPTR title),
	TR_REG(a2, STRPTR prompt))
{
	struct EasyStruct es;
	LONG choice;

	(void)lib;
	es.es_StructSize = sizeof(struct EasyStruct);
	es.es_Flags = 0;
	es.es_Title = title ? (UBYTE *)title : (UBYTE *)"TTX";
	es.es_TextFormat = prompt ? (UBYTE *)prompt : (UBYTE *)"?";
	es.es_GadgetFormat = (UBYTE *)"OK|Cancel";
	choice = EasyRequestArgs(parent, &es, NULL, NULL);
	return (BOOL)(choice == 1);
}

LONG
TR_LVO TR_RequestChoice(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, STRPTR title),
	TR_REG(a2, STRPTR prompt),
	TR_REG(a3, STRPTR gadgets))
{
	struct EasyStruct es;

	(void)lib;
	es.es_StructSize = sizeof(struct EasyStruct);
	es.es_Flags = 0;
	es.es_Title = title ? (UBYTE *)title : (UBYTE *)"TTX";
	es.es_TextFormat = prompt ? (UBYTE *)prompt : (UBYTE *)"?";
	es.es_GadgetFormat = gadgets ? (UBYTE *)gadgets : (UBYTE *)"OK|Cancel";
	return EasyRequestArgs(parent, &es, NULL, NULL);
}

/****************************************************************************/

static BOOL
TR_UIEditWindow(
	struct Window *parent,
	STRPTR title,
	STRPTR prompt,
	STRPTR defStr,
	BOOL asInt,
	BOOL positiveOnly,
	STRPTR outBuf,
	ULONG outLen,
	LONG *outNum)
{
	struct TRFontMetrics m;
	struct Screen *scr;
	APTR vi;
	struct Gadget *glist;
	struct Gadget *gad;
	struct Gadget *editGad;
	struct NewGadget ng;
	struct Window *win;
	struct IntuiMessage *imsg;
	BOOL unlock;
	BOOL done;
	BOOL ok;
	WORD top;
	WORD winW;
	WORD labelW;
	WORD btnW;
	WORD editW;
	WORD y;
	ULONG class;
	UWORD code;
	LONG num;
	STRPTR s;

	glist = NULL;
	editGad = NULL;
	win = NULL;
	vi = NULL;
	scr = NULL;
	unlock = FALSE;
	done = FALSE;
	ok = FALSE;
	num = 0;

	if (!outBuf || outLen < 2)
		return FALSE;

	TR_GetFontMetrics(parent, &m);

	if (parent && parent->WScreen)
		scr = parent->WScreen;
	else {
		scr = LockPubScreen(NULL);
		unlock = TRUE;
	}
	if (!scr) {
		TR_FreeFontMetrics(&m);
		return FALSE;
	}

	vi = GetVisualInfo(scr, TAG_DONE);
	if (!vi) {
		if (unlock)
			UnlockPubScreen(NULL, scr);
		TR_FreeFontMetrics(&m);
		return FALSE;
	}

	labelW = (WORD)TR_TextWidth(&m, prompt ? prompt : (STRPTR)"");
	btnW = (WORD)TR_TextWidth(&m, "Cancel") + (WORD)(m.fontX * 2);
	if (btnW < (WORD)(m.fontX * 8))
		btnW = (WORD)(m.fontX * 8);
	editW = (WORD)(m.fontX * 28);
	if (editW < 140)
		editW = 140;

	top = (WORD)(scr->WBorTop + scr->Font->ta_YSize + 1);
	winW = (WORD)(m.margin * 2 + labelW + m.gap + editW);
	if (winW < (WORD)(m.margin * 2 + btnW * 2 + m.gap))
		winW = (WORD)(m.margin * 2 + btnW * 2 + m.gap);

	gad = CreateContext(&glist);
	if (!gad)
		goto fail;

	y = (WORD)(top + m.margin);
	ng.ng_LeftEdge = (WORD)(m.margin + labelW + m.gap);
	ng.ng_TopEdge = y;
	ng.ng_Width = editW;
	ng.ng_Height = m.rowH;
	ng.ng_GadgetText = prompt ? (UBYTE *)prompt : (UBYTE *)"";
	ng.ng_TextAttr = &m.attr;
	ng.ng_GadgetID = GID_EDIT;
	ng.ng_Flags = PLACETEXT_LEFT;
	ng.ng_VisualInfo = vi;
	ng.ng_UserData = NULL;

	if (asInt) {
		if (outNum)
			num = *outNum;
		else if (defStr)
			StrToLong(defStr, &num);
		editGad = CreateGadget(INTEGER_KIND, gad, &ng,
			GTIN_Number, num,
			GTIN_MaxChars, 12,
			TAG_DONE);
	} else {
		editGad = CreateGadget(STRING_KIND, gad, &ng,
			GTST_String, defStr ? defStr : (STRPTR)"",
			GTST_MaxChars, (outLen > 1) ? (outLen - 1) : 1,
			TAG_DONE);
	}
	if (!editGad)
		goto fail;
	gad = editGad;

	y = (WORD)(y + m.rowH + m.gap * 2);
	ng.ng_TopEdge = y;
	ng.ng_Width = btnW;
	ng.ng_LeftEdge = (WORD)(winW - scr->WBorRight - m.margin - btnW * 2 - m.gap);
	ng.ng_GadgetText = (UBYTE *)"OK";
	ng.ng_GadgetID = GID_OK;
	ng.ng_Flags = PLACETEXT_IN;
	gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_DONE);
	if (!gad)
		goto fail;

	ng.ng_LeftEdge = (WORD)(ng.ng_LeftEdge + btnW + m.gap);
	ng.ng_GadgetText = (UBYTE *)"Cancel";
	ng.ng_GadgetID = GID_CANCEL;
	gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_DONE);
	if (!gad)
		goto fail;

	win = OpenWindowTags(NULL,
		WA_Title, title ? title : (STRPTR)"TTX",
		WA_InnerWidth, winW - scr->WBorLeft - scr->WBorRight,
		WA_InnerHeight, (WORD)(y + m.rowH + m.margin - top),
		WA_AutoAdjust, TRUE,
		WA_CloseGadget, TRUE,
		WA_DragBar, TRUE,
		WA_DepthGadget, TRUE,
		WA_Activate, TRUE,
		WA_Gadgets, glist,
		WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
			IDCMP_GADGETUP | IDCMP_VANILLAKEY,
		WA_PubScreen, scr,
		TAG_DONE);
	if (!win)
		goto fail;

	GT_RefreshWindow(win, NULL);
	ActivateGadget(editGad, win, NULL);

	while (!done) {
		WaitPort(win->UserPort);
		while ((imsg = GT_GetIMsg(win->UserPort)) != NULL) {
			class = imsg->Class;
			code = imsg->Code;
			gad = (struct Gadget *)imsg->IAddress;
			GT_ReplyIMsg(imsg);

			if (class == IDCMP_CLOSEWINDOW) {
				done = TRUE;
				ok = FALSE;
			} else if (class == IDCMP_REFRESHWINDOW) {
				GT_BeginRefresh(win);
				GT_EndRefresh(win, TRUE);
			} else if (class == IDCMP_VANILLAKEY) {
				if (code == 27) {
					done = TRUE;
					ok = FALSE;
				} else if (code == 13) {
					done = TRUE;
					ok = TRUE;
				}
			} else if (class == IDCMP_GADGETUP && gad) {
				if (gad->GadgetID == GID_OK ||
				    gad->GadgetID == GID_EDIT) {
					done = TRUE;
					ok = TRUE;
				} else if (gad->GadgetID == GID_CANCEL) {
					done = TRUE;
					ok = FALSE;
				}
			}
		}
	}

	if (ok) {
		if (asInt) {
			num = 0;
			GT_GetGadgetAttrs(editGad, win, NULL,
				GTIN_Number, &num, TAG_DONE);
			if (positiveOnly && num < 0)
				num = -num;
			if (outNum)
				*outNum = num;
			TR_UIFormatLong(outBuf, outLen, num);
		} else {
			s = NULL;
			GT_GetGadgetAttrs(editGad, win, NULL,
				GTST_String, &s, TAG_DONE);
			TR_UICopy(outBuf, outLen, s ? s : (STRPTR)"");
		}
	}

	CloseWindow(win);
	FreeGadgets(glist);
	FreeVisualInfo(vi);
	if (unlock)
		UnlockPubScreen(NULL, scr);
	TR_FreeFontMetrics(&m);
	return ok;

fail:
	if (glist)
		FreeGadgets(glist);
	if (vi)
		FreeVisualInfo(vi);
	if (unlock)
		UnlockPubScreen(NULL, scr);
	TR_FreeFontMetrics(&m);
	return FALSE;
}

BOOL
TR_LVO TR_RequestStr(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, STRPTR title),
	TR_REG(a2, STRPTR defStr),
	TR_REG(a3, STRPTR *outStr))
{
	TEXT buf[256];
	STRPTR copy;
	ULONG n;
	ULONG i;

	(void)lib;
	if (!outStr)
		return FALSE;
	*outStr = NULL;
	buf[0] = '\0';
	if (!TR_UIEditWindow(parent, title, (STRPTR)"", defStr,
		FALSE, FALSE, buf, (ULONG)sizeof(buf), NULL))
		return FALSE;
	n = TR_UILen(buf);
	copy = (STRPTR)TR_Alloc(n + 1UL, MEMF_CLEAR);
	if (!copy)
		return FALSE;
	for (i = 0; i < n; i++)
		copy[i] = buf[i];
	copy[n] = '\0';
	*outStr = copy;
	return TRUE;
}

BOOL
TR_LVO TR_RequestNum(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, STRPTR title),
	TR_REG(d0, LONG defVal),
	TR_REG(d1, BOOL positiveOnly),
	TR_REG(a2, LONG *outNum))
{
	TEXT buf[32];
	LONG val;

	(void)lib;
	if (!outNum)
		return FALSE;
	val = defVal;
	buf[0] = '\0';
	if (!TR_UIEditWindow(parent, title, (STRPTR)"", NULL,
		TRUE, positiveOnly, buf, (ULONG)sizeof(buf), &val))
		return FALSE;
	*outNum = val;
	return TRUE;
}

/****************************************************************************/

STRPTR
TR_LVO TR_RequestFile(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, STRPTR title),
	TR_REG(d0, BOOL saveMode),
	TR_REG(a2, STRPTR initialFile),
	TR_REG(a3, STRPTR initialDrawer))
{
	struct FileRequester *req;
	struct TagItem tags[10];
	ULONG ti;
	STRPTR result;
	STRPTR drawer;
	STRPTR file;
	ULONG dlen;
	ULONG flen;
	ULONG i;

	(void)lib;
	result = NULL;
	ti = 0;
	if (!AslBase)
		return NULL;

	tags[ti].ti_Tag = ASLFR_Window;
	tags[ti].ti_Data = (ULONG)parent;
	ti++;
	tags[ti].ti_Tag = ASLFR_TitleText;
	tags[ti].ti_Data = (ULONG)(title ? title : (STRPTR)"Select File");
	ti++;
	tags[ti].ti_Tag = ASLFR_DoSaveMode;
	tags[ti].ti_Data = saveMode ? TRUE : FALSE;
	ti++;
	tags[ti].ti_Tag = ASLFR_RejectIcons;
	tags[ti].ti_Data = TRUE;
	ti++;
	if (initialFile) {
		tags[ti].ti_Tag = ASLFR_InitialFile;
		tags[ti].ti_Data = (ULONG)initialFile;
		ti++;
	}
	if (initialDrawer) {
		tags[ti].ti_Tag = ASLFR_InitialDrawer;
		tags[ti].ti_Data = (ULONG)initialDrawer;
		ti++;
	}
	tags[ti].ti_Tag = TAG_DONE;
	tags[ti].ti_Data = 0;

	req = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, tags);
	if (!req)
		return NULL;

	if (AslRequest(req, NULL)) {
		drawer = req->fr_Drawer ? req->fr_Drawer : (STRPTR)"";
		file = req->fr_File ? req->fr_File : (STRPTR)"";
		dlen = TR_UILen(drawer);
		flen = TR_UILen(file);
		result = (STRPTR)TR_Alloc(dlen + flen + 4UL, MEMF_CLEAR);
		if (result) {
			for (i = 0; i < dlen; i++)
				result[i] = drawer[i];
			if (dlen > 0 && drawer[dlen - 1] != ':' &&
			    drawer[dlen - 1] != '/') {
				result[dlen] = '/';
				dlen++;
			}
			for (i = 0; i < flen; i++)
				result[dlen + i] = file[i];
			result[dlen + flen] = '\0';
		}
	}
	FreeAslRequest(req);
	return result;
}

/****************************************************************************/

#define TTX_FIND_DEFBTN_PAD  3
#define TTX_FIND_SEP_W       2

static VOID
TR_UIFindDrawDecor(
	struct Window *win,
	APTR vi,
	WORD sepX,
	WORD sepTop,
	WORD sepH,
	WORD defL,
	WORD defT,
	WORD defW,
	WORD defH)
{
	struct TagItem tags[4];
	struct RastPort *rp;
	struct DrawInfo *dri;
	ULONG penShadow;
	ULONG penShine;

	if (!win || !win->RPort || !vi)
		return;

	rp = win->RPort;
	penShadow = 1;
	penShine = 2;
	dri = GetScreenDrawInfo(win->WScreen);
	if (dri && dri->dri_Pens) {
		penShadow = (ULONG)dri->dri_Pens[SHADOWPEN];
		penShine = (ULONG)dri->dri_Pens[SHINEPEN];
	}

	/* Vertical separator: shadow then shine (recessed groove). */
	if (sepH > 2) {
		SetDrMd(rp, JAM1);
		SetAPen(rp, penShadow);
		Move(rp, sepX, sepTop);
		Draw(rp, sepX, (WORD)(sepTop + sepH - 1));
		SetAPen(rp, penShine);
		Move(rp, (WORD)(sepX + 1), sepTop);
		Draw(rp, (WORD)(sepX + 1), (WORD)(sepTop + sepH - 1));
	}

	tags[0].ti_Tag = GT_VisualInfo;
	tags[0].ti_Data = (ULONG)vi;
	tags[1].ti_Tag = GTBB_Recessed;
	tags[1].ti_Data = TRUE;
	tags[2].ti_Tag = GTBB_FrameType;
	tags[2].ti_Data = BBFT_BUTTON;
	tags[3].ti_Tag = TAG_DONE;
	tags[3].ti_Data = 0;

	/*
	 * Default action frame: recessed bevel a few pixels outside Find Next.
	 * Marks the Enter-key default (AmigaUI style).
	 */
	if (defW > 0 && defH > 0) {
		DrawBevelBoxA(rp,
			(WORD)(defL - TTX_FIND_DEFBTN_PAD),
			(WORD)(defT - TTX_FIND_DEFBTN_PAD),
			(WORD)(defW + 2 * TTX_FIND_DEFBTN_PAD),
			(WORD)(defH + 2 * TTX_FIND_DEFBTN_PAD),
			tags);
	}

	if (dri)
		FreeScreenDrawInfo(win->WScreen, dri);
}

static BOOL
TR_UIFindWindow(
	struct Window *parent,
	BOOL withChange,
	struct TRFindOptions *opts,
	STRPTR findBuf,
	STRPTR changeBuf,
	ULONG bufLen,
	LONG *action)
{
	struct TRFontMetrics m;
	struct Screen *scr;
	APTR vi;
	struct Gadget *glist;
	struct Gadget *gad;
	struct Gadget *findGad;
	struct Gadget *changeGad;
	struct Gadget *findNextGad;
	struct Gadget *chk[5];
	struct NewGadget ng;
	struct Window *win;
	struct IntuiMessage *imsg;
	BOOL unlock;
	BOOL done;
	BOOL ok;
	WORD top;
	WORD leftEdge;
	WORD innerW;
	WORD innerH;
	WORD leftW;
	WORD rightW;
	WORD labelW;
	WORD strW;
	WORD btnFindW;
	WORD btnChgW;
	WORD btnAllW;
	WORD btnGap;
	WORD chkW;
	WORD sepX;
	WORD sepTop;
	WORD sepH;
	WORD defL;
	WORD defT;
	WORD defW;
	WORD defH;
	WORD leftX;
	WORD rightX;
	WORD strY;
	WORD btnY;
	WORD chkY;
	WORD y;
	WORD i;
	WORD w;
	ULONG class;
	UWORD code;
	LONG act;
	STRPTR s;
	static STRPTR chkLabels[5] = {
		"Do _Patterns",
		"Ignore _Accents",
		"Ignore _Letter Case",
		"O_nly Whole Words",
		"_Scan Backwards"
	};

	glist = NULL;
	findGad = NULL;
	changeGad = NULL;
	findNextGad = NULL;
	win = NULL;
	vi = NULL;
	scr = NULL;
	unlock = FALSE;
	done = FALSE;
	ok = FALSE;
	act = 0;
	defL = 0;
	defT = 0;
	defW = 0;
	defH = 0;
	sepX = 0;
	sepTop = 0;
	sepH = 0;

	for (i = 0; i < 5; i++)
		chk[i] = NULL;

	if (!opts || !findBuf || bufLen < 2)
		return FALSE;
	if (withChange && !changeBuf)
		return FALSE;

	TR_GetFontMetrics(parent, &m);

	if (parent && parent->WScreen)
		scr = parent->WScreen;
	else {
		scr = LockPubScreen(NULL);
		unlock = TRUE;
	}
	if (!scr) {
		TR_FreeFontMetrics(&m);
		return FALSE;
	}

	vi = GetVisualInfo(scr, TAG_DONE);
	if (!vi) {
		if (unlock)
			UnlockPubScreen(NULL, scr);
		TR_FreeFontMetrics(&m);
		return FALSE;
	}

	labelW = (WORD)TR_TextWidth(&m, "Change");
	w = (WORD)TR_TextWidth(&m, "Find");
	if (w > labelW)
		labelW = w;

	chkW = 0;
	for (i = 0; i < 5; i++) {
		/* Ignore '_' mnemonic markers when measuring. */
		w = (WORD)TR_TextWidth(&m, chkLabels[i]) + (WORD)(m.fontX * 3);
		if (w > chkW)
			chkW = w;
	}
	rightW = chkW;
	if (rightW < (WORD)(m.fontX * 16))
		rightW = (WORD)(m.fontX * 16);

	/* Left column ~60% of content: string field + labels + action buttons. */
	btnFindW = (WORD)TR_TextWidth(&m, "Find Next") + (WORD)(m.fontX * 2);
	if (btnFindW < (WORD)(m.fontX * 10))
		btnFindW = (WORD)(m.fontX * 10);
	btnChgW = (WORD)TR_TextWidth(&m, "Change") + (WORD)(m.fontX * 2);
	if (btnChgW < (WORD)(m.fontX * 8))
		btnChgW = (WORD)(m.fontX * 8);
	btnAllW = (WORD)TR_TextWidth(&m, "Change All") + (WORD)(m.fontX * 2);
	if (btnAllW < (WORD)(m.fontX * 10))
		btnAllW = (WORD)(m.fontX * 10);
	btnGap = m.gap;

	leftW = (WORD)((rightW * 3) / 2);
	w = (WORD)(labelW + m.gap + m.fontX * 28);
	if (leftW < w)
		leftW = w;
	if (withChange) {
		w = (WORD)(btnFindW + btnGap + btnChgW + btnGap + btnAllW +
			2 * TTX_FIND_DEFBTN_PAD);
	} else {
		w = (WORD)(btnFindW + 2 * TTX_FIND_DEFBTN_PAD);
	}
	if (leftW < w)
		leftW = w;

	strW = (WORD)(leftW - labelW - m.gap);
	if (strW < (WORD)(m.fontX * 18))
		strW = (WORD)(m.fontX * 18);

	innerW = (WORD)(m.margin + leftW + m.gap + TTX_FIND_SEP_W + m.gap +
		rightW + m.margin);

	/* Height: right column of 5 checks, or left strings+default button. */
	innerH = (WORD)(m.margin + 5 * (m.rowH + m.gap / 2) + m.margin);
	y = (WORD)(m.margin + (withChange ? 2 : 1) * (m.rowH + m.gap) +
		m.gap + 2 * TTX_FIND_DEFBTN_PAD + m.rowH + m.margin);
	if (innerH < y)
		innerH = y;

	top = (WORD)(scr->WBorTop + scr->Font->ta_YSize + 1);
	leftEdge = (WORD)scr->WBorLeft;

	leftX = (WORD)(leftEdge + m.margin);
	sepX = (WORD)(leftX + leftW + m.gap);
	rightX = (WORD)(sepX + TTX_FIND_SEP_W + m.gap);
	sepTop = (WORD)(top + m.margin / 2);
	sepH = (WORD)(innerH - m.margin);

	/* Strings sit in the upper/middle of the left column. */
	if (withChange)
		strY = (WORD)(top + m.margin + m.rowH);
	else
		strY = (WORD)(top + (innerH - m.rowH) / 3);
	btnY = (WORD)(top + innerH - m.margin - m.rowH - TTX_FIND_DEFBTN_PAD);
	chkY = (WORD)(top + m.margin);

	gad = CreateContext(&glist);
	if (!gad)
		goto fail;

	ng.ng_TextAttr = &m.attr;
	ng.ng_VisualInfo = vi;
	ng.ng_UserData = NULL;

	ng.ng_LeftEdge = (WORD)(leftX + labelW + m.gap);
	ng.ng_TopEdge = strY;
	ng.ng_Width = strW;
	ng.ng_Height = m.rowH;
	ng.ng_GadgetText = (UBYTE *)"_Find";
	ng.ng_GadgetID = GID_FINDSTR;
	ng.ng_Flags = PLACETEXT_LEFT;
	findGad = CreateGadget(STRING_KIND, gad, &ng,
		GTST_String, findBuf,
		GTST_MaxChars, (bufLen > 1) ? (bufLen - 1) : 1,
		GT_Underscore, (ULONG)'_',
		TAG_DONE);
	if (!findGad)
		goto fail;
	gad = findGad;

	if (withChange) {
		ng.ng_TopEdge = (WORD)(strY + m.rowH + m.gap);
		ng.ng_GadgetText = (UBYTE *)"_Change";
		ng.ng_GadgetID = GID_CHGSTR;
		changeGad = CreateGadget(STRING_KIND, gad, &ng,
			GTST_String, changeBuf,
			GTST_MaxChars, (bufLen > 1) ? (bufLen - 1) : 1,
			GT_Underscore, (ULONG)'_',
			TAG_DONE);
		if (!changeGad)
			goto fail;
		gad = changeGad;
	}

	{
		BOOL fl[5];

		fl[0] = opts->doPatterns;
		fl[1] = opts->ignoreAccents;
		fl[2] = opts->ignoreCase;
		fl[3] = opts->wholeWords;
		fl[4] = opts->scanBackwards;

		y = chkY;
		for (i = 0; i < 5; i++) {
			ng.ng_LeftEdge = rightX;
			ng.ng_TopEdge = y;
			ng.ng_Width = rightW;
			ng.ng_Height = m.rowH;
			ng.ng_GadgetText = (UBYTE *)chkLabels[i];
			ng.ng_GadgetID = (UWORD)(GID_CHK0 + i);
			ng.ng_Flags = PLACETEXT_RIGHT;
			chk[i] = CreateGadget(CHECKBOX_KIND, gad, &ng,
				GTCB_Checked, fl[i] ? TRUE : FALSE,
				GT_Underscore, (ULONG)'_',
				TAG_DONE);
			if (!chk[i])
				goto fail;
			gad = chk[i];
			y = (WORD)(y + m.rowH + m.gap / 2);
		}
	}

	/* Find Next — default CTA; recessed frame drawn after open/refresh. */
	defL = (WORD)(leftX + TTX_FIND_DEFBTN_PAD);
	defT = btnY;
	defW = btnFindW;
	defH = m.rowH;

	ng.ng_Flags = PLACETEXT_IN;
	ng.ng_LeftEdge = defL;
	ng.ng_TopEdge = defT;
	ng.ng_Width = defW;
	ng.ng_Height = defH;
	ng.ng_GadgetText = (UBYTE *)"Find _Next";
	ng.ng_GadgetID = GID_FINDNEXT;
	findNextGad = CreateGadget(BUTTON_KIND, gad, &ng,
		GT_Underscore, (ULONG)'_',
		TAG_DONE);
	if (!findNextGad)
		goto fail;
	gad = findNextGad;

	if (withChange) {
		ng.ng_LeftEdge = (WORD)(defL + defW + 2 * TTX_FIND_DEFBTN_PAD +
			btnGap);
		ng.ng_TopEdge = defT;
		ng.ng_Width = btnChgW;
		ng.ng_GadgetText = (UBYTE *)"C_hange";
		ng.ng_GadgetID = GID_CHANGE;
		gad = CreateGadget(BUTTON_KIND, gad, &ng,
			GT_Underscore, (ULONG)'_',
			TAG_DONE);
		if (!gad)
			goto fail;
		ng.ng_LeftEdge = (WORD)(ng.ng_LeftEdge + btnChgW + btnGap);
		ng.ng_Width = btnAllW;
		ng.ng_GadgetText = (UBYTE *)"Change _All";
		ng.ng_GadgetID = GID_CHANGEALL;
		gad = CreateGadget(BUTTON_KIND, gad, &ng,
			GT_Underscore, (ULONG)'_',
			TAG_DONE);
		if (!gad)
			goto fail;
	}

	win = OpenWindowTags(NULL,
		WA_Title, withChange ? (STRPTR)"Find & Change" : (STRPTR)"Find",
		WA_InnerWidth, innerW,
		WA_InnerHeight, innerH,
		WA_AutoAdjust, TRUE,
		WA_CloseGadget, TRUE,
		WA_DragBar, TRUE,
		WA_DepthGadget, TRUE,
		WA_Activate, TRUE,
		WA_Gadgets, glist,
		WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
			IDCMP_GADGETUP | IDCMP_VANILLAKEY,
		WA_PubScreen, scr,
		TAG_DONE);
	if (!win)
		goto fail;

	GT_RefreshWindow(win, NULL);
	TR_UIFindDrawDecor(win, vi, sepX, sepTop, sepH, defL, defT, defW, defH);
	ActivateGadget(findGad, win, NULL);

	while (!done) {
		WaitPort(win->UserPort);
		while ((imsg = GT_GetIMsg(win->UserPort)) != NULL) {
			class = imsg->Class;
			code = imsg->Code;
			gad = (struct Gadget *)imsg->IAddress;
			GT_ReplyIMsg(imsg);

			if (class == IDCMP_CLOSEWINDOW) {
				done = TRUE;
				ok = FALSE;
			} else if (class == IDCMP_REFRESHWINDOW) {
				GT_BeginRefresh(win);
				GT_EndRefresh(win, TRUE);
				TR_UIFindDrawDecor(win, vi, sepX, sepTop, sepH,
					defL, defT, defW, defH);
			} else if (class == IDCMP_VANILLAKEY) {
				if (code == 27) {
					done = TRUE;
					ok = FALSE;
				} else if (code == '\r' || code == '\n') {
					/* Enter activates the default Find Next. */
					act = 0;
					done = TRUE;
					ok = TRUE;
				}
			} else if (class == IDCMP_GADGETUP && gad) {
				if (gad->GadgetID == GID_FINDNEXT ||
				    gad->GadgetID == GID_FINDSTR) {
					act = 0;
					done = TRUE;
					ok = TRUE;
				} else if (gad->GadgetID == GID_CHANGE) {
					act = 1;
					done = TRUE;
					ok = TRUE;
				} else if (gad->GadgetID == GID_CHANGEALL) {
					act = 2;
					done = TRUE;
					ok = TRUE;
				}
			}
		}
	}

	if (ok) {
		s = NULL;
		GT_GetGadgetAttrs(findGad, win, NULL, GTST_String, &s, TAG_DONE);
		TR_UICopy(findBuf, bufLen, s ? s : (STRPTR)"");
		if (withChange && changeGad) {
			s = NULL;
			GT_GetGadgetAttrs(changeGad, win, NULL,
				GTST_String, &s, TAG_DONE);
			TR_UICopy(changeBuf, bufLen, s ? s : (STRPTR)"");
		}
		opts->doPatterns =
			(chk[0]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		opts->ignoreAccents =
			(chk[1]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		opts->ignoreCase =
			(chk[2]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		opts->wholeWords =
			(chk[3]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		opts->scanBackwards =
			(chk[4]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		if (action)
			*action = act;
	}

	CloseWindow(win);
	FreeGadgets(glist);
	FreeVisualInfo(vi);
	if (unlock)
		UnlockPubScreen(NULL, scr);
	TR_FreeFontMetrics(&m);
	return ok;

fail:
	if (glist)
		FreeGadgets(glist);
	if (vi)
		FreeVisualInfo(vi);
	if (unlock)
		UnlockPubScreen(NULL, scr);
	TR_FreeFontMetrics(&m);
	return FALSE;
}

BOOL
TR_LVO TR_RequestFind(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, struct TRFindOptions *opts),
	TR_REG(a2, STRPTR findBuf),
	TR_REG(d0, ULONG bufLen),
	TR_REG(a3, LONG *action))
{
	(void)lib;
	return TR_UIFindWindow(parent, FALSE, opts, findBuf, NULL, bufLen, action);
}

BOOL
TR_LVO TR_RequestFindChange(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, struct TRFindOptions *opts),
	TR_REG(a2, STRPTR findBuf),
	TR_REG(a3, STRPTR changeBuf),
	TR_REG(d0, ULONG bufLen),
	TR_REG(d1, LONG *action))
{
	(void)lib;
	return TR_UIFindWindow(parent, TRUE, opts, findBuf, changeBuf, bufLen,
		action);
}
