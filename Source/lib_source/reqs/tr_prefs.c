/*
 * ttxreqs.library - preferences load/save + TurboText Preferences requester
 *
 * Layout matches original TurboText Preferences: left MX category list (~1/3),
 * vertical recessed separator, right settings in a titled recessed frame,
 * horizontal separator, Use (default framed) + Cancel.
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
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/exec.h>
#include <proto/utility.h>

/****************************************************************************/

#define GID_USE       1
#define GID_CANCEL    2
#define GID_MX        3
#define GID_PAGE0    100

#define PREF_CAT_AUTOSAVE  0
#define PREF_CAT_BACKUP    1
#define PREF_CAT_COLORS    2
#define PREF_CAT_EDITING   3
#define PREF_CAT_EOL       4
#define PREF_CAT_FOLDS     5
#define PREF_CAT_FONTS     6
#define PREF_CAT_ICONS     7
#define PREF_CAT_MISC      8
#define PREF_CAT_MARKS     9
#define PREF_CAT_TABS      10
#define PREF_CAT_WINDOW    11
#define PREF_CAT_COUNT     12

#define TTX_PREF_DEFBTN_PAD  3
#define TTX_PREF_SEP_W       2
#define TTX_PREF_FRAME_PAD   8
/* Fixed GadTools MX imagery size (do not scale to column width). */
#define TTX_PREF_MX_W        17
#define TTX_PREF_MX_H        9

static struct TRPrefs TR_AppPrefs;
static BOOL TR_AppPrefsInited = FALSE;

static STRPTR TR_PrefsCatLabels[PREF_CAT_COUNT + 1] = {
	(STRPTR)"Automatic Saving",
	(STRPTR)"Backup Creation",
	(STRPTR)"Colors",
	(STRPTR)"Editing",
	(STRPTR)"End Of Lines",
	(STRPTR)"Folds",
	(STRPTR)"Fonts",
	(STRPTR)"Icons",
	(STRPTR)"Miscellaneous",
	(STRPTR)"Special Marks",
	(STRPTR)"Tabs",
	(STRPTR)"Window And Screen",
	NULL
};

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
	p->autoSaveDocuments = FALSE;
	p->askBeforeAutoSave = TRUE;
	p->autoSaveMinutes = 15;
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
		else if (TR_PrefsKeyEq(line, "AutoSaveDocuments"))
			p->autoSaveDocuments = TR_PrefsIsTrue(v);
		else if (TR_PrefsKeyEq(line, "AskBeforeAutoSave"))
			p->askBeforeAutoSave = TR_PrefsIsTrue(v);
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
		} else if (TR_PrefsKeyEq(line, "AutoSaveMinutes")) {
			n = 15;
			StrToLong(v, &n);
			if (n > 0)
				p->autoSaveMinutes = (ULONG)n;
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
	FPrintf(fh, "AutoSaveDocuments=%s\n",
		p->autoSaveDocuments ? "YES" : "NO");
	FPrintf(fh, "AskBeforeAutoSave=%s\n",
		p->askBeforeAutoSave ? "YES" : "NO");
	FPrintf(fh, "AutoSaveMinutes=%lu\n", p->autoSaveMinutes);
	Close(fh);
	return TRUE;
}

/****************************************************************************/

static VOID
TR_PrefsDrawSepV(struct RastPort *rp, ULONG penShadow, ULONG penShine,
	WORD x, WORD topY, WORD height)
{
	if (!rp || height < 2)
		return;
	SetDrMd(rp, JAM1);
	SetAPen(rp, penShadow);
	Move(rp, x, topY);
	Draw(rp, x, (WORD)(topY + height - 1));
	SetAPen(rp, penShine);
	Move(rp, (WORD)(x + 1), topY);
	Draw(rp, (WORD)(x + 1), (WORD)(topY + height - 1));
}

static VOID
TR_PrefsDrawSepH(struct RastPort *rp, ULONG penShadow, ULONG penShine,
	WORD left, WORD y, WORD width)
{
	if (!rp || width < 2)
		return;
	SetDrMd(rp, JAM1);
	SetAPen(rp, penShadow);
	Move(rp, left, y);
	Draw(rp, (WORD)(left + width - 1), y);
	SetAPen(rp, penShine);
	Move(rp, left, (WORD)(y + 1));
	Draw(rp, (WORD)(left + width - 1), (WORD)(y + 1));
}

