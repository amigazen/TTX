/*
 * TTX driver - multiline text editor BOOPSI class
 *
 * Subclass of gadgetclass. GM_RENDER draws the document via RenderText;
 * GM_HANDLEINPUT routes IECLASS_RAWKEY / IECLASS_RAWMOUSE to turbotext.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_texteditor.h"

#include <devices/inputevent.h>
#include <intuition/classes.h>
#include <intuition/gadgetclass.h>
#include <intuition/intuition.h>
#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <proto/keymap.h>

/****************************************************************************/

struct TTXTextEditorData {
	struct Session *session;
	struct TTXApplication *app;
	BOOL readOnly;
	BOOL mouseSelecting;
};

static Class *TTXTextEditorClass = NULL;

/****************************************************************************/

static VOID
TTX_TE_RefreshSession(struct Session *session)
{
	if (!session || !session->window || !TT_SessionBuffer(session))
		return;

	CalculateMaxScroll(TT_SessionBuffer(session), session->window);
	UpdateScrollBars(session);
	RenderText(session->window, session);
	UpdateCursor(session->window, session);
}

/****************************************************************************/

static BOOL
TTX_TE_HandleKey(
	struct TTXApplication *app,
	struct Session *session,
	struct TTXTextEditorData *data,
	struct InputEvent *ievent)
{
	ULONG qual;
	STRPTR insArgs[1];
	TEXT chBuf[2];
	UBYTE rawCode;
	struct InputEvent mapEvent;
	UBYTE charBuffer[10];
	WORD chars;
	struct KeyMap *keymap;
	BOOL processed;

	if (!app || !session || !data || !ievent || !TT_SessionBuffer(session))
		return FALSE;
	if (data->readOnly || session->document->state.readOnly)
		return FALSE;
	if (ievent->ie_Class != IECLASS_RAWKEY)
		return FALSE;

	processed = FALSE;
	qual = (ULONG)ievent->ie_Qualifier;
	rawCode = (UBYTE)ievent->ie_Code;
	if (rawCode & IECODE_UP_PREFIX)
		return FALSE;
	if (rawCode >= 0x60 && rawCode <= 0x6A)
		return FALSE;

	if (rawCode == 0x41) {
		TTX_DoEngineCommand(app, session, "Delete", NULL, 0);
		processed = TRUE;
	} else if (rawCode == 0x42) {
		TTX_DoEngineCommand(app, session, "DeleteForward", NULL, 0);
		processed = TRUE;
	} else if (rawCode == 0x43) {
		TTX_DoEngineCommand(app, session, "InsertLine", NULL, 0);
		processed = TRUE;
	} else if (rawCode == 0x4F) {
		if (TT_SessionBuffer(session)->cursorX > 0)
			TT_SessionBuffer(session)->cursorX--;
		else if (TT_SessionBuffer(session)->cursorY > 0) {
			TT_SessionBuffer(session)->cursorY--;
			TT_SessionBuffer(session)->cursorX =
				TT_SessionBuffer(session)->lines[
					TT_SessionBuffer(session)->cursorY].length;
		}
		processed = TRUE;
	} else if (rawCode == 0x4E) {
		if (TT_SessionBuffer(session)->cursorX <
		    TT_SessionBuffer(session)->lines[
			    TT_SessionBuffer(session)->cursorY].length)
			TT_SessionBuffer(session)->cursorX++;
		else if (TT_SessionBuffer(session)->cursorY <
			 TT_SessionBuffer(session)->lineCount - 1) {
			TT_SessionBuffer(session)->cursorY++;
			TT_SessionBuffer(session)->cursorX = 0;
		}
		processed = TRUE;
	} else if (rawCode == 0x4C) {
		if (TT_SessionBuffer(session)->cursorY > 0)
			TT_SessionBuffer(session)->cursorY--;
		processed = TRUE;
	} else if (rawCode == 0x4D) {
		if (TT_SessionBuffer(session)->cursorY <
		    TT_SessionBuffer(session)->lineCount - 1)
			TT_SessionBuffer(session)->cursorY++;
		processed = TRUE;
	} else if (KeymapBase) {
		keymap = (struct KeyMap *)KeymapBase;
		mapEvent.ie_Class = IECLASS_RAWKEY;
		mapEvent.ie_Code = rawCode;
		mapEvent.ie_Qualifier = (UWORD)qual;
		mapEvent.ie_SubClass = 0;
		mapEvent.ie_X = 0;
		mapEvent.ie_Y = 0;
		mapEvent.ie_NextEvent = NULL;
		mapEvent.ie_TimeStamp = ievent->ie_TimeStamp;
		chars = MapRawKey(&mapEvent, charBuffer, 9, keymap);
		if (chars > 0 && charBuffer[0] >= 32) {
			chBuf[0] = (TEXT)charBuffer[0];
			chBuf[1] = '\0';
			insArgs[0] = chBuf;
			TTX_DoEngineCommand(app, session, "Insert", insArgs, 1);
			processed = TRUE;
		}
	}

