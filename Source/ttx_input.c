/*
 * TTX driver - shared keyboard input (HEAD~1 ttx.c behaviour via engine)
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_input.h"

#include <proto/keymap.h>

/****************************************************************************/

VOID
TTX_InputRefreshSession(struct Session *session)
{
	if (!session || !session->window || !TT_SessionBuffer(session))
		return;

	CalculateMaxScroll(TT_SessionBuffer(session), session->window);
	ScrollToCursor(TT_SessionBuffer(session), session->window);
	UpdateScrollBars(session);
	RenderText(session->window, session);
	UpdateCursor(session->window, session);
}

/****************************************************************************/

static BOOL
TTX_InputInsertChar(
	struct TTXApplication *app,
	struct Session *session,
	UBYTE ch)
{
	STRPTR insArgs[1];
	TEXT chBuf[2];

	chBuf[0] = (TEXT)ch;
	chBuf[1] = '\0';
	insArgs[0] = chBuf;
	return TTX_DoEngineCommand(app, session, "Insert", insArgs, 1);
}

/****************************************************************************/

static BOOL
TTX_InputInsertMapped(
	struct TTXApplication *app,
	struct Session *session,
	UBYTE *charBuffer,
	WORD chars)
{
	ULONG i;
	BOOL processed;

	processed = FALSE;
	if (chars <= 0)
		return FALSE;

	charBuffer[chars] = '\0';
	for (i = 0; i < (ULONG)chars; i++) {
		if (charBuffer[i] >= 0x20 && charBuffer[i] < 0x7F) {
			if (TTX_InputInsertChar(app, session, charBuffer[i]))
				processed = TRUE;
		} else if (charBuffer[i] == 0x0A || charBuffer[i] == 0x0D) {
			if (TTX_DoEngineCommand(app, session, "InsertLine", NULL, 0))
				processed = TRUE;
		}
	}
	return processed;
}

/****************************************************************************/

BOOL
TTX_InputVanillaKey(
	struct TTXApplication *app,
	struct Session *session,
	UWORD code,
	ULONG qualifier,
	APTR iaddr)
{
	UBYTE keyCode;
	BOOL processed;

	if (!app || !session || !TT_SessionBuffer(session))
		return FALSE;
	if (session->document->state.readOnly)
		return FALSE;

	keyCode = (UBYTE)code;
	processed = FALSE;

	/* Arrow keys as VANILLAKEY — ignore; expect RAWKEY */
	if (keyCode == 0x1C || keyCode == 0x1D || keyCode == 0x1E || keyCode == 0x1F)
		return FALSE;

	if ((keyCode >= 27 && keyCode <= 126) || (keyCode >= 128 && keyCode <= 255)) {
		if (TTX_InputInsertChar(app, session, keyCode))
			processed = TRUE;
	} else if (keyCode == 0x08) {
		if (TTX_DoEngineCommand(app, session, "Delete", NULL, 0))
			processed = TRUE;
	} else if (keyCode == 0x7F) {
		if (TTX_DoEngineCommand(app, session, "DeleteForward", NULL, 0))
			processed = TRUE;
	} else if (keyCode == 0x0A || keyCode == 0x0D) {
		if (TTX_DoEngineCommand(app, session, "InsertLine", NULL, 0))
			processed = TRUE;
	} else if (keyCode == 0x1B) {
		TTX_RequestDestroySession(app, session);
		return TRUE;
	} else if (keyCode == 0x45 && (qualifier & IEQUALIFIER_CONTROL)) {
		if (TTX_HandleCommand(app, session, "SaveFile", NULL, 0))
			processed = TRUE;
	} else if (keyCode == 0 && qualifier != 0) {
		processed = TTX_InputRawKey(app, session, (UBYTE)qualifier, qualifier,
			iaddr);
	}

	if (processed && session->document)
		session->document->state.modified = TT_SessionBuffer(session)->modified;

	return processed;
}

/****************************************************************************/

