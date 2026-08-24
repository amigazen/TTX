/*
 * TTX driver - Intuition windowing and IDCMP event handling
 *
 * Per-window UserPort (WA_IDCMP). ReplyMsg before any nested Intuition call.
 * Scroll bars use ICA -> IDCMP_IDCMPUPDATE for live drag; text editing uses
 * the registered ttxtexteditorclass gadget (GM_HANDLEINPUT).
 * No super-bitmap or layers.library calls.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_intui.h"
#include "ttx_boopsi.h"
#include "ttx_texteditor.h"
#include "ttx_input.h"
#include "ttx_prefs.h"
#include "ttx_commands_prot.h"

#include <devices/inputevent.h>

/****************************************************************************/

static BOOL
TTX_IntuiDecodeMenuItem(struct MenuItem *item, ULONG *outMenu, ULONG *outItem)
{
	ULONG userData;

	if (!item || !outMenu || !outItem)
		return FALSE;

	*outMenu = 0;
	*outItem = 0;

	userData = (ULONG)GTMENUITEM_USERDATA(item);
	if (userData == 0)
		return FALSE;

	/* Builtin Open... sentinel — not a packed index. */
	if (userData == TTX_MENU_UD_FLAG) {
		*outMenu = 0;
		*outItem = 0;
		return TRUE;
	}

	/*
	 * DFN menus store a DFNMenuEntry* in UserData (heap pointer). Those must
	 * NOT be decoded as (menu<<8)|item — that produced garbage like menu=198.
	 * Packed builtin userdata fits in 16 bits.
	 */
	if (userData >= 0x10000UL)
		return FALSE;

	*outMenu = (userData >> 8) & 0xFF;
	*outItem = userData & 0xFF;
	return TRUE;
}

/****************************************************************************/

/*
 * True menu pick codes are never the lone IEQUALIFIER_RELVERIFY bit (0x8000).
 * MENUNULL is 0xFFFF.
 */
static BOOL
TTX_IntuiIsPlausibleMenuPick(ULONG pick)
{
	if (pick == 0 || pick == (ULONG)MENUNULL || pick == 0xFFFFUL)
		return FALSE;
	if (pick == 0x8000UL)
		return FALSE;
	if (MENUNUM(pick) > 15)
		return FALSE;
	if (ITEMNUM(pick) > 63)
		return FALSE;
	return TRUE;
}

static BOOL
TTX_IntuiHandleMenuPick(struct TTXApplication *app, struct Session *session,
	struct IntuiMessage *imsg)
{
	ULONG menuCode;
	struct MenuItem *item;
	ULONG menuNum;
	ULONG itemNum;
	APTR userData;

	if (!app || !session || !imsg || !session->window)
		return FALSE;

	/*
	 * Prefer Code (standard MENUPICK). Some builds deliver key ASCII in
	 * Qualifier with Code==0; do not treat Qualifier as a pick unless it
	 * looks like a real menu number — bare 0x8000 is IEQUALIFIER_RELVERIFY.
	 */
	menuCode = (ULONG)(UWORD)imsg->Code;
	if (!TTX_IntuiIsPlausibleMenuPick(menuCode)) {
		ULONG q;

		q = (ULONG)(UWORD)imsg->Qualifier;
		if (TTX_IntuiIsPlausibleMenuPick(q))
			menuCode = q;
	}

	Printf("[INTUI] MENUPICK code=%04lx qual=%04lx -> pick=%04lx\n",
		(ULONG)(UWORD)imsg->Code, (ULONG)(UWORD)imsg->Qualifier,
		menuCode);

	if (!TTX_IntuiIsPlausibleMenuPick(menuCode)) {
		TTX_ResetMenuStrip(session);
		return TRUE;
	}

	while (TTX_IntuiIsPlausibleMenuPick(menuCode)) {
		item = ItemAddress(session->window->MenuStrip, (UWORD)menuCode);
		if (!item)
			break;

		menuNum = 0;
		itemNum = 0;
		userData = GTMENUITEM_USERDATA(item);
		if (!TTX_IntuiDecodeMenuItem(item, &menuNum, &itemNum)) {
			/*
			 * DFN menus store an entry pointer in UserData (not a packed
			 * menu/item index). Still dispatch via the pointer.
			 */
			if (!(session->menuDFNBacking && userData))
				break;
		}

		Printf("[INTUI] MENUPICK pick=%04lx -> menu=%lu item=%lu ud=%lx\n",
			menuCode, menuNum, itemNum, (ULONG)userData);

		if (!TTX_HandleMenuPick(app, session, menuNum, itemNum, userData))
			break;

		menuCode = (ULONG)item->NextSelect;
	}