static VOID
TR_PrefsDrawDecor(
	struct Window *win,
	APTR vi,
	struct TRFontMetrics *m,
	WORD sepVX,
	WORD sepVT,
	WORD sepVH,
	WORD sepHY,
	WORD sepHL,
	WORD sepHW,
	WORD frameL,
	WORD frameT,
	WORD frameW,
	WORD frameH,
	STRPTR frameTitle,
	WORD useL,
	WORD useT,
	WORD useW,
	WORD useH)
{
	struct TagItem tags[4];
	struct RastPort *rp;
	struct DrawInfo *dri;
	ULONG penText;
	ULONG penBack;
	ULONG penShadow;
	ULONG penShine;
	LONG tw;
	LONG tx;
	LONG baseline;
	ULONG len;

	if (!win || !win->RPort || !vi || !m)
		return;

	rp = win->RPort;
	penText = 1;
	penBack = 0;
	penShadow = 1;
	penShine = 2;
	dri = GetScreenDrawInfo(win->WScreen);
	if (dri && dri->dri_Pens) {
		penBack = (ULONG)dri->dri_Pens[BACKGROUNDPEN];
		penText = (ULONG)dri->dri_Pens[TEXTPEN];
		penShadow = (ULONG)dri->dri_Pens[SHADOWPEN];
		penShine = (ULONG)dri->dri_Pens[SHINEPEN];
	}
	if (m->font)
		SetFont(rp, m->font);

	/* Separators: recessed look = shadow line then shine line. */
	TR_PrefsDrawSepV(rp, penShadow, penShine, sepVX, sepVT, sepVH);
	TR_PrefsDrawSepH(rp, penShadow, penShine, sepHL, sepHY, sepHW);

	tags[0].ti_Tag = GT_VisualInfo;
	tags[0].ti_Data = (ULONG)vi;
	tags[1].ti_Tag = GTBB_Recessed;
	tags[1].ti_Data = TRUE;
	tags[2].ti_Tag = GTBB_FrameType;
	tags[2].ti_Data = BBFT_BUTTON;
	tags[3].ti_Tag = TAG_DONE;
	tags[3].ti_Data = 0;

	if (frameW > 8 && frameH > 8)
		DrawBevelBoxA(rp, frameL, frameT, frameW, frameH, tags);

	/* Title punched into the top bevel of the settings frame. */
	if (frameTitle && m->font) {
		len = 0;
		while (frameTitle[len] != '\0')
			len++;
		tw = TextLength(rp, frameTitle, len);
		tx = frameL + (frameW - tw) / 2;
		if (tx < frameL + 4)
			tx = frameL + 4;
		baseline = m->font->tf_Baseline;
		SetAPen(rp, penBack);
		SetDrMd(rp, JAM1);
		RectFill(rp, tx - 4, frameT, tx + tw + 3,
			frameT + m->fontY);
		SetAPen(rp, penText);
		Move(rp, tx, frameT + baseline);
		Text(rp, frameTitle, len);
	}

	/* Default Use button outer recessed frame. */
	if (useW > 0 && useH > 0) {
		DrawBevelBoxA(rp,
			(WORD)(useL - TTX_PREF_DEFBTN_PAD),
			(WORD)(useT - TTX_PREF_DEFBTN_PAD),
			(WORD)(useW + 2 * TTX_PREF_DEFBTN_PAD),
			(WORD)(useH + 2 * TTX_PREF_DEFBTN_PAD),
			tags);
	}

	if (dri)
		FreeScreenDrawInfo(win->WScreen, dri);
}

static VOID
TR_PrefsClearPane(
	struct Window *win,
	struct TRFontMetrics *m,
	WORD frameL,
	WORD frameT,
	WORD frameW,
	WORD frameH)
{
	struct RastPort *rp;
	struct DrawInfo *dri;
	ULONG penBack;

	if (!win || !win->RPort)
		return;
	rp = win->RPort;
	penBack = 0;
	dri = GetScreenDrawInfo(win->WScreen);
	if (dri && dri->dri_Pens)
		penBack = (ULONG)dri->dri_Pens[BACKGROUNDPEN];
	SetAPen(rp, penBack);
	SetDrMd(rp, JAM1);
	RectFill(rp, frameL + 2, frameT + (m ? m->fontY : 8) + 2,
		frameL + frameW - 3, frameT + frameH - 3);
	if (dri)
		FreeScreenDrawInfo(win->WScreen, dri);
}