	if (processed) {
		session->document->state.modified = TT_SessionBuffer(session)->modified;
		TTX_TE_RefreshSession(session);
	}

	return processed;
}

/****************************************************************************/

static BOOL
TTX_TE_HandleMouse(
	struct Session *session,
	struct TTXTextEditorData *data,
	struct InputEvent *ievent,
	struct Window *window,
	struct gpInput *gpi)
{
	ULONG newCursorX;
	ULONG newCursorY;
	BOOL isPress;
	BOOL isRelease;
	LONG mouseX;
	LONG mouseY;

	if (!session || !data || !ievent || !window || !gpi || !gpi->gpi_GInfo)
		return FALSE;
	if (!TT_SessionBuffer(session))
		return FALSE;
	if (ievent->ie_Class != IECLASS_RAWMOUSE)
		return FALSE;

	isPress = FALSE;
	isRelease = FALSE;
	if (ievent->ie_Code == IECODE_LBUTTON)
		isPress = TRUE;
	if (ievent->ie_Code == (IECODE_LBUTTON | IECODE_UP_PREFIX))
		isRelease = TRUE;

	if (!isPress && !isRelease)
		return FALSE;

	if (isPress)
		ActivateWindow(window);

	mouseX = (LONG)gpi->gpi_GInfo->gi_Domain.Left + (LONG)gpi->gpi_Mouse.X;
	mouseY = (LONG)gpi->gpi_GInfo->gi_Domain.Top + (LONG)gpi->gpi_Mouse.Y;

	MouseToCursor(TT_SessionBuffer(session), window,
		mouseX, mouseY, &newCursorX, &newCursorY);

	if (newCursorY < TT_SessionBuffer(session)->lineCount) {
		TT_SessionBuffer(session)->cursorY = newCursorY;
		if (newCursorX <= TT_SessionBuffer(session)->lines[newCursorY].length)
			TT_SessionBuffer(session)->cursorX = newCursorX;
		else
			TT_SessionBuffer(session)->cursorX =
				TT_SessionBuffer(session)->lines[newCursorY].length;
	}

	if (isPress) {
		data->mouseSelecting = TRUE;
		session->mouseSelecting = TRUE;
		session->selectStartX = TT_SessionBuffer(session)->cursorX;
		session->selectStartY = TT_SessionBuffer(session)->cursorY;
	} else if (data->mouseSelecting) {
		data->mouseSelecting = FALSE;
		session->mouseSelecting = FALSE;
	}

	TTX_TE_RefreshSession(session);
	return TRUE;
}

/****************************************************************************/

static BOOL
TTX_TE_HandleMouseMove(
	struct Session *session,
	struct TTXTextEditorData *data,
	struct gpInput *gpi)
{
	ULONG newCursorX;
	ULONG newCursorY;
	LONG mouseX;
	LONG mouseY;
	struct Window *window;

	if (!session || !data || !gpi || !gpi->gpi_GInfo)
		return FALSE;
	if (!TT_SessionBuffer(session) || !data->mouseSelecting)
		return FALSE;

	window = session->window;
	if (!window)
		return FALSE;

	mouseX = (LONG)gpi->gpi_GInfo->gi_Domain.Left + (LONG)gpi->gpi_Mouse.X;
	mouseY = (LONG)gpi->gpi_GInfo->gi_Domain.Top + (LONG)gpi->gpi_Mouse.Y;

	MouseToCursor(TT_SessionBuffer(session), window,
		mouseX, mouseY, &newCursorX, &newCursorY);

	if (newCursorY < TT_SessionBuffer(session)->lineCount) {
		TT_SessionBuffer(session)->cursorY = newCursorY;
		TT_SessionBuffer(session)->cursorX = newCursorX;
	}

	TTX_TE_RefreshSession(session);
	return TRUE;
}

/****************************************************************************/

static ULONG __saveds
TTX_TE_New(Class *cl, Object *obj, struct opSet *ops)
{
	struct TTXTextEditorData *data;
	Object *gadget;

	geta4();

	gadget = (Object *)DoSuperMethodA(cl, obj, (Msg)ops);
	if (!gadget) {
		Printf("[INIT] TTX_TE_New: DoSuperMethodA FAIL io=%lu\n",
			(ULONG)IoErr());
		return 0;
	}

	data = (struct TTXTextEditorData *)INST_DATA(cl, gadget);
	data->session = NULL;
	data->app = NULL;
	data->readOnly = FALSE;
	data->mouseSelecting = FALSE;

	return (ULONG)gadget;
}

