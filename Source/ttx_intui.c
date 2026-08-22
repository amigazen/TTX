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

#include <devices/inputevent.h>
#include <proto/keymap.h>

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
	if (userData == TTX_MENU_UD_FLAG) {
		*outMenu = 0;
		*outItem = 0;
		return TRUE;
	}
	if (userData != 0 && userData < 256UL) {
		*outMenu = 0;
		*outItem = userData;
		return TRUE;
	}
	if (userData != 0) {
		*outMenu = (userData >> 8) & 0xFF;
		*outItem = userData & 0xFF;
		return TRUE;
	}

	return FALSE;
}

/****************************************************************************/

static BOOL
TTX_IntuiMapKey(struct IntuiMessage *imsg, UBYTE *outChar)
{
	UWORD rawCode;
	ULONG qual;
	struct InputEvent ievent;
	UBYTE charBuffer[10];
	WORD chars;
	struct KeyMap *keymap;

	if (!imsg || !outChar)
		return FALSE;

	*outChar = 0;

	/* Direct ASCII in Code (normal VANILLAKEY) */
	if (imsg->Code >= 32 && imsg->Code <= 126) {
		*outChar = (UBYTE)imsg->Code;
		return TRUE;
	}

	/*
	 * This host puts raw scan codes in Qualifier when Code is 0.
	 * Map via keymap.library (same approach as test.c RAWKEY path).
	 */
	rawCode = imsg->Code;
	if (rawCode == 0)
		rawCode = imsg->Qualifier;
	if (rawCode & IECODE_UP_PREFIX)
		return FALSE;

	if (!KeymapBase)
		return FALSE;

	keymap = (struct KeyMap *)KeymapBase;
	qual = (ULONG)imsg->Qualifier;
	ievent.ie_Class = IECLASS_RAWKEY;
	ievent.ie_Code = rawCode;
	ievent.ie_Qualifier = (UWORD)qual;
	ievent.ie_SubClass = 0;
	ievent.ie_X = 0;
	ievent.ie_Y = 0;
	ievent.ie_NextEvent = NULL;
	ievent.ie_TimeStamp.tv_secs = 0;
	ievent.ie_TimeStamp.tv_micro = 0;
	ievent.ie_EventAddress = NULL;
	if (imsg->IAddress)
		ievent.ie_EventAddress = (APTR)(*((ULONG *)imsg->IAddress));

	chars = MapRawKey(&ievent, charBuffer, 9, keymap);
	if (chars > 0 && charBuffer[0] >= 32) {
		*outChar = charBuffer[0];
		return TRUE;
	}

	return FALSE;
}

/****************************************************************************/

static BOOL
TTX_IntuiHandleMenuPick(struct TTXApplication *app, struct Session *session,
	struct IntuiMessage *imsg)
{
	ULONG menuCode;
	struct MenuItem *item;
	ULONG menuNum;
	ULONG itemNum;

	if (!app || !session || !imsg || !session->window)
		return FALSE;

	/*
	 * New Look menus: Code is often 0; the pick index is in Qualifier
	 * (e.g. 0xF820). Never call ItemAddress(strip, 0) — that is wrong.
	 */
	menuCode = (ULONG)(UWORD)imsg->Code;
	if (menuCode == 0)
		menuCode = (ULONG)imsg->Qualifier;

	Printf("[INTUI] MENUPICK code=%04x qual=%04x -> pick=%04lx\n",
		(unsigned int)imsg->Code, (unsigned int)imsg->Qualifier,
		menuCode);

	if (menuCode == MENUNULL || menuCode == 0xFFFF) {
		TTX_ResetMenuStrip(session);
		return TRUE;
	}

