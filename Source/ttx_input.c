/*
 * TTX driver - shared keyboard input (HEAD~1 window IDCMP behaviour via engine)
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
	struct TTTextBuffer *buf;
	struct TTView *view;
	ULONG oldScrollX;
	ULONG oldScrollY;
	ULONG oldLineCount;
	ULONG cursorLine;

	if (!session || !session->window || !TT_SessionBuffer(session))
		return;

	buf = TT_SessionBuffer(session);
	view = TTX_SessionView(session);
	if (!view)
		return;

	oldScrollX = view->scrollX;
	oldScrollY = view->scrollY;
	oldLineCount = buf->lineCount;
	cursorLine = view->cursorY;

	CalculateMaxScroll(session, session->window);
	ScrollToCursor(session, session->window);

	/*
	 * Typing one character must not RectFill the whole text area or poke
	 * prop gadgets every time — that is the flicker. Full redraw only when
	 * scroll position or line count changes (newline, delete-line, etc.).
	 */
	if (view->scrollX != oldScrollX || view->scrollY != oldScrollY ||
	    buf->lineCount != oldLineCount) {
		UpdateScrollBars(session);
		TTX_RequestRedraw(session);
	} else {
		TTX_RequestLineRedraw(session, cursorLine);
	}
}

/****************************************************************************/

