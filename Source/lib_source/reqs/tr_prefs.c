/*
 * ttxreqs.library - preferences load/save + Editing/Tabs requester
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 *
 * C89: locals at block start.
 */

#include "private/tr_internal.h"
#include "tr_funcs.h"

#include <dos/dos.h>
#include <libraries/gadtools.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <string.h>
#include <stdio.h>

/****************************************************************************/

#define GID_OK       1
#define GID_CANCEL   2
#define GID_TABW     3
#define GID_MARGIN   4
#define GID_CHK0     20

static struct TRPrefs TR_AppPrefs;
static BOOL TR_AppPrefsInited = FALSE;

/****************************************************************************/

static BOOL
TR_PrefsKeyEq(STRPTR line, STRPTR key)
{
	ULONG i;

	if (!line || !key)
		return FALSE;
	i = 0;
	while (key[i] != '\0') {
		if (line[i] != key[i])
			return FALSE;
		i++;
	}
	return (BOOL)(line[i] == '=');
}

static STRPTR
TR_PrefsVal(STRPTR line)
{
	while (*line && *line != '=')
		line++;
	if (*line == '=')
		line++;
	return line;
}

static BOOL
TR_PrefsIsTrue(STRPTR v)
{
	if (!v)
		return FALSE;
	if (v[0] == '1' || v[0] == 'Y' || v[0] == 'y' || v[0] == 'T' || v[0] == 't')
		return TRUE;
	if (Stricmp(v, "ON") == 0 || Stricmp(v, "TRUE") == 0)
		return TRUE;
	return FALSE;
}

/****************************************************************************/

static VOID
TR_PrefsSetDefaultsI(struct TRPrefs *p)
{
	if (!p)
		return;
	p->autoCorrectWordCase = FALSE;
	p->autoEraseSelectedBlocks = FALSE;
	p->autoIndentNewLines = TRUE;
	p->selectWhenDragging = TRUE;
	p->freeForm = FALSE;
	p->lineWrap = TRUE;
	p->overstrike = FALSE;
	p->wordWrap = FALSE;
	p->rightMargin = 80;
	p->tabWidth = 8;
	p->expandTabs = FALSE;
}

VOID
TR_LVO TR_PrefsSetDefaults(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct TRPrefs *p))
{
	(void)lib;
	TR_PrefsSetDefaultsI(p);
}

struct TRPrefs *
TR_LVO TR_PrefsGet(
	TR_REG(a6, struct Library *lib))
{
	(void)lib;
	if (!TR_AppPrefsInited) {
		TR_PrefsSetDefaultsI(&TR_AppPrefs);
		TR_AppPrefsInited = TRUE;
	}
	return &TR_AppPrefs;
}

VOID
TR_LVO TR_PrefsSet(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct TRPrefs *p))
{
	(void)lib;
	if (!p)
		return;
	TR_AppPrefs = *p;
	TR_AppPrefsInited = TRUE;
}

BOOL
TR_LVO TR_PrefsLoad(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct TRPrefs *p),
	TR_REG(a1, STRPTR path))
{
	BPTR fh;
	TEXT line[256];
	STRPTR v;
	LONG n;

	(void)lib;
	if (!p || !path)
		return FALSE;
	TR_PrefsSetDefaultsI(p);

	fh = Open(path, MODE_OLDFILE);
	if (!fh)
		return FALSE;

	while (FGets(fh, line, (LONG)sizeof(line))) {
		if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
			continue;
		v = TR_PrefsVal(line);
		if (TR_PrefsKeyEq(line, "AutoCorrectWordCase"))
			p->autoCorrectWordCase = TR_PrefsIsTrue(v);
		else if (TR_PrefsKeyEq(line, "AutoEraseSelectedBlocks"))
			p->autoEraseSelectedBlocks = TR_PrefsIsTrue(v);
		else if (TR_PrefsKeyEq(line, "AutoIndentNewLines"))
			p->autoIndentNewLines = TR_PrefsIsTrue(v);
		else if (TR_PrefsKeyEq(line, "SelectWhenDragging"))
			p->selectWhenDragging = TR_PrefsIsTrue(v);
		else if (TR_PrefsKeyEq(line, "FreeForm"))
			p->freeForm = TR_PrefsIsTrue(v);
		else if (TR_PrefsKeyEq(line, "LineWrap"))
			p->lineWrap = TR_PrefsIsTrue(v);
		else if (TR_PrefsKeyEq(line, "Overstrike"))
			p->overstrike = TR_PrefsIsTrue(v);
		else if (TR_PrefsKeyEq(line, "WordWrap"))
			p->wordWrap = TR_PrefsIsTrue(v);
		else if (TR_PrefsKeyEq(line, "ExpandTabs"))
			p->expandTabs = TR_PrefsIsTrue(v);
		else if (TR_PrefsKeyEq(line, "RightMargin")) {
			n = 80;
			StrToLong(v, &n);
			if (n > 0)
				p->rightMargin = (ULONG)n;
		} else if (TR_PrefsKeyEq(line, "TabWidth")) {
			n = 8;
			StrToLong(v, &n);
			if (n > 0)
				p->tabWidth = (ULONG)n;
		}
	}
	Close(fh);
	return TRUE;
}