/****************************************************************************/

static ULONG
TTX_TE_Dispose(Class *cl, Object *obj, Msg msg)
{
	return DoSuperMethodA(cl, obj, msg);
}

/****************************************************************************/

static ULONG
TTX_TE_Set(Class *cl, Object *obj, struct opSet *ops)
{
	struct TTXTextEditorData *data;
	struct TagItem *tag;
	BOOL passToSuper;

	data = (struct TTXTextEditorData *)INST_DATA(cl, obj);
	passToSuper = FALSE;
	for (tag = ops->ops_AttrList; tag->ti_Tag != TAG_DONE; tag++) {
		if (tag->ti_Tag == TEA_Session)
			data->session = (struct Session *)tag->ti_Data;
		else if (tag->ti_Tag == TEA_Application)
			data->app = (struct TTXApplication *)tag->ti_Data;
		else if (tag->ti_Tag == TEA_ReadOnly)
			data->readOnly = (BOOL)tag->ti_Data;
		else
			passToSuper = TRUE;
	}
	if (passToSuper)
		return DoSuperMethodA(cl, obj, (Msg)ops);
	return 0;
}

/****************************************************************************/

static ULONG
TTX_TE_Render(Class *cl, Object *obj, struct gpRender *gpr)
{
	/*
	 * Drawing is done at window level (RenderText). GM_RENDER here caused
	 * refresh storms when RefreshGList re-entered the full-window paint.
	 */
	(void)cl;
	(void)obj;
	(void)gpr;
	return 0;
}

/****************************************************************************/

static ULONG
TTX_TE_GoActive(Class *cl, Object *obj, struct gpInput *gpi)
{
	(void)cl;
	(void)obj;
	(void)gpi;
	return (ULONG)(GMR_MEACTIVE | GMR_REUSE);
}

/****************************************************************************/

static ULONG
TTX_TE_HandleInput(Class *cl, Object *obj, struct gpInput *gpi)
{
	struct TTXTextEditorData *data;
	struct Session *session;
	struct TTXApplication *app;
	struct InputEvent *ievent;
	struct Window *window;
	ULONG result;

	data = (struct TTXTextEditorData *)INST_DATA(cl, obj);
	session = data->session;
	app = data->app;
	result = (ULONG)(GMR_MEACTIVE | GMR_REUSE);

	if (!session || !app || !session->window)
		return result;

	window = session->window;
	ievent = gpi->gpi_IEvent;
	while (ievent) {
		if (ievent->ie_Class == IECLASS_RAWKEY)
			TTX_TE_HandleKey(app, session, data, ievent);
		else if (ievent->ie_Class == IECLASS_RAWMOUSE) {
			if (ievent->ie_Code == IECODE_NOBUTTON)
				TTX_TE_HandleMouseMove(session, data, gpi);
			else
				TTX_TE_HandleMouse(session, data, ievent, window, gpi);
		}
		ievent = ievent->ie_NextEvent;
	}

	return result;
}

/****************************************************************************/

/*
 * Register hook entry (A0/A2/A1) calling stack-args h_SubEntry.
 * Same role as amiga.lib HookEntry; implemented in C for SAS/C register params.
 */
static ULONG __saveds __asm TTX_HookEntry(
	register __a0 struct Hook *hook,
	register __a2 Object *obj,
	register __a1 Msg msg)
{
	ULONG __stdargs (*sub)(struct Hook *, Object *, Msg);

	sub = (ULONG __stdargs (*)(struct Hook *, Object *, Msg))hook->h_SubEntry;
	return sub(hook, obj, msg);
}

/****************************************************************************/

static ULONG __saveds __stdargs TTX_TE_Dispatch(
	struct Hook *hook,
	Object *obj,
	Msg msg)
{
	Class *cl;
	ULONG result;

	if (obj)
		cl = OCLASS(obj);
	else
		cl = (Class *)hook;

	switch (msg->MethodID) {
	case OM_NEW:
		result = TTX_TE_New(cl, obj, (struct opSet *)msg);
		break;
	case OM_DISPOSE:
		result = TTX_TE_Dispose(cl, obj, msg);
		break;
	case OM_SET:
		result = TTX_TE_Set(cl, obj, (struct opSet *)msg);
		break;
	case GM_HITTEST:
		result = (ULONG)GMR_GADGETHIT;
		break;
	case GM_RENDER:
		result = TTX_TE_Render(cl, obj, (struct gpRender *)msg);
		break;
	case GM_GOACTIVE:
		result = TTX_TE_GoActive(cl, obj, (struct gpInput *)msg);
		break;
	case GM_HANDLEINPUT:
		result = TTX_TE_HandleInput(cl, obj, (struct gpInput *)msg);
		break;
	default:
		result = DoSuperMethodA(cl, obj, msg);
		break;
	}

	return result;
}

