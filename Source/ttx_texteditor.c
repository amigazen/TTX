/*
 * TTX driver - multiline text editor BOOPSI class
 *
 * Subclass of gadgetclass. Domain fills the window client area (inside
 * title/borders; scroll props sit in BorderRight/BorderBottom). Drawing is
 * done at window level (RenderText); GM_HANDLEINPUT routes RAWKEY/RAWMOUSE.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_texteditor.h"
#include "ttx_input.h"
#include "ttx.h"

#include <devices/inputevent.h>
#include <intuition/classes.h>
#include <intuition/gadgetclass.h>
#include <intuition/intuition.h>
#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <proto/utility.h>

/* SAS/C register-parameter BOOPSI dispatchers need A4 for FAR data. */
void geta4(void);

/* Flush init trace lines before a wedge so output.txt shows the last step. */
static VOID
TTX_InitTrace(STRPTR step)
{
	BPTR out;

	Printf("[INIT] %s\n", step);
	out = Output();
	if (out)
		Flush(out);
}

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

	CalculateMaxScroll(session, session->window);
	UpdateScrollBars(session);
	TTX_RequestRedraw(session);
}

/****************************************************************************/

static VOID
TTX_TE_ApplyCursor(
	struct Session *session,
	ULONG newCursorX,
	ULONG newCursorY)
{
	struct TTView *view;
	struct TTTextBuffer *buffer;

	view = TTX_SessionView(session);
	buffer = TT_SessionBuffer(session);
	if (!view || !buffer || !buffer->lines)
		return;
	if (newCursorY >= buffer->lineCount)
		return;

	view->cursorY = newCursorY;
	if (newCursorX <= buffer->lines[newCursorY].length)
		view->cursorX = newCursorX;
	else
		view->cursorX = buffer->lines[newCursorY].length;
	buffer->cursorX = view->cursorX;
	buffer->cursorY = view->cursorY;
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
	struct TTView *view;

	if (!session || !data || !ievent || !window || !gpi || !gpi->gpi_GInfo)
		return FALSE;
	if (!TT_SessionBuffer(session))
		return FALSE;
	view = TTX_SessionView(session);
	if (!view)
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

	/* Domain origin + gadget-relative mouse = window coordinates. */
	mouseX = (LONG)gpi->gpi_GInfo->gi_Domain.Left + (LONG)gpi->gpi_Mouse.X;
	mouseY = (LONG)gpi->gpi_GInfo->gi_Domain.Top + (LONG)gpi->gpi_Mouse.Y;

	MouseToCursor(session, window,
		mouseX, mouseY, &newCursorX, &newCursorY);
	TTX_TE_ApplyCursor(session, newCursorX, newCursorY);

	if (isPress) {
		data->mouseSelecting = TRUE;
		session->mouseSelecting = TRUE;
		session->selectStartX = view->cursorX;
		session->selectStartY = view->cursorY;
		/* Anchor = current caret (same as pre-refactor SetMarking). */
		TTX_ApplyMarking(session,
			session->selectStartY, session->selectStartX,
			view->cursorY, view->cursorX);
	} else if (data->mouseSelecting) {
		TTX_ApplyMarking(session,
			session->selectStartY, session->selectStartX,
			view->cursorY, view->cursorX);
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
	struct TTView *view;

	if (!session || !data || !gpi || !gpi->gpi_GInfo)
		return FALSE;
	if (!TT_SessionBuffer(session) || !data->mouseSelecting)
		return FALSE;
	view = TTX_SessionView(session);
	if (!view)
		return FALSE;

	window = session->window;
	if (!window)
		return FALSE;

	mouseX = (LONG)gpi->gpi_GInfo->gi_Domain.Left + (LONG)gpi->gpi_Mouse.X;
	mouseY = (LONG)gpi->gpi_GInfo->gi_Domain.Top + (LONG)gpi->gpi_Mouse.Y;

	MouseToCursor(session, window,
		mouseX, mouseY, &newCursorX, &newCursorY);
	TTX_TE_ApplyCursor(session, newCursorX, newCursorY);

	/* Extend selection end to caret while dragging (pre-refactor behaviour). */
	TTX_ApplyMarking(session,
		session->selectStartY, session->selectStartX,
		view->cursorY, view->cursorX);

	ScrollToCursor(session, window);
	TTX_TE_RefreshSession(session);
	return TRUE;
}

/****************************************************************************/

static ULONG __saveds
TTX_TE_New(Class *cl, Object *obj, struct opSet *ops)
{
	struct TTXTextEditorData *data;
	Object *gadget;

	gadget = (Object *)DoSuperMethodA(cl, obj, (Msg)ops);
	if (!gadget) {
		Printf("[INIT] TTX_TE_New: DoSuperMethodA FAIL io=%lu\n",
			(ULONG)IoErr());
		return 0;
	}

	data = (struct TTXTextEditorData *)INST_DATA(cl, gadget);
	data->session = (struct Session *)GetTagData(TEA_Session, NULL,
		ops->ops_AttrList);
	data->app = (struct TTXApplication *)GetTagData(TEA_Application, NULL,
		ops->ops_AttrList);
	data->readOnly = (BOOL)GetTagData(TEA_ReadOnly, FALSE, ops->ops_AttrList);
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
	struct TTXTextEditorData *data;
	struct Session *session;
	struct InputEvent *ievent;

	(void)cl;
	data = (struct TTXTextEditorData *)INST_DATA(cl, obj);
	session = data->session;

	/*
	 * Only stay active for an actual LBUTTON press (drag-select).
	 * ActivateGadget() with no event must not trap the window forever —
	 * that blocks menus, close, and other gadgets.
	 */
	if (!session || !session->window || !gpi)
		return (ULONG)GMR_NOREUSE;

	ievent = gpi->gpi_IEvent;
	if (!ievent || ievent->ie_Class != IECLASS_RAWMOUSE ||
	    ievent->ie_Code != IECODE_LBUTTON)
		return (ULONG)GMR_NOREUSE;

	TTX_TE_HandleMouse(session, data, ievent, session->window, gpi);
	if (!data->mouseSelecting)
		return (ULONG)GMR_NOREUSE;

	return (ULONG)GMR_MEACTIVE;
}

/****************************************************************************/

static ULONG
TTX_TE_GoInactive(Class *cl, Object *obj, struct gpGoInactive *gpgi)
{
	struct TTXTextEditorData *data;

	(void)cl;
	(void)gpgi;
	data = (struct TTXTextEditorData *)INST_DATA(cl, obj);
	data->mouseSelecting = FALSE;
	if (data->session)
		data->session->mouseSelecting = FALSE;
	return 0;
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
	APTR deadKey;
	BOOL sawButtonUp;

	data = (struct TTXTextEditorData *)INST_DATA(cl, obj);
	session = data->session;
	app = data->app;
	sawButtonUp = FALSE;
	/*
	 * Stay active only while the LBUTTON drag is in progress. Keys are
	 * handled via window IDCMP when inactive (see ttx_intui.c).
	 */
	result = (ULONG)GMR_MEACTIVE;

	if (!session || !app || !session->window)
		return (ULONG)GMR_NOREUSE;

	window = session->window;
	ievent = gpi->gpi_IEvent;
	while (ievent) {
		if (!data->readOnly && session->document &&
		    !session->document->state.readOnly) {
			if (ievent->ie_Class == IECLASS_RAWKEY) {
				deadKey = ievent->ie_EventAddress;
				TTX_InputFromInputEvent(app, session, ievent, deadKey);
			}
		}
		if (ievent->ie_Class == IECLASS_RAWMOUSE) {
			if (ievent->ie_Code == IECODE_NOBUTTON) {
				TTX_TE_HandleMouseMove(session, data, gpi);
			} else {
				if (ievent->ie_Code ==
				    (IECODE_LBUTTON | IECODE_UP_PREFIX))
					sawButtonUp = TRUE;
				TTX_TE_HandleMouse(session, data, ievent, window, gpi);
			}
		}
		ievent = ievent->ie_NextEvent;
	}

	/* Release activation so menus / close / other gadgets work again. */
	if (sawButtonUp || !data->mouseSelecting)
		result = (ULONG)GMR_NOREUSE;

	return result;
}

/****************************************************************************/

/*
 * BOOPSI dispatcher (A0=class, A2=object, A1=message). SAS/C register
 * convention; geta4() required for far data access in the driver.
 */
static ULONG __saveds __asm TTX_TE_Dispatcher(
	register __a0 Class *cl,
	register __a2 Object *obj,
	register __a1 Msg msg)
{
	ULONG result;

	geta4();

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
		result = DoSuperMethodA(cl, obj, msg);
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
	case GM_GOINACTIVE:
		result = TTX_TE_GoInactive(cl, obj, (struct gpGoInactive *)msg);
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

	/*
	 * Private class (NULL public name). Do not call FindClass() - it is
	 * ==private in intuition_lib.sfd and is not in the public link stubs.
	 */
	TTXTextEditorClass = MakeClass(
		NULL,
		GADGETCLASS,
		NULL,
		(ULONG)sizeof(struct TTXTextEditorData),
		0L);
	if (!TTXTextEditorClass)
		return FALSE;

	TTXTextEditorClass->cl_Dispatcher.h_Entry =
		(ULONG (*)())TTX_TE_Dispatcher;
	TTXTextEditorClass->cl_Dispatcher.h_SubEntry = NULL;

	/* Private classes are not AddClass()'d. */
	Printf("[INIT] TTX_TextEditor_InitClass: private class=%lx\n",
		(ULONG)TTXTextEditorClass);
	return TRUE;
}

/****************************************************************************/

VOID
TTX_TextEditor_FreeClass(VOID)
{
	if (!TTXTextEditorClass)
		return;

	FreeClass(TTXTextEditorClass);
	TTXTextEditorClass = NULL;
	Printf("[CLEANUP] TTX_TextEditor_FreeClass: private class freed\n");
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
	LONG left;
	LONG top;
	LONG width;
	LONG height;

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

	/*
	 * Domain = empty client area after title bar and size/scroll borders.
	 * Vert/horiz prop gadgets sit inside BorderRight / BorderBottom, so
	 * RelWidth/RelHeight against those borders matches the paint clip.
	 */
	TTX_GetTextClientBounds(window, &left, &top, &width, &height);

	TTX_InitTrace("TTX_TextEditor_CreateGadget: NewObject");
	gadget = (struct Gadget *)NewObject(
		TTXTextEditorClass, NULL,
		GA_ID, GID_TEXT_EDITOR,
		GA_Left, left,
		GA_Top, top,
		GA_RelWidth, -(window->BorderLeft + window->BorderRight),
		GA_RelHeight, -(window->BorderTop + window->BorderBottom),
		GA_Immediate, TRUE,
		GA_RelVerify, TRUE,
		TEA_Session, (ULONG)session,
		TEA_Application, (ULONG)app,
		TEA_ReadOnly, (ULONG)readOnly,
		TAG_DONE);
	if (!gadget) {
		Printf("[INIT] TTX_TextEditor_CreateGadget: FAIL io=%lu\n",
			(ULONG)IoErr());
		return FALSE;
	}

	gadget->Flags |= GFLG_RELWIDTH | GFLG_RELHEIGHT | GFLG_TABCYCLE |
		GFLG_GADGHNONE;

	session->textEditorGadget = gadget;
	Printf("[INIT] TTX_TextEditor_CreateGadget: gadget=%lx domain=%ld,%ld %ldx%ld\n",
		(ULONG)gadget, left, top, width, height);
	return TRUE;
}

/****************************************************************************/

VOID
TTX_TextEditor_DestroyGadget(struct Session *session)
{
	if (!session || !session->textEditorGadget)
		return;

	/*
	 * Scroll gadget teardown RemoveGList's the combined list; only
	 * dispose the BOOPSI object here.
	 */
	DisposeObject((Object *)session->textEditorGadget);
	session->textEditorGadget = NULL;
}

/****************************************************************************/

VOID
TTX_TextEditor_Activate(struct Session *session)
{
	(void)session;
	/*
	 * Do not call ActivateGadget(). A full-window editor that returns
	 * GMR_MEACTIVE until deactivated swallows menus, close, and other
	 * gadgets. Clicks use GM_GOACTIVE; typing uses window IDCMP.
	 */
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