BOOL
TR_LVO TR_PrefsSave(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct TRPrefs *p),
	TR_REG(a1, STRPTR path))
{
	BPTR fh;

	(void)lib;
	if (!p || !path)
		return FALSE;
	fh = Open(path, MODE_NEWFILE);
	if (!fh)
		return FALSE;

	FPrintf(fh, "# TTX preferences (Latin-1)\n");
	FPrintf(fh, "AutoCorrectWordCase=%s\n",
		p->autoCorrectWordCase ? "YES" : "NO");
	FPrintf(fh, "AutoEraseSelectedBlocks=%s\n",
		p->autoEraseSelectedBlocks ? "YES" : "NO");
	FPrintf(fh, "AutoIndentNewLines=%s\n",
		p->autoIndentNewLines ? "YES" : "NO");
	FPrintf(fh, "SelectWhenDragging=%s\n",
		p->selectWhenDragging ? "YES" : "NO");
	FPrintf(fh, "FreeForm=%s\n", p->freeForm ? "YES" : "NO");
	FPrintf(fh, "LineWrap=%s\n", p->lineWrap ? "YES" : "NO");
	FPrintf(fh, "Overstrike=%s\n", p->overstrike ? "YES" : "NO");
	FPrintf(fh, "WordWrap=%s\n", p->wordWrap ? "YES" : "NO");
	FPrintf(fh, "RightMargin=%lu\n", p->rightMargin);
	FPrintf(fh, "TabWidth=%lu\n", p->tabWidth);
	FPrintf(fh, "ExpandTabs=%s\n", p->expandTabs ? "YES" : "NO");
	Close(fh);
	return TRUE;
}

/****************************************************************************/