	TTX_ResetMenuStrip(session);
	return TRUE;
}

/****************************************************************************/

static VOID
TTX_IntuiRedrawText(struct Session *session)
{
	if (!session || !session->window || !TT_SessionBuffer(session))
		return;

	CalculateMaxScroll(session, session->window);
	TTX_RequestRedraw(session);
}

/****************************************************************************/

static VOID
TTX_IntuiRefreshSession(struct Session *session)
{
	if (!session || !session->window || !TT_SessionBuffer(session))
		return;

	CalculateMaxScroll(session, session->window);
	UpdateScrollBars(session);
	TTX_RequestRedraw(session);
}

/****************************************************************************/

BOOL
TTX_IntuiOpenWindow(struct Session *session, struct Screen *screen)
{
	struct TagItem tags[20];
	ULONG i;

	if (!session || !screen)
		return FALSE;

	i = 0;
	tags[i].ti_Tag = WA_Flags;
	tags[i].ti_Data = session->windowState.flags;
	i++;
	tags[i].ti_Tag = WA_Left;
	tags[i].ti_Data = (ULONG)session->windowState.leftEdge;
	i++;
	tags[i].ti_Tag = WA_Top;
	tags[i].ti_Data = (ULONG)session->windowState.topEdge;
	i++;
	tags[i].ti_Tag = WA_InnerWidth;
	tags[i].ti_Data = session->windowState.innerWidth;
	i++;
	tags[i].ti_Tag = WA_InnerHeight;
	tags[i].ti_Data = session->windowState.innerHeight;
	i++;
	tags[i].ti_Tag = WA_Title;
	tags[i].ti_Data = (ULONG)(session->windowState.title
		? session->windowState.title : (STRPTR)"Untitled");
	i++;
	tags[i].ti_Tag = WA_IDCMP;
	tags[i].ti_Data = session->windowState.idcmpFlags;
	i++;
	tags[i].ti_Tag = WA_ScreenTitle;
	tags[i].ti_Data = (ULONG)(session->windowState.screenTitle
		? session->windowState.screenTitle : (STRPTR)"TTX");
	i++;
	tags[i].ti_Tag = WA_AutoAdjust;
	tags[i].ti_Data = TRUE;
	i++;
	tags[i].ti_Tag = WA_PubScreen;
	tags[i].ti_Data = (ULONG)screen;
	i++;
	tags[i].ti_Tag = WA_DragBar;
	tags[i].ti_Data = TRUE;
	i++;
	tags[i].ti_Tag = WA_DepthGadget;
	tags[i].ti_Data = TRUE;
	i++;
	tags[i].ti_Tag = WA_CloseGadget;
	tags[i].ti_Data = TRUE;
	i++;
	tags[i].ti_Tag = WA_SizeGadget;
	tags[i].ti_Data = TRUE;
	i++;
	tags[i].ti_Tag = WA_SizeBRight;
	tags[i].ti_Data = TRUE;
	i++;
	tags[i].ti_Tag = WA_SizeBBottom;
	tags[i].ti_Data = TRUE;
	i++;
	tags[i].ti_Tag = TAG_DONE;
	tags[i].ti_Data = 0;

	session->window = OpenWindowTagList(NULL, tags);
	if (!session->window)
		return FALSE;

	WindowLimits(session->window, session->windowState.minWidth,
		session->windowState.minHeight, session->windowState.maxWidth,
		session->windowState.maxHeight);

	session->windowState.leftEdge = session->window->LeftEdge;
	session->windowState.topEdge = session->window->TopEdge;
	session->windowState.innerWidth = session->window->Width
		- session->window->BorderLeft - session->window->BorderRight;
	session->windowState.innerHeight = session->window->Height
		- session->window->BorderTop - session->window->BorderBottom;
	session->windowState.windowOpen = TRUE;

	Printf("[INTUI] OpenWindow ok window=%lx drag=%s depth=%s\n",
		(ULONG)session->window,
		(session->window->Flags & WFLG_DRAGBAR) ? "YES" : "NO",
		(session->window->Flags & WFLG_DEPTHGADGET) ? "YES" : "NO");

	return TRUE;
}

/****************************************************************************/