static VOID
TR_PrefsHarvestPage(
	struct Window *win,
	WORD cat,
	struct TRPrefs *edit,
	struct Gadget **gads,
	WORD gadCount)
{
	LONG num;
	WORD i;

	if (!edit || !gads)
		return;

	if (cat == PREF_CAT_AUTOSAVE && gadCount >= 3) {
		edit->autoSaveDocuments =
			(gads[0]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		edit->askBeforeAutoSave =
			(gads[1]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		num = (LONG)edit->autoSaveMinutes;
		GT_GetGadgetAttrs(gads[2], win, NULL, GTSL_Level, &num, TAG_DONE);
		if (num < 1)
			num = 1;
		edit->autoSaveMinutes = (ULONG)num;
	} else if (cat == PREF_CAT_EDITING && gadCount >= 9) {
		edit->autoCorrectWordCase =
			(gads[0]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		edit->autoEraseSelectedBlocks =
			(gads[1]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		edit->autoIndentNewLines =
			(gads[2]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		edit->selectWhenDragging =
			(gads[3]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		edit->freeForm =
			(gads[4]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		edit->lineWrap =
			(gads[5]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		edit->overstrike =
			(gads[6]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		edit->wordWrap =
			(gads[7]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		num = (LONG)edit->rightMargin;
		GT_GetGadgetAttrs(gads[8], win, NULL, GTIN_Number, &num, TAG_DONE);
		if (num > 0)
			edit->rightMargin = (ULONG)num;
	} else if (cat == PREF_CAT_TABS && gadCount >= 2) {
		edit->expandTabs =
			(gads[0]->Flags & GFLG_SELECTED) ? TRUE : FALSE;
		num = (LONG)edit->tabWidth;
		GT_GetGadgetAttrs(gads[1], win, NULL, GTIN_Number, &num, TAG_DONE);
		if (num > 0)
			edit->tabWidth = (ULONG)num;
	}

	(void)i;
}

static BOOL
TR_PrefsBuildPage(
	struct Window *win,
	APTR vi,
	struct TRFontMetrics *m,
	WORD cat,
	struct TRPrefs *edit,
	WORD paneL,
	WORD paneT,
	WORD paneW,
	struct Gadget **outGlist,
	struct Gadget **outGads,
	WORD *outCount)
{
	struct Gadget *glist;
	struct Gadget *gad;
	struct NewGadget ng;
	WORD y;
	WORD labelW;
	WORD row;
	WORD i;
	WORD w;
	static STRPTR editLabels[8] = {
		(STRPTR)"Auto-Correct Word Case",
		(STRPTR)"Auto-Erase Selected Blocks",
		(STRPTR)"Auto-Indent New Lines",
		(STRPTR)"Select When Dragging",
		(STRPTR)"Free Form",
		(STRPTR)"Line Wrap",
		(STRPTR)"Overstrike",
		(STRPTR)"Word Wrap"
	};
	BOOL fl[8];

	if (!outGlist || !outGads || !outCount || !m || !edit)
		return FALSE;

	*outGlist = NULL;
	*outCount = 0;
	for (i = 0; i < 16; i++)
		outGads[i] = NULL;

	gad = CreateContext(&glist);
	if (!gad)
		return FALSE;

	ng.ng_TextAttr = &m->attr;
	ng.ng_VisualInfo = vi;
	ng.ng_UserData = NULL;
	y = paneT;

	if (cat == PREF_CAT_AUTOSAVE) {
		labelW = (WORD)TR_TextWidth(m, "Minutes Between Auto-Saves:");
		w = (WORD)TR_TextWidth(m, "Ask Before Auto-Saving");
		if (w > labelW)
			labelW = w;
		w = (WORD)TR_TextWidth(m, "Auto-Save Documents");
		if (w > labelW)
			labelW = w;

		ng.ng_Flags = PLACETEXT_LEFT;
		ng.ng_LeftEdge = (WORD)(paneL + labelW + m->gap);
		ng.ng_TopEdge = y;
		ng.ng_Width = 26;
		ng.ng_Height = m->rowH;
		ng.ng_GadgetText = (UBYTE *)"Auto-Save Documents";
		ng.ng_GadgetID = GID_PAGE0;
		outGads[0] = CreateGadget(CHECKBOX_KIND, gad, &ng,
			GTCB_Checked, edit->autoSaveDocuments ? TRUE : FALSE,
			TAG_DONE);
		if (!outGads[0])
			goto fail;
		gad = outGads[0];
		y = (WORD)(y + m->rowH + m->gap);

		ng.ng_TopEdge = y;
		ng.ng_GadgetText = (UBYTE *)"Ask Before Auto-Saving";
		ng.ng_GadgetID = GID_PAGE0 + 1;
		outGads[1] = CreateGadget(CHECKBOX_KIND, gad, &ng,
			GTCB_Checked, edit->askBeforeAutoSave ? TRUE : FALSE,
			TAG_DONE);
		if (!outGads[1])
			goto fail;
		gad = outGads[1];
		y = (WORD)(y + m->rowH + m->gap);

		ng.ng_LeftEdge = (WORD)(paneL + labelW + m->gap);
		ng.ng_TopEdge = y;
		ng.ng_Width = (WORD)(paneW - labelW - m->gap);
		if (ng.ng_Width < 80)
			ng.ng_Width = 80;
		ng.ng_Height = m->rowH;
		ng.ng_GadgetText = (UBYTE *)"Minutes Between Auto-Saves:";
		ng.ng_GadgetID = GID_PAGE0 + 2;
		outGads[2] = CreateGadget(SLIDER_KIND, gad, &ng,
			GTSL_Min, 1,
			GTSL_Max, 60,
			GTSL_Level, (LONG)edit->autoSaveMinutes,
			GTSL_LevelFormat, (STRPTR)"%ld",
			GTSL_MaxLevelLen, 3,
			GTSL_LevelPlace, PLACETEXT_LEFT,
			GA_RelVerify, TRUE,
			TAG_DONE);
		if (!outGads[2])
			goto fail;
		*outCount = 3;
	} else if (cat == PREF_CAT_EDITING) {
		fl[0] = edit->autoCorrectWordCase;
		fl[1] = edit->autoEraseSelectedBlocks;
		fl[2] = edit->autoIndentNewLines;
		fl[3] = edit->selectWhenDragging;
		fl[4] = edit->freeForm;
		fl[5] = edit->lineWrap;
		fl[6] = edit->overstrike;
		fl[7] = edit->wordWrap;

		labelW = 0;
		for (i = 0; i < 8; i++) {
			w = (WORD)TR_TextWidth(m, editLabels[i]);
			if (w > labelW)
				labelW = w;
		}
		w = (WORD)TR_TextWidth(m, "Right Margin");
		if (w > labelW)
			labelW = w;

		ng.ng_Flags = PLACETEXT_LEFT;
		for (row = 0; row < 8; row++) {
			ng.ng_LeftEdge = (WORD)(paneL + labelW + m->gap);
			ng.ng_TopEdge = y;
			ng.ng_Width = 26;
			ng.ng_Height = m->rowH;
			ng.ng_GadgetText = (UBYTE *)editLabels[row];
			ng.ng_GadgetID = (UWORD)(GID_PAGE0 + row);
			outGads[row] = CreateGadget(CHECKBOX_KIND, gad, &ng,
				GTCB_Checked, fl[row] ? TRUE : FALSE,
				TAG_DONE);
			if (!outGads[row])
				goto fail;
			gad = outGads[row];
			y = (WORD)(y + m->rowH + m->gap / 2);
		}

		ng.ng_LeftEdge = (WORD)(paneL + labelW + m->gap);
		ng.ng_TopEdge = y;
		ng.ng_Width = (WORD)(m->fontX * 8);
		ng.ng_Height = m->rowH;
		ng.ng_GadgetText = (UBYTE *)"Right Margin";
		ng.ng_GadgetID = GID_PAGE0 + 8;
		outGads[8] = CreateGadget(INTEGER_KIND, gad, &ng,
			GTIN_Number, (LONG)edit->rightMargin,
			GTIN_MaxChars, 4,
			TAG_DONE);
		if (!outGads[8])
			goto fail;
		*outCount = 9;
	} else if (cat == PREF_CAT_TABS) {
		labelW = (WORD)TR_TextWidth(m, "Expand Tabs");
		w = (WORD)TR_TextWidth(m, "Tab Width");
		if (w > labelW)
			labelW = w;

		ng.ng_Flags = PLACETEXT_LEFT;
		ng.ng_LeftEdge = (WORD)(paneL + labelW + m->gap);
		ng.ng_TopEdge = y;
		ng.ng_Width = 26;
		ng.ng_Height = m->rowH;
		ng.ng_GadgetText = (UBYTE *)"Expand Tabs";
		ng.ng_GadgetID = GID_PAGE0;
		outGads[0] = CreateGadget(CHECKBOX_KIND, gad, &ng,
			GTCB_Checked, edit->expandTabs ? TRUE : FALSE,
			TAG_DONE);
		if (!outGads[0])
			goto fail;
		gad = outGads[0];
		y = (WORD)(y + m->rowH + m->gap);

		ng.ng_TopEdge = y;
		ng.ng_Width = (WORD)(m->fontX * 8);
		ng.ng_GadgetText = (UBYTE *)"Tab Width";
		ng.ng_GadgetID = GID_PAGE0 + 1;
		outGads[1] = CreateGadget(INTEGER_KIND, gad, &ng,
			GTIN_Number, (LONG)edit->tabWidth,
			GTIN_MaxChars, 4,
			TAG_DONE);
		if (!outGads[1])
			goto fail;
		*outCount = 2;
	} else {
		/* Categories not yet wired — empty framed pane. */
		*outCount = 0;
	}

	*outGlist = glist;
	if (win && glist && *outCount > 0) {
		AddGList(win, glist, (UWORD)-1, (WORD)-1, NULL);
		RefreshGList(glist, win, NULL, (WORD)-1);
		GT_RefreshWindow(win, NULL);
	}
	return TRUE;

fail:
	if (glist)
		FreeGadgets(glist);
	*outGlist = NULL;
	*outCount = 0;
	return FALSE;
}

static VOID
TR_PrefsFreePage(struct Window *win, struct Gadget **glist)
{
	if (!glist || !*glist)
		return;
	if (win) {
		RemoveGList(win, *glist, (LONG)-1);
	}
	FreeGadgets(*glist);
	*glist = NULL;
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
	struct Gadget *mainGlist;
	struct Gadget *gad;
	struct Gadget *mxGad;
	struct Gadget *useGad;
	struct Gadget *detailGlist;
	struct Gadget *detailGads[16];
	struct NewGadget ng;
	struct Window *win;
	struct IntuiMessage *imsg;
	struct TRPrefs edit;
	BOOL unlock;
	BOOL done;
	BOOL ok;
	WORD top;
	WORD leftEdge;
	WORD innerW;
	WORD innerH;
	WORD mxW;
	WORD frameL;
	WORD frameT;
	WORD frameW;
	WORD frameH;
	WORD paneL;
	WORD paneT;
	WORD paneW;
	WORD sepVX;
	WORD sepVT;
	WORD sepVH;
	WORD sepHY;
	WORD sepHL;
	WORD sepHW;
	WORD useL;
	WORD useT;
	WORD useW;
	WORD useH;
	WORD cancelL;
	WORD btnW;
	WORD btnY;
	WORD contentBottom;
	WORD cat;
	WORD detailCount;
	WORD i;
	WORD w;
	ULONG class;
	UWORD code;
	ULONG mxActive;

	(void)lib;
	mainGlist = NULL;
	detailGlist = NULL;
	mxGad = NULL;
	useGad = NULL;
	win = NULL;
	vi = NULL;
	scr = NULL;
	unlock = FALSE;
	done = FALSE;
	ok = FALSE;
	cat = PREF_CAT_AUTOSAVE;
	detailCount = 0;

	for (i = 0; i < 16; i++)
		detailGads[i] = NULL;

	if (!p)
		return FALSE;
	edit = *p;

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

	mxW = 0;
	for (i = 0; i < PREF_CAT_COUNT; i++) {
		w = (WORD)(TTX_PREF_MX_W + m.gap +
			TR_TextWidth(&m, TR_PrefsCatLabels[i]));
		if (w > mxW)
			mxW = w;
	}
	if (mxW < (WORD)(TTX_PREF_MX_W + m.fontX * 14))
		mxW = (WORD)(TTX_PREF_MX_W + m.fontX * 14);

	frameW = (WORD)(m.fontX * 42);
	if (frameW < 280)
		frameW = 280;

	innerW = (WORD)(m.margin + mxW + m.gap + TTX_PREF_SEP_W + m.gap +
		frameW + m.margin);

	/* Height from 12 MX rows + bottom Use/Cancel strip. */
	innerH = (WORD)(m.margin +
		PREF_CAT_COUNT * (TTX_PREF_MX_H + 1) +
		m.gap + TTX_PREF_SEP_W + m.gap +
		2 * TTX_PREF_DEFBTN_PAD + m.rowH + m.margin);

	top = (WORD)(scr->WBorTop + scr->Font->ta_YSize + 1);
	leftEdge = (WORD)scr->WBorLeft;

	sepVX = (WORD)(leftEdge + m.margin + mxW + m.gap);
	sepVT = (WORD)(top + m.margin / 2);
	contentBottom = (WORD)(top + innerH - m.margin -
		2 * TTX_PREF_DEFBTN_PAD - m.rowH - m.gap);
	sepHY = contentBottom;
	sepHL = (WORD)(leftEdge + m.margin);
	sepHW = (WORD)(innerW - 2 * m.margin);
	sepVH = (WORD)(sepHY - sepVT);

	frameL = (WORD)(sepVX + TTX_PREF_SEP_W + m.gap);
	frameT = (WORD)(top + m.margin);
	frameH = (WORD)(sepHY - frameT - m.gap);
	paneL = (WORD)(frameL + TTX_PREF_FRAME_PAD);
	paneT = (WORD)(frameT + m.fontY + TTX_PREF_FRAME_PAD);
	paneW = (WORD)(frameW - 2 * TTX_PREF_FRAME_PAD);

	btnW = (WORD)TR_TextWidth(&m, "Cancel") + (WORD)(m.fontX * 3);
	if (btnW < (WORD)(m.fontX * 10))
		btnW = (WORD)(m.fontX * 10);
	btnY = (WORD)(sepHY + TTX_PREF_SEP_W + m.gap + TTX_PREF_DEFBTN_PAD);
	useL = (WORD)(leftEdge + m.margin + TTX_PREF_DEFBTN_PAD);
	useT = btnY;
	useW = btnW;
	useH = m.rowH;
	cancelL = (WORD)(leftEdge + innerW - m.margin - btnW);

	gad = CreateContext(&mainGlist);
	if (!gad)
		goto fail;

	ng.ng_TextAttr = &m.attr;
	ng.ng_VisualInfo = vi;
	ng.ng_UserData = NULL;

	/* Left MX: fixed-size radio imagery; labels sit to the right. */
	ng.ng_LeftEdge = (WORD)(leftEdge + m.margin);
	ng.ng_TopEdge = (WORD)(top + m.margin);
	ng.ng_Width = TTX_PREF_MX_W;
	ng.ng_Height = TTX_PREF_MX_H;
	ng.ng_GadgetText = NULL;
	ng.ng_GadgetID = GID_MX;
	ng.ng_Flags = PLACETEXT_RIGHT;
	mxGad = CreateGadget(MX_KIND, gad, &ng,
		GTMX_Labels, TR_PrefsCatLabels,
		GTMX_Active, cat,
		GTMX_Spacing, 1,
		TAG_DONE);
	if (!mxGad)
		goto fail;
	gad = mxGad;

	ng.ng_Flags = PLACETEXT_IN;
	ng.ng_LeftEdge = useL;
	ng.ng_TopEdge = useT;
	ng.ng_Width = useW;
	ng.ng_Height = useH;
	ng.ng_GadgetText = (UBYTE *)"_Use";
	ng.ng_GadgetID = GID_USE;
	useGad = CreateGadget(BUTTON_KIND, gad, &ng,
		GT_Underscore, (ULONG)'_',
		TAG_DONE);
	if (!useGad)
		goto fail;
	gad = useGad;

	ng.ng_LeftEdge = cancelL;
	ng.ng_GadgetText = (UBYTE *)"_Cancel";
	ng.ng_GadgetID = GID_CANCEL;
	gad = CreateGadget(BUTTON_KIND, gad, &ng,
		GT_Underscore, (ULONG)'_',
		TAG_DONE);
	if (!gad)
		goto fail;

	win = OpenWindowTags(NULL,
		WA_Title, (STRPTR)"TurboText Preferences",
		WA_InnerWidth, innerW,
		WA_InnerHeight, innerH,
		WA_AutoAdjust, TRUE,
		WA_CloseGadget, TRUE,
		WA_DragBar, TRUE,
		WA_DepthGadget, TRUE,
		WA_Activate, TRUE,
		WA_Gadgets, mainGlist,
		WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
			IDCMP_GADGETUP | IDCMP_VANILLAKEY | IDCMP_MOUSEMOVE,
		WA_PubScreen, scr,
		TAG_DONE);
	if (!win)
		goto fail;

	if (!TR_PrefsBuildPage(win, vi, &m, cat, &edit, paneL, paneT, paneW,
	    &detailGlist, detailGads, &detailCount))
		goto fail_win;

	GT_RefreshWindow(win, NULL);
	TR_PrefsDrawDecor(win, vi, &m, sepVX, sepVT, sepVH, sepHY, sepHL, sepHW,
		frameL, frameT, frameW, frameH, TR_PrefsCatLabels[cat],
		useL, useT, useW, useH);

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
				TR_PrefsDrawDecor(win, vi, &m, sepVX, sepVT,
					sepVH, sepHY, sepHL, sepHW,
					frameL, frameT, frameW, frameH,
					TR_PrefsCatLabels[cat],
					useL, useT, useW, useH);
			} else if (class == IDCMP_VANILLAKEY) {
				if (code == 27) {
					done = TRUE;
					ok = FALSE;
				} else if (code == '\r' || code == '\n') {
					done = TRUE;
					ok = TRUE;
				}
			} else if (class == IDCMP_GADGETUP && gad) {
				if (gad->GadgetID == GID_USE) {
					done = TRUE;
					ok = TRUE;
				} else if (gad->GadgetID == GID_CANCEL) {
					done = TRUE;
					ok = FALSE;
				} else if (gad->GadgetID == GID_MX) {
					mxActive = cat;
					GT_GetGadgetAttrs(mxGad, win, NULL,
						GTMX_Active, &mxActive, TAG_DONE);
					if ((WORD)mxActive != cat &&
					    mxActive < PREF_CAT_COUNT) {
						TR_PrefsHarvestPage(win, cat,
							&edit, detailGads,
							detailCount);
						TR_PrefsFreePage(win,
							&detailGlist);
						detailCount = 0;
						cat = (WORD)mxActive;
						TR_PrefsClearPane(win, &m,
							frameL, frameT,
							frameW, frameH);
						if (!TR_PrefsBuildPage(win, vi,
						    &m, cat, &edit, paneL,
						    paneT, paneW, &detailGlist,
						    detailGads, &detailCount)) {
							done = TRUE;
							ok = FALSE;
							break;
						}
						TR_PrefsDrawDecor(win, vi, &m,
							sepVX, sepVT, sepVH,
							sepHY, sepHL, sepHW,
							frameL, frameT, frameW,
							frameH,
							TR_PrefsCatLabels[cat],
							useL, useT, useW,
							useH);
					}
				}
			}
		}
	}

	if (ok) {
		TR_PrefsHarvestPage(win, cat, &edit, detailGads, detailCount);
		*p = edit;
	}

	TR_PrefsFreePage(win, &detailGlist);
	CloseWindow(win);
	FreeGadgets(mainGlist);
	FreeVisualInfo(vi);
	if (unlock)
		UnlockPubScreen(NULL, scr);
	TR_FreeFontMetrics(&m);
	return ok;

fail_win:
	TR_PrefsFreePage(win, &detailGlist);
	CloseWindow(win);
fail:
	if (mainGlist)
		FreeGadgets(mainGlist);
	if (vi)
		FreeVisualInfo(vi);
	if (unlock)
		UnlockPubScreen(NULL, scr);
	TR_FreeFontMetrics(&m);
	return FALSE;
}