BOOL
TR_LVO TR_PrefsRequester(
	TR_REG(a6, struct Library *lib),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, struct TRPrefs *p))
{
	struct TRFontMetrics m;
	struct Screen *scr;
	APTR vi;
	struct Gadget *glist;
	struct Gadget *gad;
	struct Gadget *chk[9];
	struct Gadget *tabGad;
	struct Gadget *marginGad;
	struct NewGadget ng;
	struct Window *win;
	struct IntuiMessage *imsg;
	BOOL unlock;
	BOOL done;
	BOOL ok;
	WORD top;
	WORD winW;
	WORD chkW;
	WORD btnW;
	WORD y;
	WORD x;
	WORD i;
	WORD w;
	ULONG class;
	UWORD code;
	LONG num;
	static STRPTR labels[9] = {
		"Auto-Correct Word Case",
		"Auto-Erase Selected Blocks",
		"Auto-Indent New Lines",
		"Select When Dragging",
		"Free Form",
		"Line Wrap",
		"Overstrike",
		"Word Wrap",
		"Expand Tabs"
	};

	glist = NULL;
	tabGad = NULL;
	marginGad = NULL;
	win = NULL;
	vi = NULL;
	scr = NULL;
	unlock = FALSE;
	done = FALSE;
	ok = FALSE;

	(void)lib;
	for (i = 0; i < 9; i++)
		chk[i] = NULL;

	if (!p)
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

	chkW = 0;
	for (i = 0; i < 9; i++) {
		w = (WORD)TR_TextWidth(&m, labels[i]) + (WORD)(m.fontX * 3);
		if (w > chkW)
			chkW = w;
	}
	btnW = (WORD)TR_TextWidth(&m, "Cancel") + (WORD)(m.fontX * 2);
	if (btnW < (WORD)(m.fontX * 8))
		btnW = (WORD)(m.fontX * 8);

	winW = (WORD)(m.margin * 2 + chkW);
	w = (WORD)(m.margin * 2 + btnW * 2 + m.gap);
	if (winW < w)
		winW = w;
	w = (WORD)(m.margin * 2 + TR_TextWidth(&m, "Right Margin") +
		m.gap + m.fontX * 8);
	if (winW < w)
		winW = w;

	top = (WORD)(scr->WBorTop + scr->Font->ta_YSize + 1);

	gad = CreateContext(&glist);
	if (!gad)
		goto fail;

	y = (WORD)(top + m.margin);
	x = m.margin;
	ng.ng_TextAttr = &m.attr;
	ng.ng_VisualInfo = vi;
	ng.ng_UserData = NULL;

	{
		BOOL fl[9];

		fl[0] = p->autoCorrectWordCase;
		fl[1] = p->autoEraseSelectedBlocks;
		fl[2] = p->autoIndentNewLines;
		fl[3] = p->selectWhenDragging;
		fl[4] = p->freeForm;
		fl[5] = p->lineWrap;
		fl[6] = p->overstrike;
		fl[7] = p->wordWrap;
		fl[8] = p->expandTabs;

		for (i = 0; i < 9; i++) {
			ng.ng_LeftEdge = x;
			ng.ng_TopEdge = y;
			ng.ng_Width = chkW;
			ng.ng_Height = m.rowH;
			ng.ng_GadgetText = (UBYTE *)labels[i];
			ng.ng_GadgetID = (UWORD)(GID_CHK0 + i);
			ng.ng_Flags = PLACETEXT_RIGHT;
			chk[i] = CreateGadget(CHECKBOX_KIND, gad, &ng,
				GTCB_Checked, fl[i] ? TRUE : FALSE,
				TAG_DONE);
			if (!chk[i])
				goto fail;
			gad = chk[i];
			y = (WORD)(y + m.rowH + m.gap / 2);
		}
	}

	y = (WORD)(y + m.gap);
	ng.ng_LeftEdge = (WORD)(x + TR_TextWidth(&m, "Tab Width") + m.gap);
	ng.ng_TopEdge = y;
	ng.ng_Width = (WORD)(m.fontX * 8);
	ng.ng_Height = m.rowH;
	ng.ng_GadgetText = (UBYTE *)"Tab Width";
	ng.ng_GadgetID = GID_TABW;
	ng.ng_Flags = PLACETEXT_LEFT;
	tabGad = CreateGadget(INTEGER_KIND, gad, &ng,
		GTIN_Number, (LONG)p->tabWidth,
		GTIN_MaxChars, 4,
		TAG_DONE);
	if (!tabGad)
		goto fail;
	gad = tabGad;
	y = (WORD)(y + m.rowH + m.gap);

	ng.ng_LeftEdge = (WORD)(x + TR_TextWidth(&m, "Right Margin") + m.gap);
	ng.ng_TopEdge = y;
	ng.ng_GadgetText = (UBYTE *)"Right Margin";
	ng.ng_GadgetID = GID_MARGIN;
	marginGad = CreateGadget(INTEGER_KIND, gad, &ng,
		GTIN_Number, (LONG)p->rightMargin,
		GTIN_MaxChars, 4,
		TAG_DONE);
	if (!marginGad)
		goto fail;
	gad = marginGad;
	y = (WORD)(y + m.rowH + m.gap * 2);

	ng.ng_Flags = PLACETEXT_IN;
	ng.ng_TopEdge = y;
	ng.ng_Width = btnW;
	ng.ng_Height = m.rowH;
	ng.ng_LeftEdge = x;
	ng.ng_GadgetText = (UBYTE *)"OK";
	ng.ng_GadgetID = GID_OK;
	gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_DONE);
	if (!gad)
		goto fail;

	ng.ng_LeftEdge = (WORD)(x + btnW + m.gap);
	ng.ng_GadgetText = (UBYTE *)"Cancel";
	ng.ng_GadgetID = GID_CANCEL;
	gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_DONE);
	if (!gad)
		goto fail;

	win = OpenWindowTags(NULL,
		WA_Title, (STRPTR)"Preferences - Editing / Tabs",
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
			} else if (class == IDCMP_VANILLAKEY && code == 27) {
				done = TRUE;
				ok = FALSE;
			} else if (class == IDCMP_GADGETUP && gad) {
				if (gad->GadgetID == GID_OK) {
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
		p->autoCorrectWordCase =
			(chk[0]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		p->autoEraseSelectedBlocks =
			(chk[1]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		p->autoIndentNewLines =
			(chk[2]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		p->selectWhenDragging =
			(chk[3]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		p->freeForm =
			(chk[4]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		p->lineWrap =
			(chk[5]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		p->overstrike =
			(chk[6]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		p->wordWrap =
			(chk[7]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		p->expandTabs =
			(chk[8]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		num = 8;
		GT_GetGadgetAttrs(tabGad, win, NULL, GTIN_Number, &num, TAG_DONE);
		if (num > 0)
			p->tabWidth = (ULONG)num;
		num = 80;
		GT_GetGadgetAttrs(marginGad, win, NULL, GTIN_Number, &num, TAG_DONE);
		if (num > 0)
			p->rightMargin = (ULONG)num;
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