VOID
TTX_IntuiCloseWindow(struct TTXApplication *app, struct Session *session)
{
	struct IntuiMessage *imsg;
	struct Window *window;
	struct MsgPort *userPort;

	(void)app;

	if (!session || !session->window || session->window == INVALID_RESOURCE)
		return;

	window = session->window;

	TTX_FreeMenuStrip(session);
	TTX_BoopsiDestroyScrollGadgets(session);
	TTX_TextEditor_DestroyGadget(session);

	userPort = window->UserPort;
	if (userPort) {
		while ((imsg = (struct IntuiMessage *)GetMsg(userPort)) != NULL)
			ReplyMsg((struct Message *)imsg);
	}

	Forbid();
	ModifyIDCMP(window, 0UL);
	CloseWindow(window);
	Permit();

	session->window = INVALID_RESOURCE;
}

/****************************************************************************/

BOOL
TTX_IntuiHandleMessage(struct TTXApplication *app, struct Session *portSession,
	struct IntuiMessage *imsg)
{
	struct Session *session;
	BOOL result;
	ULONG gadgetID;

	if (!app || !imsg)
		return FALSE;

	session = portSession;
	if (!session || !session->window || session->window == INVALID_RESOURCE) {
		session = app->sessions;
		while (session) {
			if (session->window == imsg->IDCMPWindow)
				break;
			session = session->next;
		}
	}
	if (!session)
		return FALSE;

	result = FALSE;