	while (menuCode != MENUNULL && menuCode != 0xFFFF) {
		item = ItemAddress(session->window->MenuStrip, (UWORD)menuCode);
		if (!item)
			break;

		menuNum = 0;
		itemNum = 0;
		if (!TTX_IntuiDecodeMenuItem(item, &menuNum, &itemNum))
			break;

		Printf("[INTUI] MENUPICK pick=%04lx -> menu=%lu item=%lu\n",
			menuCode, menuNum, itemNum);

		if (!TTX_HandleMenuPick(app, session, menuNum, itemNum))
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

	CalculateMaxScroll(TT_SessionBuffer(session), session->window);
	RenderText(session->window, session);
	UpdateCursor(session->window, session);
}

/****************************************************************************/

static VOID
TTX_IntuiRefreshSession(struct Session *session)
{
	if (!session || !session->window || !TT_SessionBuffer(session))
		return;

	CalculateMaxScroll(TT_SessionBuffer(session), session->window);
	UpdateScrollBars(session);
	RenderText(session->window, session);
	UpdateCursor(session->window, session);
}

/****************************************************************************/

static VOID
TTX_IntuiHandleKey(struct TTXApplication *app, struct Session *session,
	struct IntuiMessage *imsg)
{
	ULONG key;
	ULONG qual;
	STRPTR insArgs[1];
	TEXT chBuf[2];
	BOOL processed;
	UBYTE rawCode;
	struct InputEvent ievent;
	UBYTE charBuffer[10];
	WORD chars;
	struct KeyMap *keymap;

	if (!app || !session || !imsg || !TT_SessionBuffer(session))
		return;
	if (session->document->state.readOnly)
		return;

	key = 0;
	processed = FALSE;
	qual = (ULONG)imsg->Qualifier;

	if (imsg->Class == IDCMP_VANILLAKEY) {
		if (imsg->Code == 0x08) {
			TTX_DoEngineCommand(app, session, "Delete", NULL, 0);
			processed = TRUE;
		} else if (imsg->Code == 0x7F) {
			TTX_DoEngineCommand(app, session, "DeleteForward", NULL, 0);
			processed = TRUE;
		} else if (imsg->Code == 0x0A || imsg->Code == 0x0D) {
			TTX_DoEngineCommand(app, session, "InsertLine", NULL, 0);
			processed = TRUE;
		} else if (imsg->Code == 0x1B) {
			TTX_RequestDestroySession(app, session);
			return;
		} else if (imsg->Code == 0x45 && (qual & IEQUALIFIER_CONTROL)) {
			TTX_HandleCommand(app, session, "SaveFile", NULL, 0);
			processed = TRUE;
		} else if (TTX_IntuiMapKey(imsg, (UBYTE *)&key)) {
			chBuf[0] = (TEXT)key;
			chBuf[1] = '\0';
			insArgs[0] = chBuf;
			TTX_DoEngineCommand(app, session, "Insert", insArgs, 1);
			processed = TRUE;
		}
	} else if (imsg->Class == IDCMP_RAWKEY) {
		rawCode = (UBYTE)imsg->Code;
		if (rawCode == 0)
			rawCode = (UBYTE)((UWORD)imsg->Qualifier & 0xFF);
		if (rawCode & 0x80)
			return;

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
			ievent.ie_Class = IECLASS_RAWKEY;
			ievent.ie_Code = rawCode;
			ievent.ie_Qualifier = (UWORD)qual;
			ievent.ie_SubClass = 0;
			ievent.ie_X = 0;
			ievent.ie_Y = 0;
			ievent.ie_NextEvent = NULL;
			ievent.ie_TimeStamp.tv_secs = 0;
			ievent.ie_TimeStamp.tv_micro = 0;
			ievent.ie_EventAddress = NULL;
			if (imsg->IAddress)
				ievent.ie_EventAddress = (APTR)(*((ULONG *)imsg->IAddress));
			chars = MapRawKey(&ievent, charBuffer, 9, keymap);
			if (chars > 0 && charBuffer[0] >= 32) {
				chBuf[0] = (TEXT)charBuffer[0];
				chBuf[1] = '\0';
				insArgs[0] = chBuf;
				TTX_DoEngineCommand(app, session, "Insert", insArgs, 1);
				processed = TRUE;
			}
		}
	}

	Printf("[INTUI] KEY class=%08lx code=%02x qual=%04x -> processed=%s\n",
		(ULONG)imsg->Class, (unsigned int)imsg->Code,
		(unsigned int)imsg->Qualifier, processed ? "YES" : "NO");