BOOL
TTX_InputRawKey(
	struct TTXApplication *app,
	struct Session *session,
	UBYTE rawCode,
	ULONG qualifier,
	APTR iaddr)
{
	struct TTTextBuffer *buffer;
	struct InputEvent ievent;
	UBYTE charBuffer[10];
	WORD chars;
	struct KeyMap *keymap;
	BOOL processed;

	if (!app || !session || !TT_SessionBuffer(session))
		return FALSE;
	if (session->document->state.readOnly)
		return FALSE;

	buffer = TT_SessionBuffer(session);
	processed = FALSE;

	if (rawCode & 0x80)
		return FALSE;

	if (rawCode >= 0x60 && rawCode <= 0x67)
		return FALSE;

	if (rawCode >= 0x68 && rawCode <= 0x6A)
		return FALSE;

	if (rawCode == 0x4F) {
		if (buffer->cursorX > 0)
			buffer->cursorX--;
		else if (buffer->cursorY > 0) {
			buffer->cursorY--;
			buffer->cursorX = buffer->lines[buffer->cursorY].length;
		}
		processed = TRUE;
	} else if (rawCode == 0x4E) {
		if (buffer->cursorX < buffer->lines[buffer->cursorY].length)
			buffer->cursorX++;
		else if (buffer->cursorY < buffer->lineCount - 1) {
			buffer->cursorY++;
			buffer->cursorX = 0;
		}
		processed = TRUE;
	} else if (rawCode == 0x4C) {
		if (buffer->cursorY > 0) {
			buffer->cursorY--;
			if (buffer->cursorX > buffer->lines[buffer->cursorY].length)
				buffer->cursorX = buffer->lines[buffer->cursorY].length;
		}
		processed = TRUE;
	} else if (rawCode == 0x4D) {
		if (buffer->cursorY < buffer->lineCount - 1) {
			buffer->cursorY++;
			if (buffer->cursorX > buffer->lines[buffer->cursorY].length)
				buffer->cursorX = buffer->lines[buffer->cursorY].length;
		}
		processed = TRUE;
	} else if (rawCode == 0x46) {
		if (TTX_DoEngineCommand(app, session, "DeleteForward", NULL, 0))
			processed = TRUE;
	} else if (KeymapBase) {
		keymap = AskKeyMapDefault();
		if (keymap) {
			ievent.ie_Class = IECLASS_RAWKEY;
			ievent.ie_SubClass = 0;
			ievent.ie_Code = rawCode;
			ievent.ie_Qualifier = (UWORD)(qualifier &
				~(IEQUALIFIER_CAPSLOCK | IEQUALIFIER_RELATIVEMOUSE));
			ievent.ie_X = 0;
			ievent.ie_Y = 0;
			ievent.ie_NextEvent = NULL;
			ievent.ie_TimeStamp.tv_secs = 0;
			ievent.ie_TimeStamp.tv_micro = 0;
			if (iaddr)
				ievent.ie_EventAddress = (APTR)(*((ULONG *)iaddr));
			else
				ievent.ie_EventAddress = NULL;

			chars = MapRawKey(&ievent, charBuffer,
				(WORD)(sizeof(charBuffer) - 1), keymap);
			if (chars > 0 && chars < (WORD)(sizeof(charBuffer) - 1)) {
				if (TTX_InputInsertMapped(app, session, charBuffer, chars))
					processed = TRUE;
			}
		}
	}

	if (processed && session->document)
		session->document->state.modified = buffer->modified;

	return processed;
}

/****************************************************************************/

BOOL
TTX_InputFromIntuiMessage(
	struct TTXApplication *app,
	struct Session *session,
	struct IntuiMessage *imsg)
{
	UBYTE rawCode;
	BOOL processed;
	APTR iaddr;

	if (!app || !session || !imsg)
		return FALSE;

	processed = FALSE;
	iaddr = NULL;
	if (imsg->IAddress)
		iaddr = (APTR)(*((ULONG *)imsg->IAddress));

	if (imsg->Class == IDCMP_VANILLAKEY) {
		processed = TTX_InputVanillaKey(app, session, imsg->Code,
			(ULONG)imsg->Qualifier, iaddr);
	} else if (imsg->Class == IDCMP_RAWKEY) {
		rawCode = (UBYTE)imsg->Code;
		if (rawCode == 0)
			rawCode = (UBYTE)((UWORD)imsg->Qualifier & 0xFF);
		processed = TTX_InputRawKey(app, session, rawCode,
			(ULONG)imsg->Qualifier, iaddr);
	}

	Printf("[INTUI] KEY class=%08lx code=%02x qual=%04x -> processed=%s\n",
		(ULONG)imsg->Class, (unsigned int)imsg->Code,
		(unsigned int)imsg->Qualifier, processed ? "YES" : "NO");

	if (processed)
		TTX_InputRefreshSession(session);

	return processed;
}

/****************************************************************************/

BOOL
TTX_InputFromInputEvent(
	struct TTXApplication *app,
	struct Session *session,
	struct InputEvent *ievent,
	APTR iaddr)
{
	UBYTE rawCode;
	BOOL processed;

	if (!app || !session || !ievent)
		return FALSE;

	processed = FALSE;

	if (ievent->ie_Class == IECLASS_RAWKEY) {
		rawCode = (UBYTE)ievent->ie_Code;
		if (rawCode == 0)
			rawCode = (UBYTE)((UWORD)ievent->ie_Qualifier & 0xFF);
		processed = TTX_InputRawKey(app, session, rawCode,
			(ULONG)ievent->ie_Qualifier, iaddr);
	}

	if (processed)
		TTX_InputRefreshSession(session);

	return processed;
}

/****************************************************************************/