/****************************************************************************/

BOOL
TTX_TextEditor_InitClass(VOID)
{
	if (TTXTextEditorClass)
		return TRUE;

	TTXTextEditorClass = MakeClass(
		TTXTEXTEDITORCLASS,
		GADGETCLASS,
		NULL,
		(ULONG)sizeof(struct TTXTextEditorData),
		0L);
	if (!TTXTextEditorClass)
		return FALSE;

	TTXTextEditorClass->cl_Dispatcher.h_Entry = (ULONG (*)())TTX_HookEntry;
	TTXTextEditorClass->cl_Dispatcher.h_SubEntry =
		(ULONG (*)())TTX_TE_Dispatch;

	AddClass(TTXTextEditorClass);
	Printf("[INIT] TTX_TextEditor_InitClass: registered %s\n",
		TTXTEXTEDITORCLASS);
	return TRUE;
}

/****************************************************************************/

VOID
TTX_TextEditor_FreeClass(VOID)
{
	if (!TTXTextEditorClass)
		return;

	RemoveClass(TTXTextEditorClass);
	FreeClass(TTXTextEditorClass);
	TTXTextEditorClass = NULL;
}

/****************************************************************************/

BOOL
TTX_TextEditor_CreateGadget(
	struct TTXApplication *app,
	struct Session *session)
{
	struct Window *window;
	struct Gadget *gadget;
	BOOL readOnly;
	ULONG gadgetW;
	ULONG gadgetH;

	if (!app || !session || !session->window)
		return FALSE;
	if (session->textEditorGadget)
		return TRUE;
	if (!TTXTextEditorClass && !TTX_TextEditor_InitClass())
		return FALSE;

	window = session->window;
	readOnly = FALSE;
	if (session->document)
		readOnly = session->document->state.readOnly;

	gadgetW = (ULONG)window->Width - (ULONG)window->BorderLeft -
		(ULONG)window->BorderRight;
	gadgetH = (ULONG)window->Height - (ULONG)window->BorderTop -
		(ULONG)window->BorderBottom;
	if (gadgetW < 8)
		gadgetW = 8;
	if (gadgetH < 8)
		gadgetH = 8;

	gadget = (struct Gadget *)NewObject(
		NULL, TTXTEXTEDITORCLASS,
		GA_ID, GID_TEXT_EDITOR,
		GA_Left, window->BorderLeft,
		GA_Top, window->BorderTop,
		GA_Width, gadgetW,
		GA_Height, gadgetH,
		GA_Immediate, TRUE,
		GA_RelVerify, TRUE,
		TAG_DONE);
	if (!gadget) {
		Printf("[INIT] TTX_TextEditor_CreateGadget: FAIL io=%lu\n",
			(ULONG)IoErr());
		return FALSE;
	}

	DoMethod((Object *)gadget, OM_SET,
		TEA_Session, (ULONG)session,
		TEA_Application, (ULONG)app,
		TEA_ReadOnly, (ULONG)readOnly,
		TAG_DONE);

	gadget->Flags |= GFLG_TABCYCLE;

	session->textEditorGadget = gadget;
	Printf("[INIT] TTX_TextEditor_CreateGadget: gadget=%lx\n", (ULONG)gadget);
	return TRUE;
}

/****************************************************************************/

VOID
TTX_TextEditor_DestroyGadget(struct Session *session)
{
	if (!session || !session->textEditorGadget)
		return;

	/*
	 * Scroll gadget teardown calls RemoveGList on the combined list;
	 * only dispose the BOOPSI object here.
	 */
	DisposeObject((Object *)session->textEditorGadget);
	session->textEditorGadget = NULL;
}

/****************************************************************************/

VOID
TTX_TextEditor_Activate(struct Session *session)
{
	if (!session || !session->window || !session->textEditorGadget)
		return;

	/* ActivateGadget only — ActivateWindow is already implied when active. */
	ActivateGadget(session->textEditorGadget, session->window, NULL);
}

/****************************************************************************/

VOID
TTX_TextEditor_Refresh(struct Session *session)
{
	if (!session || !session->window)
		return;

	TTX_TE_RefreshSession(session);
	if (session->textEditorGadget)
		RefreshGList(session->textEditorGadget, session->window, NULL, 1);
}

/****************************************************************************/