	if (processed) {
		session->document->state.modified = TT_SessionBuffer(session)->modified;
		TTX_IntuiRefreshSession(session);
	}
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
		if (session->document->state.modified) {
			struct IntuiText bodyText;
			struct IntuiText posText;
			struct IntuiText negText;
			BOOL save;
			STRPTR bodyStr;
			STRPTR posStr;
			STRPTR negStr;

			bodyStr = "Document has been modified.\nSave before closing?";
			posStr = "Save";
			negStr = "Cancel";

			bodyText.FrontPen = 0;
			bodyText.BackPen = 1;
			bodyText.DrawMode = JAM2;
			bodyText.LeftEdge = 0;
			bodyText.TopEdge = 0;
			bodyText.ITextFont = NULL;
			bodyText.IText = bodyStr;
			bodyText.NextText = NULL;

			posText.FrontPen = 0;
			posText.BackPen = 1;
			posText.DrawMode = JAM2;
			posText.LeftEdge = 0;
			posText.TopEdge = 0;
			posText.ITextFont = NULL;
			posText.IText = posStr;
			posText.NextText = NULL;

			negText.FrontPen = 0;
			negText.BackPen = 1;
			negText.DrawMode = JAM2;
			negText.LeftEdge = 0;
			negText.TopEdge = 0;
			negText.ITextFont = NULL;
			negText.IText = negStr;
			negText.NextText = NULL;

			save = AutoRequest(session->window, &bodyText, &posText, &negText,
				0, 0, 320, 100);
			if (save) {
				if (session->document->state.fileName)
					TTX_HandleCommand(app, session, "SaveFile", NULL, 0);
				else
					TTX_HandleCommand(app, session, "SaveFileAs", NULL, 0);
				if (session->document->state.modified) {
					result = TRUE;
					break;
				}
			} else {
				result = TRUE;
				break;
			}
		}
		TTX_RequestDestroySession(app, session);
		result = TRUE;
		break;

	case IDCMP_VANILLAKEY:
	case IDCMP_RAWKEY:
		/* Window-level fallback when the text editor gadget is absent. */
		if (!session->textEditorGadget)
			TTX_IntuiHandleKey(app, session, imsg);
		result = TRUE;
		break;

	case IDCMP_REFRESHWINDOW:
		if (TT_SessionBuffer(session)) {
			BeginRefresh(session->window);
			RenderText(session->window, session);
			UpdateCursor(session->window, session);
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
		if (gadgetID == 0)
			gadgetID = (ULONG)imsg->Qualifier;
		if (gadgetID == 0 && imsg->IAddress)
			GetAttr(GA_ID, (Object *)imsg->IAddress, &gadgetID);
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
		if (gadgetID == 0)
			gadgetID = (ULONG)imsg->Qualifier;
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
				if (isPress)
					ActivateWindow(session->window);
				MouseToCursor(TT_SessionBuffer(session), session->window,
					imsg->MouseX, imsg->MouseY,
					&newCursorX, &newCursorY);
				if (TT_SessionBuffer(session) &&
				    newCursorY < TT_SessionBuffer(session)->lineCount) {
					TT_SessionBuffer(session)->cursorY = newCursorY;
					if (newCursorX <= TT_SessionBuffer(session)->lines[
						    newCursorY].length)
						TT_SessionBuffer(session)->cursorX = newCursorX;
					else
						TT_SessionBuffer(session)->cursorX =
							TT_SessionBuffer(session)->lines[
								newCursorY].length;
				}
				if (isPress) {
					session->mouseSelecting = TRUE;
					session->selectStartX = TT_SessionBuffer(session)->cursorX;
					session->selectStartY = TT_SessionBuffer(session)->cursorY;
				} else if (session->mouseSelecting) {
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

			MouseToCursor(TT_SessionBuffer(session), session->window,
				imsg->MouseX, imsg->MouseY,
				&newCursorX, &newCursorY);
			if (TT_SessionBuffer(session) &&
			    newCursorY < TT_SessionBuffer(session)->lineCount) {
				TT_SessionBuffer(session)->cursorY = newCursorY;
				TT_SessionBuffer(session)->cursorX = newCursorX;
			}
			TTX_IntuiRefreshSession(session);
		}
		result = TRUE;
		break;

	case IDCMP_ACTIVEWINDOW:
		app->activeSession = session;
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
		session = session->next;
	}
}

/****************************************************************************/
