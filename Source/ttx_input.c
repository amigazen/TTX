/*
 * TTX driver - shared keyboard input (HEAD~1 window IDCMP behaviour via engine)
 *
 * Live prefs (ttxreqs.library): overstrike, expand-tabs, auto-erase selection,
 * free-form pad, auto-indent newlines, word/line wrap at right margin.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 *
 * C89: locals at block start.
 */

#include "ttx_input.h"
#include "ttx_prefs.h"

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
TTX_InputMarkingActive(struct Session *session)
{
	struct TTView *view;

	view = TTX_SessionView(session);
	if (view && view->marking.enabled)
		return TRUE;
	if (TT_SessionBuffer(session) && TT_SessionBuffer(session)->marking.enabled)
		return TRUE;
	return FALSE;
}

static BOOL
TTX_InputEraseSelectionIfNeeded(
	struct TTXApplication *app,
	struct Session *session)
{
	struct TRPrefs *prefs;

	prefs = TR_PrefsGet();
	if (!prefs || !prefs->autoEraseSelectedBlocks)
		return TRUE;
	if (!TTX_InputMarkingActive(session))
		return TRUE;
	return TTX_DoEngineCommand(app, session, "DeleteBlk", NULL, 0);
}

static BOOL
TTX_InputEnsureLineCapacity(struct TTTextLine *line, ULONG needLen)
{
	ULONG newAlloc;
	STRPTR newText;

	if (!line || !line->text)
		return FALSE;
	if (needLen < line->allocated)
		return TRUE;
	newAlloc = line->allocated * 2;
	if (newAlloc < needLen + 64)
		newAlloc = needLen + 256;
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
	return TRUE;
}

/*
 * Free-form: pad spaces from EOL up to caret column before insert.
 */
static BOOL
TTX_InputPadToCursor(struct TTTextLine *line, ULONG targetX)
{
	ULONG i;

	if (!line || !line->text)
		return FALSE;
	if (targetX <= line->length)
		return TRUE;
	if (!TTX_InputEnsureLineCapacity(line, targetX + 1))
		return FALSE;
	for (i = line->length; i < targetX; i++)
		line->text[i] = ' ';
	line->length = targetX;
	line->text[line->length] = '\0';
	return TRUE;
}

static VOID
TTX_InputMarkModified(struct Session *session, struct TTTextBuffer *buf,
	struct TTView *view)
{
	view->lastChangeX = view->cursorX;
	view->lastChangeY = view->cursorY;
	view->lastChangeValid = 1;
	buf->cursorX = view->cursorX;
	buf->cursorY = view->cursorY;
	buf->modified = TRUE;
	if (session->document)
		session->document->state.modified = TRUE;
}

/****************************************************************************/

static BOOL
TTX_InputMaybeWrapLine(
	struct TTXApplication *app,
	struct Session *session,
	struct TTTextBuffer *buf,
	struct TTView *view)
{
	struct TRPrefs *prefs;
	ULONG margin;
	ULONG tabW;
	ULONG col;
	ULONG breakAt;
	ULONG i;
	STRPTR text;
	ULONG len;
	STRPTR indentArgs[1];

	prefs = TR_PrefsGet();
	if (!prefs)
		return FALSE;
	if (!prefs->wordWrap && !prefs->lineWrap)
		return FALSE;

	margin = prefs->rightMargin;
	if (margin < 2)
		margin = 80;
	tabW = TTX_TabWidth(buf);
	if (view->cursorY >= buf->lineCount || !buf->lines)
		return FALSE;
	text = buf->lines[view->cursorY].text;
	len = buf->lines[view->cursorY].length;
	if (!text)
		return FALSE;

	col = TTX_VisualColumn(text, len, tabW);
	if (col <= margin)
		return FALSE;

	/* Find split point: last whitespace before overflow, or hard at margin. */
	breakAt = 0;
	if (prefs->wordWrap) {
		ULONG vis;

		vis = 0;
		for (i = 0; i < len; i++) {
			if (text[i] == '\t')
				vis += tabW - (vis % tabW);
			else
				vis++;
			if (vis > margin)
				break;
			if (text[i] == ' ' || text[i] == '\t')
				breakAt = i + 1;
		}
		if (breakAt == 0 || breakAt >= len) {
			/* No whitespace — hard split at first char past margin. */
			vis = 0;
			for (i = 0; i < len; i++) {
				if (text[i] == '\t')
					vis += tabW - (vis % tabW);
				else
					vis++;
				if (vis > margin) {
					breakAt = i;
					break;
				}
			}
		}
	} else {
		ULONG vis;

		vis = 0;
		for (i = 0; i < len; i++) {
			if (text[i] == '\t')
				vis += tabW - (vis % tabW);
			else
				vis++;
			if (vis > margin) {
				breakAt = i;
				break;
			}
		}
	}