	switch (imsg->Class) {
	case IDCMP_MENUPICK:
		TTX_IntuiHandleMenuPick(app, session, imsg);
		result = TRUE;
		break;

	case IDCMP_CLOSEWINDOW:
		if (!TTX_PromptSaveBeforeClose(app, session)) {
			result = TRUE;
			break;
		}
		TTX_RequestDestroySession(app, session);
		result = TRUE;
		break;

	case IDCMP_VANILLAKEY:
	case IDCMP_RAWKEY:
		/*
		 * Always handle window IDCMP keys (HEAD~1 behaviour). The text
		 * editor gadget also receives IECLASS_RAWKEY when active; InputRawKey
		 * is idempotent enough for navigation, and printable inserts are
		 * gated by the active gadget typically consuming RAWKEY first.
		 * When the gadget is absent or inactive, this is the only path.
		 */
		TTX_InputFromIntuiMessage(app, session, imsg);
		result = TRUE;
		break;

	case IDCMP_REFRESHWINDOW:
		if (TT_SessionBuffer(session)) {
			BeginRefresh(session->window);
			TTX_DrawSession(session);
			EndRefresh(session->window, TRUE);
		}
		result = TRUE;
		break;

	case IDCMP_NEWSIZE:
	case IDCMP_CHANGEWINDOW:
		if (TT_SessionBuffer(session))
			TTX_IntuiRefreshSession(session);
		result = TRUE;
		break;

	case IDCMP_GADGETUP:
		gadgetID = (ULONG)imsg->Code;
		if (gadgetID == 0 && imsg->IAddress)
			GetAttr(GA_ID, (Object *)imsg->IAddress, &gadgetID);
		/*
		 * Qualifier 0x8000 is IEQUALIFIER_RELVERIFY, not a gadget id.
		 * Prefer IAddress GA_ID above; only fall back to Qualifier when it
		 * looks like a real GID.
		 */
		if (gadgetID == 0) {
			ULONG q;

			q = (ULONG)(UWORD)imsg->Qualifier;
			if (q != 0 && q != 0x8000UL)
				gadgetID = q;
		}
		if (TTX_BoopsiHandleScrollGadgetUp(session, gadgetID)) {
			TTX_IntuiRefreshSession(session);
			ActivateWindow(session->window);
		}
		result = TRUE;
		break;

	case IDCMP_IDCMPUPDATE:
		if (session->scroll.suppressIcmp)
			break;
		gadgetID = (ULONG)imsg->Code;
		if (gadgetID == 0 && imsg->IAddress)
			GetAttr(GA_ID, (Object *)imsg->IAddress, &gadgetID);
		if (gadgetID == 0) {
			ULONG q;

			q = (ULONG)(UWORD)imsg->Qualifier;
			if (q != 0 && q != 0x8000UL)
				gadgetID = q;
		}
		if (TTX_BoopsiHandleIdcmpUpdate(session, gadgetID))
			TTX_IntuiRedrawText(session);
		result = TRUE;
		break;

	case IDCMP_MOUSEBUTTONS:
		if (session->textEditorGadget) {
			result = TRUE;
			break;
		}
		{
			ULONG newCursorX;
			ULONG newCursorY;
			BOOL isPress;
			BOOL isRelease;

			isPress = FALSE;
			isRelease = FALSE;
			if (imsg->Code == IECODE_LBUTTON)
				isPress = TRUE;
			if (imsg->Code == (IECODE_LBUTTON | IECODE_UP_PREFIX))
				isRelease = TRUE;

			if (isPress || isRelease) {
				struct TTView *view;
				struct TTDocument *doc;
				struct TTTextBuffer *buffer;

				view = TTX_SessionView(session);
				doc = session->document;
				buffer = TT_SessionBuffer(session);
				/* Clicking a split pane activates that view. */
				if (isPress && session->splitRatio > 0 && doc &&
				    doc->views && doc->views->next && session->window) {
					ULONG mid = session->splitY;
					if (mid == 0)
						mid = session->window->BorderTop +
							((session->window->Height -
							  session->window->BorderTop -
							  session->window->BorderBottom) *
							 session->splitRatio) / 100;
					if ((ULONG)imsg->MouseY >= mid) {
						doc->activeView = doc->views->next;
						doc->views->active = FALSE;
						doc->views->next->active = TRUE;
					} else {
						doc->activeView = doc->views;
						doc->views->active = TRUE;
						doc->views->next->active = FALSE;
					}
					view = doc->activeView;
				}
				if (isPress)
					ActivateWindow(session->window);
				MouseToCursor(session, session->window,
					imsg->MouseX, imsg->MouseY,
					&newCursorX, &newCursorY);
				if (view && buffer && buffer->lines &&
				    newCursorY < buffer->lineCount) {
					view->cursorY = newCursorY;
					if (newCursorX <= buffer->lines[newCursorY].length)
						view->cursorX = newCursorX;
					else if (TTX_PrefsGet()->freeForm)
						view->cursorX = newCursorX;
					else
						view->cursorX =
							buffer->lines[newCursorY].length;
					buffer->cursorX = view->cursorX;
					buffer->cursorY = view->cursorY;
				}
				if (isPress && view) {
					session->mouseSelecting = FALSE;
					session->selectStartX = view->cursorX;
					session->selectStartY = view->cursorY;
					if (TTX_PrefsGet()->selectWhenDragging) {
						session->mouseSelecting = TRUE;
						TTX_ApplyMarking(session,
							session->selectStartY,
							session->selectStartX,
							view->cursorY, view->cursorX);
					}
				} else if (session->mouseSelecting && view) {
					TTX_ApplyMarking(session,
						session->selectStartY, session->selectStartX,
						view->cursorY, view->cursorX);
					session->mouseSelecting = FALSE;
				}
				TTX_IntuiRefreshSession(session);
			}
			result = TRUE;
		}
		break;

	case IDCMP_MOUSEMOVE:
		if (session->textEditorGadget) {
			result = TRUE;
			break;
		}
		if (session->mouseSelecting) {
			ULONG newCursorX;
			ULONG newCursorY;
			struct TTView *view;
			struct TTTextBuffer *buffer;

			view = TTX_SessionView(session);
			buffer = TT_SessionBuffer(session);
			MouseToCursor(session, session->window,
				imsg->MouseX, imsg->MouseY,
				&newCursorX, &newCursorY);
			if (view && buffer && buffer->lines &&
			    newCursorY < buffer->lineCount) {
				view->cursorY = newCursorY;
				if (newCursorX <= buffer->lines[newCursorY].length)
					view->cursorX = newCursorX;
				else
					view->cursorX = buffer->lines[newCursorY].length;
				buffer->cursorX = view->cursorX;
				buffer->cursorY = view->cursorY;
				TTX_ApplyMarking(session,
					session->selectStartY, session->selectStartX,
					view->cursorY, view->cursorX);
				ScrollToCursor(session, session->window);
			}
			TTX_IntuiRefreshSession(session);
		}
		result = TRUE;
		break;

	case IDCMP_ACTIVEWINDOW:
		TTX_NoteSessionActivated(app, session);
		if (TT_SessionBuffer(session))
			UpdateCursor(session->window, session);
		result = TRUE;
		break;

	case IDCMP_INACTIVEWINDOW:
		result = TRUE;
		break;

	default:
		break;
	}

	return result;
}

/****************************************************************************/

VOID
TTX_IntuiRebuildSignalMask(struct TTXApplication *app)
{
	struct Session *session;

	if (!app)
		return;

	app->sigmask = (1UL << app->appPort->mp_SigBit);
	session = app->sessions;
	while (session) {
		if (session->window && session->window != INVALID_RESOURCE &&
		    session->window->UserPort)
			app->sigmask |= (1UL << session->window->UserPort->mp_SigBit);
		if (session->infoWindow && session->infoWindow->UserPort)
			app->sigmask |= (1UL << session->infoWindow->UserPort->mp_SigBit);
		session = session->next;
	}
}

/****************************************************************************/