static BOOL
TTX_InputInsertChar(
	struct TTXApplication *app,
	struct Session *session,
	UBYTE ch)
{
	struct TTTextBuffer *buf;
	struct TTView *view;
	struct TTTextLine *line;
	ULONG x;
	ULONG i;
	ULONG newAlloc;
	STRPTR newText;

	(void)app;

	/*
	 * Insert directly into the shared TTTextBuffer. Calling library "Insert"
	 * with a FAR data pointer from a DATA=FAR driver into a smalldata
	 * turbotext.library was returning TRUE while writing zero characters
	 * (processed=1, buffer unchanged, cursor flickers, no glyphs).
	 */
	buf = TT_SessionBuffer(session);
	view = TTX_SessionView(session);
	if (!buf || !view || !buf->lines || view->cursorY >= buf->lineCount)
		return FALSE;

	line = &buf->lines[view->cursorY];
	if (!line->text)
		return FALSE;

	x = view->cursorX;
	if (x > line->length)
		x = line->length;

	if (line->length + 1 >= line->allocated) {
		newAlloc = line->allocated * 2;
		if (newAlloc < 256)
			newAlloc = 256;
		newText = (STRPTR)TTX_Alloc(newAlloc, MEMF_CLEAR);
		if (!newText)
			return FALSE;
		if (line->length > 0)
			CopyMem(line->text, newText, line->length);
		TTX_Free(line->text);
		line->text = newText;
		line->allocated = newAlloc;
	}

	for (i = line->length; i > x; i--)
		line->text[i] = line->text[i - 1];
	line->text[x] = (TEXT)ch;
	line->length++;
	line->text[line->length] = '\0';
	view->cursorX = x + 1;
	view->lastChangeX = view->cursorX;
	view->lastChangeY = view->cursorY;
	view->lastChangeValid = 1;
	buf->cursorX = view->cursorX;
	buf->cursorY = view->cursorY;
	buf->modified = TRUE;
	if (session->document)
		session->document->state.modified = TRUE;

	Printf("[INPUT] Insert ch=%lu len=%lu x=%lu\n",
		(ULONG)ch, line->length, view->cursorX);
	return TRUE;
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

	(void)iaddr;

	if (!app || !session || !TT_SessionBuffer(session))
		return FALSE;
	if (!session->document || session->document->state.readOnly)
		return FALSE;

	/*
	 * HEAD~1 / OS3 VANILLAKEY: Code holds the ASCII character when non-zero.
	 * Some hosts deliver Code=0 with the character in Qualifier's low byte
	 * (seen on this target: Class=VANILLAKEY Code=0 Qual='j').
	 */
	keyCode = (UBYTE)code;
	if (keyCode == 0)
		keyCode = (UBYTE)(qualifier & 0xFF);

	processed = FALSE;

	/* Arrow keys as VANILLAKEY — ignore; expect RAWKEY */
	if (keyCode == 0x1C || keyCode == 0x1D || keyCode == 0x1E || keyCode == 0x1F)
		return FALSE;

  if ((keyCode >= 32 && keyCode <= 126) || keyCode >= 128) {
		if (TTX_InputInsertChar(app, session, keyCode))
			processed = TRUE;
	} else if (keyCode == 0x09) {
		/* Tab: store real tab char; display expands to tab stops. */
		if (TTX_InputInsertChar(app, session, '\t'))
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
	struct TTView *view;
	struct InputEvent ievent;
	UBYTE charBuffer[10];
	WORD chars;
	struct KeyMap *keymap;
	BOOL processed;

	if (!app || !session || !TT_SessionBuffer(session))
		return FALSE;
	if (!session->document || session->document->state.readOnly)
		return FALSE;

	buffer = TT_SessionBuffer(session);
	view = TTX_SessionView(session);
	processed = FALSE;

	if (!buffer || !view)
		return FALSE;

	if (rawCode & 0x80)
		return FALSE;

	if (rawCode == 0x4F) {
		if (view->cursorX > 0)
			view->cursorX--;
		else if (view->cursorY > 0) {
			view->cursorY--;
			view->cursorX = buffer->lines[view->cursorY].length;
		}
		processed = TRUE;
	} else if (rawCode == 0x4E) {
		if (view->cursorX < buffer->lines[view->cursorY].length)
			view->cursorX++;
		else if (view->cursorY < buffer->lineCount - 1) {
			view->cursorY++;
			view->cursorX = 0;
		}
		processed = TRUE;
	} else if (rawCode == 0x4C) {
		if (view->cursorY > 0) {
			view->cursorY--;
			if (view->cursorX > buffer->lines[view->cursorY].length)
				view->cursorX = buffer->lines[view->cursorY].length;
		}
		processed = TRUE;
	} else if (rawCode == 0x4D) {
		if (view->cursorY < buffer->lineCount - 1) {
			view->cursorY++;
			if (view->cursorX > buffer->lines[view->cursorY].length)
				view->cursorX = buffer->lines[view->cursorY].length;
		}
		processed = TRUE;
	} else if (rawCode == 0x41) {
		if (TTX_DoEngineCommand(app, session, "Delete", NULL, 0))
			processed = TRUE;
	} else if (rawCode == 0x46) {
		if (TTX_DoEngineCommand(app, session, "DeleteForward", NULL, 0))
			processed = TRUE;
	} else if (rawCode == 0x43 || rawCode == 0x44) {
		if (TTX_DoEngineCommand(app, session, "InsertLine", NULL, 0))
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
			ievent.ie_EventAddress = iaddr;

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
	UWORD code;
	ULONG qual;
	ULONG classId;
	UBYTE rawCode;
	UBYTE ch;
	BOOL processed;
	APTR deadKeyAddr;

	if (!app || !session || !imsg)
		return FALSE;

	processed = FALSE;
	classId = imsg->Class;
	code = imsg->Code;
	qual = (ULONG)imsg->Qualifier;
	/*
	 * Dead-key pointer must be captured by the caller BEFORE ReplyMsg.
	 * After ReplyMsg, IAddress is no longer safe to dereference.
	 */
	deadKeyAddr = imsg->IAddress;

	/* VANILLAKEY (0x00200000): character already translated by Intuition */
	if (classId == IDCMP_VANILLAKEY || classId == 0x00200000UL) {
		ch = (UBYTE)code;
		if (ch == 0)
			ch = (UBYTE)(qual & 0xFF);
		Printf("[INPUT] VANILLA code=%lu qual=%lu ch=%lu\n",
			(ULONG)code, qual, (ULONG)ch);
		processed = TTX_InputVanillaKey(app, session, (UWORD)ch, qual,
			deadKeyAddr);
	} else if (classId == IDCMP_RAWKEY || classId == 0x00000400UL) {
		rawCode = (UBYTE)code;
		if (rawCode == 0) {
			ch = (UBYTE)(qual & 0xFF);
			Printf("[INPUT] RAW code0 qual=%lu ch=%lu\n", qual, (ULONG)ch);
			if (ch >= 32 && ch < 127)
				processed = TTX_InputVanillaKey(app, session, (UWORD)ch,
					qual, deadKeyAddr);
			else
				processed = TTX_InputRawKey(app, session, ch, qual,
					deadKeyAddr);
		} else {
			Printf("[INPUT] RAW code=%lu qual=%lu\n",
				(ULONG)rawCode, qual);
			processed = TTX_InputRawKey(app, session, rawCode, qual,
				deadKeyAddr);
		}
	}

	Printf("[INPUT] processed=%lu\n", (ULONG)(processed ? 1 : 0));

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
	UBYTE ascii;
	BOOL processed;

	if (!app || !session || !ievent)
		return FALSE;

	processed = FALSE;

	if (ievent->ie_Class == IECLASS_RAWKEY) {
		rawCode = (UBYTE)ievent->ie_Code;
		if (rawCode & IECODE_UP_PREFIX)
			return FALSE;
		if (rawCode == 0) {
			ascii = (UBYTE)((UWORD)ievent->ie_Qualifier & 0xFF);
			if (ascii >= 32 && ascii < 127) {
				processed = TTX_InputVanillaKey(app, session, ascii,
					(ULONG)ievent->ie_Qualifier, iaddr);
			} else {
				processed = TTX_InputRawKey(app, session, ascii,
					(ULONG)ievent->ie_Qualifier, iaddr);
			}
		} else {
			processed = TTX_InputRawKey(app, session, rawCode,
				(ULONG)ievent->ie_Qualifier, iaddr);
		}
	}

	if (processed)
		TTX_InputRefreshSession(session);

	return processed;
}

/****************************************************************************/