	if (breakAt == 0 || breakAt >= len)
		return FALSE;

	view->cursorX = breakAt;
	buf->cursorX = breakAt;
	buf->cursorY = view->cursorY;

	indentArgs[0] = NULL;
	if (prefs->autoIndentNewLines)
		indentArgs[0] = (STRPTR)"Indent";
	if (indentArgs[0]) {
		if (!TTX_DoEngineCommand(app, session, "InsertLine", indentArgs, 1))
			return FALSE;
	} else {
		if (!TTX_DoEngineCommand(app, session, "InsertLine", NULL, 0))
			return FALSE;
	}
	view = TTX_SessionView(session);
	buf = TT_SessionBuffer(session);
	if (view && buf) {
		view->cursorX = buf->cursorX;
		view->cursorY = buf->cursorY;
	}
	(void)app;
	return TRUE;
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
	struct TRPrefs *prefs;
	ULONG x;
	ULONG i;
	BOOL overstrike;

	/*
	 * Insert directly into the shared TTTextBuffer. Calling library "Insert"
	 * with a FAR data pointer from a DATA=FAR driver into a smalldata
	 * turbotext.library was returning TRUE while writing zero characters.
	 */
	buf = TT_SessionBuffer(session);
	view = TTX_SessionView(session);
	prefs = TR_PrefsGet();
	if (!buf || !view || !buf->lines || view->cursorY >= buf->lineCount)
		return FALSE;
	if (!TTX_InputEraseSelectionIfNeeded(app, session))
		return FALSE;

	/* Selection erase may have changed buffer/view. */
	buf = TT_SessionBuffer(session);
	view = TTX_SessionView(session);
	if (!buf || !view || !buf->lines || view->cursorY >= buf->lineCount)
		return FALSE;

	line = &buf->lines[view->cursorY];
	if (!line->text)
		return FALSE;

	x = view->cursorX;
	if (prefs && prefs->freeForm) {
		if (x > line->length) {
			if (!TTX_InputPadToCursor(line, x))
				return FALSE;
		}
	} else if (x > line->length) {
		x = line->length;
		view->cursorX = x;
	}

	overstrike = (prefs && prefs->overstrike && x < line->length) ? TRUE : FALSE;

	if (overstrike) {
		line->text[x] = (TEXT)ch;
		view->cursorX = x + 1;
	} else {
		if (!TTX_InputEnsureLineCapacity(line, line->length + 2))
			return FALSE;
		for (i = line->length; i > x; i--)
			line->text[i] = line->text[i - 1];
		line->text[x] = (TEXT)ch;
		line->length++;
		line->text[line->length] = '\0';
		view->cursorX = x + 1;
	}

	TTX_InputMarkModified(session, buf, view);
	TTX_InputMaybeWrapLine(app, session, buf, view);
	return TRUE;
}

/****************************************************************************/

static BOOL
TTX_InputInsertTab(
	struct TTXApplication *app,
	struct Session *session)
{
	struct TRPrefs *prefs;
	struct TTTextBuffer *buf;
	struct TTView *view;
	ULONG tabW;
	ULONG col;
	ULONG spaces;
	ULONG s;

	prefs = TR_PrefsGet();
	if (!prefs || !prefs->expandTabs)
		return TTX_InputInsertChar(app, session, '\t');

	buf = TT_SessionBuffer(session);
	view = TTX_SessionView(session);
	if (!buf || !view)
		return FALSE;
	tabW = prefs->tabWidth;
	if (tabW < 1)
		tabW = TTX_TabWidth(buf);
	if (view->cursorY >= buf->lineCount || !buf->lines)
		return FALSE;
	col = TTX_VisualColumn(buf->lines[view->cursorY].text, view->cursorX, tabW);
	spaces = tabW - (col % tabW);
	if (spaces == 0)
		spaces = tabW;
	for (s = 0; s < spaces; s++) {
		if (!TTX_InputInsertChar(app, session, ' '))
			return FALSE;
	}
	return TRUE;
}

/****************************************************************************/

static BOOL
TTX_InputInsertLine(
	struct TTXApplication *app,
	struct Session *session)
{
	struct TRPrefs *prefs;
	STRPTR args[1];

	prefs = TR_PrefsGet();
	if (!TTX_InputEraseSelectionIfNeeded(app, session))
		return FALSE;
	args[0] = NULL;
	if (prefs && prefs->autoIndentNewLines)
		args[0] = (STRPTR)"Indent";
	if (args[0])
		return TTX_DoEngineCommand(app, session, "InsertLine", args, 1);
	return TTX_DoEngineCommand(app, session, "InsertLine", NULL, 0);
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
			if (TTX_InputInsertLine(app, session))
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
	 * Some hosts deliver Code=0 with the character in Qualifier's low byte.
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
		if (TTX_InputInsertTab(app, session))
			processed = TRUE;
	} else if (keyCode == 0x08) {
		if (TR_PrefsGet() && TR_PrefsGet()->autoEraseSelectedBlocks &&
		    TTX_InputMarkingActive(session)) {
			if (TTX_DoEngineCommand(app, session, "DeleteBlk", NULL, 0))
				processed = TRUE;
		} else if (TTX_DoEngineCommand(app, session, "Delete", NULL, 0)) {
			processed = TRUE;
		}
	} else if (keyCode == 0x7F) {
		if (TR_PrefsGet() && TR_PrefsGet()->autoEraseSelectedBlocks &&
		    TTX_InputMarkingActive(session)) {
			if (TTX_DoEngineCommand(app, session, "DeleteBlk", NULL, 0))
				processed = TRUE;
		} else if (TTX_DoEngineCommand(app, session, "DeleteForward", NULL, 0)) {
			processed = TRUE;
		}
	} else if (keyCode == 0x0A || keyCode == 0x0D) {
		if (TTX_InputInsertLine(app, session))
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
	struct TRPrefs *prefs;
	struct InputEvent ievent;
	UBYTE charBuffer[10];
	WORD chars;
	struct KeyMap *keymap;
	BOOL processed;
	BOOL freeForm;

	if (!app || !session || !TT_SessionBuffer(session))
		return FALSE;

	buffer = TT_SessionBuffer(session);
	view = TTX_SessionView(session);
	prefs = TR_PrefsGet();
	freeForm = (prefs && prefs->freeForm) ? TRUE : FALSE;
	processed = FALSE;

	if (!buffer || !view)
		return FALSE;

	if (rawCode & 0x80)
		return FALSE;

	/*
	 * DFN KEYBOARD / HOT_KEYS bindings (when a .dfn was loaded for menus)
	 * override the built-in rawkey handlers below.
	 */
	if (TTX_DFNTryKeyCommand(app, session, rawCode, qualifier))
		return TRUE;

	if (!session->document || session->document->state.readOnly)
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
		if (freeForm ||
		    view->cursorX < buffer->lines[view->cursorY].length) {
			view->cursorX++;
		} else if (view->cursorY < buffer->lineCount - 1) {
			view->cursorY++;
			view->cursorX = 0;
		}
		processed = TRUE;
	} else if (rawCode == 0x4C) {
		if (view->cursorY > 0) {
			view->cursorY--;
			if (!freeForm &&
			    view->cursorX > buffer->lines[view->cursorY].length)
				view->cursorX = buffer->lines[view->cursorY].length;
		}
		processed = TRUE;
	} else if (rawCode == 0x4D) {
		if (view->cursorY < buffer->lineCount - 1) {
			view->cursorY++;
			if (!freeForm &&
			    view->cursorX > buffer->lines[view->cursorY].length)
				view->cursorX = buffer->lines[view->cursorY].length;
		}
		processed = TRUE;
	} else if (rawCode == 0x41) {
		if (prefs && prefs->autoEraseSelectedBlocks &&
		    TTX_InputMarkingActive(session)) {
			if (TTX_DoEngineCommand(app, session, "DeleteBlk", NULL, 0))
				processed = TRUE;
		} else if (TTX_DoEngineCommand(app, session, "Delete", NULL, 0)) {
			processed = TRUE;
		}
	} else if (rawCode == 0x46) {
		if (prefs && prefs->autoEraseSelectedBlocks &&
		    TTX_InputMarkingActive(session)) {
			if (TTX_DoEngineCommand(app, session, "DeleteBlk", NULL, 0))
				processed = TRUE;
		} else if (TTX_DoEngineCommand(app, session, "DeleteForward", NULL, 0)) {
			processed = TRUE;
		}
	} else if (rawCode == 0x43 || rawCode == 0x44) {
		if (TTX_InputInsertLine(app, session))
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

	if (processed) {
		buffer = TT_SessionBuffer(session);
		view = TTX_SessionView(session);
		if (buffer && view) {
			buffer->cursorX = view->cursorX;
			buffer->cursorY = view->cursorY;
		}
		if (session->document && buffer)
			session->document->state.modified = buffer->modified;
	}

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
