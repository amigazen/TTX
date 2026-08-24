/*
 * turbotext.library - engine command dispatch
 *
 * Editing, cursor, file, and block commands per TurboText manual.
 * UI/window commands remain in the TTX driver.
 *
 * Push/pull wrappers around TT_HandleEngineCommand (done in TT_DoCommandI)
 * mean that all text ops here may read and write buf->cursorX/Y as the
 * canonical position; TT_PullViewFromBuffer propagates changes back to the
 * TTView after return.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "private/tt_internal.h"

/*
 * One-line undelete ring.  Populated by the DeleteLine command handler so
 * that UndeleteLine / UndoLine can restore the most recently deleted line.
 * Static storage is safe here because the library runs in a single task
 * context on AmigaOS (library functions are single-threaded through the
 * standard Forbid/Permit gate).
 */
static STRPTR s_undeleteLine = NULL;
static ULONG  s_undeleteLen  = 0;
/*
 * MarkBlk is two-step (anchor, then end). After a finished mark - or a mark
 * left by Conv2* - the next MarkBlk clears and starts a new anchor so ARexx
 * / menus never extend a stale selection across the whole buffer.
 */
static BOOL s_markAwaitingEnd = FALSE;

/****************************************************************************/

/*
 * Parse an unsigned decimal integer from a NUL-terminated string.
 * Returns 0 for a NULL pointer or a string that starts with a non-digit.
 */
static ULONG
TT_ParseULong(STRPTR s)
{
	ULONG val = 0;
	ULONG i   = 0;

	if (!s)
		return 0;

	while (s[i] >= '0' && s[i] <= '9')
	{
		val = val * 10 + (ULONG)(s[i] - '0');
		i++;
	}
	return val;
}

/****************************************************************************/

BOOL
TT_DoCommandI(
	struct TurboTextBase *base,
	struct TTDocument *doc,
	struct TTView *view,
	STRPTR command,
	STRPTR *args,
	ULONG argCount)
{
	BOOL result = FALSE;

	(void)base;

	if (!doc || !command)
	{
		TT_SetLastError(TTERR_NO_DOCUMENT);
		return FALSE;
	}

	if (!view)
		view = doc->activeView;

	/* Engine text ops use buffer mirrors; view is canonical. */
	if (view)
		TT_PushViewToBuffer(view, &doc->buffer);

	result = TT_HandleEngineCommand(doc, view, command, args, argCount);

	if (view)
	{
		TT_PullViewFromBuffer(view, &doc->buffer);
		if (doc->buffer.modified)
		{
			view->lastChangeX = view->cursorX;
			view->lastChangeY = view->cursorY;
			view->lastChangeValid = 1;
		}
	}

	/*
	 * Do not stamp TTERR_UNKNOWN_COMMAND on every FALSE return - many
	 * commands return FALSE for empty find, no mark, SOF, etc. Unknown
	 * is set only at the end of TT_HandleEngineCommand when no match.
	 */
	return result;
}

/****************************************************************************/

BOOL
TT_HandleEngineCommand(
	struct TTDocument *doc,
	struct TTView *view,
	STRPTR command,
	STRPTR *args,
	ULONG argCount)
{
	struct TTTextBuffer *buf = NULL;

	if (!doc || !command)
		return FALSE;

	if (!view)
		view = doc->activeView;

	buf = &doc->buffer;

	/* ------------------------------------------------------------------ */
	/* Chunk A - basic insert / delete (existing) */

	if (Stricmp(command, "Insert") == 0 && argCount > 0 && args[0])
	{
		ULONG a = 0;
		ULONG i = 0;

		/*
		 * ARexx often delivers Insert "a b c" as Insert a b c (argCount>1).
		 * Join args with spaces so the full phrase is inserted.
		 */
		for (a = 0; a < argCount && args[a]; a++)
		{
			if (a > 0)
			{
				if (!TT_InsertChar(buf, (UBYTE)' '))
					return FALSE;
			}
			i = 0;
			while (args[a][i] != '\0')
			{
				if (!TT_InsertChar(buf, (UBYTE)args[a][i]))
					return FALSE;
				i++;
			}
		}
		doc->state.modified = TRUE;
		return TRUE;
	}
	if (Stricmp(command, "InsertLine") == 0)
	{
		BOOL doIndent;
		ULONG ai;
		ULONG indentLen;
		ULONG prevY;
		STRPTR prevText;
		struct TTTextLine *newLn;
		ULONG newAlloc;
		STRPTR nt;
		ULONG k;

		doIndent = FALSE;
		indentLen = 0;
		prevText = NULL;
		newLn = NULL;
		nt = NULL;

		if (doc->state.readOnly)
			return FALSE;

		for (ai = 0; ai < argCount; ai++) {
			if (args[ai] && Stricmp(args[ai], "Indent") == 0)
				doIndent = TRUE;
		}

		prevY = buf->cursorY;
		if (!TT_InsertNewline(buf))
			return FALSE;

		if (doIndent && prevY < buf->lineCount &&
		    buf->cursorY < buf->lineCount &&
		    buf->lines[prevY].text) {
			prevText = buf->lines[prevY].text;
			indentLen = 0;
			while (indentLen < buf->lines[prevY].length &&
			       (prevText[indentLen] == ' ' ||
			        prevText[indentLen] == '\t'))
				indentLen++;
			if (indentLen > 0) {
				newLn = &buf->lines[buf->cursorY];
				if (!newLn->text)
					indentLen = 0;
			}
			if (indentLen > 0 && newLn) {
				if (indentLen + newLn->length + 1 > newLn->allocated) {
					newAlloc = indentLen + newLn->length + 256;
					nt = (STRPTR)TT_Alloc(newAlloc, MEMF_CLEAR);
					if (!nt) {
						indentLen = 0;
					} else {
						if (newLn->length > 0)
							CopyMem(newLn->text, nt + indentLen,
								newLn->length);
						TT_Free(newLn->text);
						newLn->text = nt;
						newLn->allocated = newAlloc;
					}
				} else if (newLn->length > 0) {
					for (k = newLn->length; k > 0; k--)
						newLn->text[k - 1 + indentLen] =
							newLn->text[k - 1];
				}
			}
			if (indentLen > 0 && newLn && newLn->text) {
				CopyMem(prevText, newLn->text, indentLen);
				newLn->length += indentLen;
				newLn->text[newLn->length] = '\0';
				buf->cursorX = indentLen;
			}
		}
		doc->state.modified = TRUE;
		return TRUE;
	}
	if (Stricmp(command, "Delete") == 0)
	{
		doc->state.modified = TT_DeleteChar(buf);
		return doc->state.modified;
	}
	if (Stricmp(command, "DeleteForward") == 0)
	{
		doc->state.modified = TT_DeleteForward(buf);
		return doc->state.modified;
	}
	if (Stricmp(command, "DeleteEOL") == 0)
	{
		struct TTTextLine *ln = NULL;
		ULONG startX = 0;
		ULONG delLen = 0;

		if (doc->state.readOnly)
			return FALSE;
		if (buf->cursorY < buf->lineCount)
		{
			ln = &buf->lines[buf->cursorY];
			startX = buf->cursorX;
			if (startX < ln->length)
			{
				delLen = ln->length - startX;
				if (s_undeleteLine)
				{
					TT_Free(s_undeleteLine);
					s_undeleteLine = NULL;
					s_undeleteLen = 0;
				}
				s_undeleteLen = delLen;
				s_undeleteLine = (STRPTR)TT_Alloc(delLen + 1, MEMF_CLEAR);
				if (s_undeleteLine && ln->text)
				{
					CopyMem(&ln->text[startX], s_undeleteLine, delLen);
					s_undeleteLine[delLen] = '\0';
				}
			}
		}
		doc->state.modified = TT_DeleteEOL(buf);
		return doc->state.modified;
	}
	if (Stricmp(command, "DeleteEOW") == 0)
	{
		doc->state.modified = TT_DeleteEOW(buf);
		return doc->state.modified;
	}
	if (Stricmp(command, "DeleteSOL") == 0)
	{
		doc->state.modified = TT_DeleteSOL(buf);
		return doc->state.modified;
	}
	if (Stricmp(command, "DeleteSOW") == 0)
	{
		doc->state.modified = TT_DeleteSOW(buf);
		return doc->state.modified;
	}

	/*
	 * DeleteLine - save the line content into the undelete ring before
	 * calling TT_DeleteLine so that UndeleteLine can restore it.
	 */
	if (Stricmp(command, "DeleteLine") == 0)
	{
		struct TTTextLine *ln = NULL;

		if (buf->cursorY < buf->lineCount)
		{
			ln = &buf->lines[buf->cursorY];

			/* Free any previously saved line */
			if (s_undeleteLine)
			{
				TT_Free(s_undeleteLine);
				s_undeleteLine = NULL;
				s_undeleteLen  = 0;
			}

			s_undeleteLen  = ln->length;
			s_undeleteLine = (STRPTR)TT_Alloc(s_undeleteLen + 1, MEMF_CLEAR);
			if (s_undeleteLine)
			{
				if (ln->text && ln->length > 0)
					CopyMem(ln->text, s_undeleteLine, s_undeleteLen);
				s_undeleteLine[s_undeleteLen] = '\0';
			}
		}

		doc->state.modified = TT_DeleteLine(buf);
		return doc->state.modified;
	}

	/* ------------------------------------------------------------------ */
	/* Chunk A - cursor movement (existing) */

	if (Stricmp(command, "MoveLeft") == 0)
	{
		if (buf->cursorX > 0)
			buf->cursorX--;
		else if (buf->cursorY > 0)
		{
			buf->cursorY--;
			buf->cursorX = buf->lines[buf->cursorY].length;
		}
		return TRUE;
	}
	if (Stricmp(command, "MoveRight") == 0)
	{
		if (buf->cursorX < buf->lines[buf->cursorY].length)
			buf->cursorX++;
		else if (buf->cursorY + 1 < buf->lineCount)
		{
			buf->cursorY++;
			buf->cursorX = 0;
		}
		return TRUE;
	}
	if (Stricmp(command, "MoveUp") == 0)
	{
		if (buf->cursorY > 0)
		{
			buf->cursorY--;
			if (buf->cursorX > buf->lines[buf->cursorY].length)
				buf->cursorX = buf->lines[buf->cursorY].length;
		}
		return TRUE;
	}
	if (Stricmp(command, "MoveDown") == 0)
	{
		if (buf->cursorY + 1 < buf->lineCount)
		{
			buf->cursorY++;
			if (buf->cursorX > buf->lines[buf->cursorY].length)
				buf->cursorX = buf->lines[buf->cursorY].length;
		}
		return TRUE;
	}
	if (Stricmp(command, "MoveSOL") == 0)
		return TT_MoveStartOfLine(buf);
	if (Stricmp(command, "MoveEOL") == 0)
		return TT_MoveEndOfLine(buf);
	if (Stricmp(command, "MovePrevWord") == 0)
	{
		TT_MovePrevWord(buf);
		return TRUE;
	}
	if (Stricmp(command, "MoveNextWord") == 0)
	{
		TT_MoveNextWord(buf);
		return TRUE;
	}

	/* ------------------------------------------------------------------ */
	/* Chunk A - block / case / shift / file (existing) */

	if (Stricmp(command, "MarkBlk") == 0)
	{
		ULONG ai;

		/*
		 * First MarkBlk: start a selection at the cursor. Second: set the
		 * stop corner. Third (or MarkBlk after Conv2x/Copy left a mark):
		 * clear and start fresh. Never MarkAll - that made ARexx paste
		 * duplicate the whole report.
		 */
		for (ai = 0; ai < argCount; ai++) {
			if (!args[ai])
				continue;
			if (Stricmp(args[ai], "Off") == 0 ||
			    Stricmp(args[ai], "Clear") == 0 ||
			    Stricmp(args[ai], "Cancel") == 0) {
				TT_ClearMarking(buf);
				s_markAwaitingEnd = FALSE;
				return TRUE;
			}
		}

		if (buf->marking.enabled && !s_markAwaitingEnd) {
			TT_ClearMarking(buf);
		}

		if (!buf->marking.enabled)
		{
			buf->marking.enabled = TRUE;
			buf->marking.startY = buf->cursorY;
			buf->marking.startX = buf->cursorX;
			buf->marking.stopY = buf->cursorY;
			if (buf->cursorY < buf->lineCount)
				buf->marking.stopX = buf->lines[buf->cursorY].length;
			else
				buf->marking.stopX = buf->cursorX;
			s_markAwaitingEnd = TRUE;
		}
		else
		{
			buf->marking.stopY = buf->cursorY;
			buf->marking.stopX = buf->cursorX;
			s_markAwaitingEnd = FALSE;
		}
		return TRUE;
	}
	if (Stricmp(command, "DeleteBlk") == 0)
	{
		doc->state.modified = TT_DeleteBlock(buf);
		s_markAwaitingEnd = FALSE;
		return doc->state.modified;
	}
	if (Stricmp(command, "Conv2Upper") == 0)
	{
		BOOL tempMark;

		if (doc->state.readOnly)
			return FALSE;
		tempMark = FALSE;
		/* No mark: convert the current line, then drop the temp mark. */
		if (!buf->marking.enabled && buf->cursorY < buf->lineCount)
		{
			TT_SetMarking(buf, buf->cursorY, 0, buf->cursorY,
				buf->lines[buf->cursorY].length);
			tempMark = TRUE;
		}
		doc->state.modified = TT_ConvertToUpper(buf);
		if (tempMark) {
			TT_ClearMarking(buf);
			s_markAwaitingEnd = FALSE;
		}
		return doc->state.modified;
	}
	if (Stricmp(command, "Conv2Lower") == 0)
	{
		BOOL tempMark;

		if (doc->state.readOnly)
			return FALSE;
		tempMark = FALSE;
		if (!buf->marking.enabled && buf->cursorY < buf->lineCount)
		{
			TT_SetMarking(buf, buf->cursorY, 0, buf->cursorY,
				buf->lines[buf->cursorY].length);
			tempMark = TRUE;
		}
		doc->state.modified = TT_ConvertToLower(buf);
		if (tempMark) {
			TT_ClearMarking(buf);
			s_markAwaitingEnd = FALSE;
		}
		return doc->state.modified;
	}
	if (Stricmp(command, "Conv2Tabs") == 0)
	{
		ULONG tabSize = 8;
		LONG n = 8;
		BOOL changed;

		if (argCount > 0 && args[0] && StrToLong(args[0], &n) && n > 0)
			tabSize = (ULONG)n;
		/* Succeed even when no space-run became a tab (e.g. mark elsewhere). */
		changed = TT_ConvertSpacesToTabsEx(buf, tabSize);
		if (changed) {
			doc->state.modified = TRUE;
			buf->modified = TRUE;
		}
		return TRUE;
	}
	if (Stricmp(command, "Conv2Spaces") == 0)
	{
		ULONG tabSize = 8;
		LONG n = 8;

		if (argCount > 0 && args[0] && StrToLong(args[0], &n) && n > 0)
			tabSize = (ULONG)n;
		/* Always succeed; Ex sets buf->modified only when tabs expanded. */
		(void)TT_ConvertTabsToSpacesEx(buf, tabSize);
		if (buf->modified)
			doc->state.modified = TRUE;
		return TRUE;
	}
	if (Stricmp(command, "ShiftLeft") == 0)
	{
		doc->state.modified = TT_ShiftLeft(buf);
		return doc->state.modified;
	}
	if (Stricmp(command, "ShiftRight") == 0)
	{
		doc->state.modified = TT_ShiftRight(buf);
		return doc->state.modified;
	}
	if (Stricmp(command, "SaveFile") == 0)
	{
		if (doc->state.fileName)
		{
			if (TT_SaveFile(doc->state.fileName, buf))
			{
				doc->state.modified = FALSE;
				buf->modified = FALSE;
				return TRUE;
			}
		}
		return FALSE;
	}
	if (Stricmp(command, "OpenFile") == 0 && argCount > 0 && args[0])
	{
		struct TTView *v = NULL;

		if (doc->state.fileName)
			TT_Free(doc->state.fileName);
		doc->state.fileName = TT_DupStr(args[0]);
		if (TT_LoadFile(args[0], buf))
		{
			doc->state.modified = FALSE;
			/* New file: reset every view's caret, scroll, and marks */
			for (v = doc->views; v; v = v->next)
			{
				v->cursorX          = 0;
				v->cursorY          = 0;
				v->scrollX          = 0;
				v->scrollY          = 0;
				v->marking.enabled  = FALSE;
				v->autoMarkValid    = 0;
				v->lastChangeValid  = 0;
			}
			if (view)
				TT_PushViewToBuffer(view, buf);
			return TRUE;
		}
		return FALSE;
	}
	/*
	 * ClearFile - drop all lines except an empty first line. Engine owns
	 * line text (TT_Alloc), so free with TT_Free and zero vacated slots.
	 */
	if (Stricmp(command, "ClearFile") == 0)
	{
		struct TTView *v = NULL;
		ULONG i = 0;

		if (doc->state.readOnly)
			return FALSE;
		if (!buf->lines || buf->lineCount == 0)
			return FALSE;

		for (i = 1; i < buf->lineCount; i++)
		{
			if (buf->lines[i].text)
			{
				TT_Free(buf->lines[i].text);
				buf->lines[i].text = NULL;
			}
			buf->lines[i].length = 0;
			buf->lines[i].allocated = 0;
		}
		buf->lineCount = 1;
		if (buf->lines[0].text)
			buf->lines[0].text[0] = '\0';
		buf->lines[0].length = 0;
		buf->marking.enabled = FALSE;
		s_markAwaitingEnd = FALSE;
		TT_LineUndoClear(buf);
		buf->cursorX = 0;
		buf->cursorY = 0;
		buf->scrollX = 0;
		buf->scrollY = 0;
		buf->modified = TRUE;
		doc->state.modified = TRUE;

		for (v = doc->views; v; v = v->next)
		{
			v->cursorX = 0;
			v->cursorY = 0;
			v->scrollX = 0;
			v->scrollY = 0;
			v->marking.enabled = FALSE;
		}
		if (view)
			TT_PushViewToBuffer(view, buf);
		return TRUE;
	}
	if (Stricmp(command, "SetReadOnly") == 0)
	{
		if (argCount > 0 && args[0] && Stricmp(args[0], "Toggle") == 0)
			doc->state.readOnly = !doc->state.readOnly;
		else if (argCount > 0 && args[0])
			doc->state.readOnly = (Stricmp(args[0], "On") == 0 ||
				Stricmp(args[0], "TRUE") == 0);
		return TRUE;
	}
	if (Stricmp(command, "ToggleCharCase") == 0)
	{
		if (doc->state.readOnly)
			return FALSE;
		/* No-op success when cursor is not on a letter. */
		if (!TT_ToggleCharCase(buf))
			return TRUE;
		doc->state.modified = TRUE;
		return TRUE;
	}
	if (Stricmp(command, "SwapChars") == 0)
	{
		doc->state.modified = TT_SwapChars(buf);
		return doc->state.modified;
	}
	if (Stricmp(command, "InsertFile") == 0 && argCount > 0 && args[0])
	{
		struct TTTextBuffer tempBuffer;
		ULONG savedCursorX = 0;
		ULONG savedCursorY = 0;
		ULONG i = 0;
		ULONG j = 0;
		BOOL ok = TRUE;

		if (doc->state.readOnly)
			return FALSE;

		savedCursorX = buf->cursorX;
		savedCursorY = buf->cursorY;

		if (!TT_InitTextBuffer(&tempBuffer))
			return FALSE;

		if (!TT_LoadFile(args[0], &tempBuffer))
		{
			TT_FreeTextBuffer(&tempBuffer);
			return FALSE;
		}

		for (i = 0; i < tempBuffer.lineCount && ok; i++)
		{
			if (i > 0)
			{
				if (!TT_InsertNewline(buf))
				{
					ok = FALSE;
					break;
				}
			}

			if (tempBuffer.lines[i].text && tempBuffer.lines[i].length > 0)
			{
				for (j = 0; j < tempBuffer.lines[i].length; j++)
				{
					if (!TT_InsertChar(buf, (UBYTE)tempBuffer.lines[i].text[j]))
					{
						ok = FALSE;
						break;
					}
				}
			}
		}

		TT_FreeTextBuffer(&tempBuffer);

		if (!ok)
			return FALSE;

		buf->cursorX = savedCursorX;
		buf->cursorY = savedCursorY;
		doc->state.modified = TRUE;
		buf->modified = TRUE;
		return TRUE;
	}

	/* ================================================================== */
	/* Chunk C - search                                                     */
	/* ================================================================== */

	/*
	 * Find args[0]=search string
	 *
	 * Case-sensitive forward search from one position past the cursor.
	 * On success the cursor advances to the match and view->autoMark is
	 * set to the same position so the driver can highlight it.
	 */
	if (Stricmp(command, "Find") == 0 && argCount > 0 && args[0])
	{
		ULONG matchY = 0;
		ULONG matchX = 0;
		BOOL ignoreCase = TRUE;
		BOOL wholeWords = FALSE;
		BOOL scanBack = FALSE;
		ULONG ai = 0;

		for (ai = 1; ai < argCount; ai++) {
			if (!args[ai])
				continue;
			if (Stricmp(args[ai], "CaseSensitive") == 0)
				ignoreCase = FALSE;
			else if (Stricmp(args[ai], "IgnoreCase") == 0)
				ignoreCase = TRUE;
			else if (Stricmp(args[ai], "WholeWord") == 0 ||
				 Stricmp(args[ai], "WholeWords") == 0)
				wholeWords = TRUE;
			else if (Stricmp(args[ai], "Backward") == 0 ||
				 Stricmp(args[ai], "Backwards") == 0)
				scanBack = TRUE;
		}

		if (!TT_FindTextAtEx(buf, args[0], &matchY, &matchX, TRUE,
				     ignoreCase, wholeWords, scanBack))
			return FALSE;

		buf->cursorY = matchY;
		buf->cursorX = matchX;

		if (view)
		{
			view->autoMarkX     = matchX;
			view->autoMarkY     = matchY;
			view->autoMarkValid = 1;
		}
		return TRUE;
	}

	/*
	 * FindChange args[0]=find  args[1]=replace
	 *
	 * Locates the next match of args[0] forward from the cursor, deletes
	 * it, and inserts args[1] in its place.  The cursor ends up after the
	 * replacement text and autoMark is set to the insertion point.
	 */
	if (Stricmp(command, "FindChange") == 0 && argCount > 1 && args[0] && args[1])
	{
		ULONG matchY      = 0;
		ULONG matchX      = 0;
		ULONG searchLen   = 0;
		ULONG replaceLen  = 0;
		ULONG i           = 0;
		BOOL ignoreCase = TRUE;
		BOOL wholeWords = FALSE;
		BOOL scanBack = FALSE;
		ULONG ai = 0;

		if (doc->state.readOnly)
			return FALSE;

		while (args[0][searchLen] != '\0')
			searchLen++;
		while (args[1][replaceLen] != '\0')
			replaceLen++;

		for (ai = 2; ai < argCount; ai++) {
			if (!args[ai])
				continue;
			if (Stricmp(args[ai], "CaseSensitive") == 0)
				ignoreCase = FALSE;
			else if (Stricmp(args[ai], "IgnoreCase") == 0)
				ignoreCase = TRUE;
			else if (Stricmp(args[ai], "WholeWord") == 0 ||
				 Stricmp(args[ai], "WholeWords") == 0)
				wholeWords = TRUE;
			else if (Stricmp(args[ai], "Backward") == 0 ||
				 Stricmp(args[ai], "Backwards") == 0)
				scanBack = TRUE;
		}

		/* Inclusive: replace the match under the cursor after Find. */
		if (!TT_FindTextAtEx(buf, args[0], &matchY, &matchX, FALSE,
				     ignoreCase, wholeWords, scanBack))
			return FALSE;

		buf->cursorY = matchY;
		buf->cursorX = matchX;

		/* Mark and delete the found text */
		TT_SetMarking(buf, matchY, matchX, matchY, matchX + searchLen);
		if (!TT_DeleteBlock(buf))
			return FALSE;

		/* Insert replacement at the now-empty match position */
		for (i = 0; i < replaceLen; i++)
		{
			if (!TT_InsertChar(buf, (UBYTE)args[1][i]))
			{
				doc->state.modified = TRUE;
				buf->modified = TRUE;
				return FALSE;
			}
		}

		if (view)
		{
			view->autoMarkX     = matchX;
			view->autoMarkY     = matchY;
			view->autoMarkValid = 1;
		}
		doc->state.modified = TRUE;
		buf->modified = TRUE;
		return TRUE;
	}

	/* ================================================================== */
	/* Chunk C - bookmarks and named positions                             */
	/* ================================================================== */

	/*
	 * SetBookmark args[0]=1..10
	 *
	 * Stores the current cursor position in the indexed bookmark slot.
	 * The manual uses 1-based numbers; we map them to 0-based array
	 * indices internally.  An args[0] value of 0 is also accepted and
	 * stored at slot 0 for convenience.
	 */
	if (Stricmp(command, "SetBookmark") == 0 && argCount > 0 && args[0])
	{
		ULONG idx = 0;

		if (!view)
			return FALSE;

		idx = TT_ParseULong(args[0]);
		/* 1-based input: convert to 0-based; 0 input stays at slot 0 */
		if (idx > 0)
			idx--;
		if (idx >= TT_MAX_BOOKMARKS)
			idx = TT_MAX_BOOKMARKS - 1;

		view->bookmarkX[idx]   = buf->cursorX;
		view->bookmarkY[idx]   = buf->cursorY;
		view->bookmarkSet[idx] = 1;
		return TRUE;
	}

	/*
	 * MoveBookmark args[0]=1..10
	 *
	 * Jumps the cursor to the stored bookmark.  Returns FALSE if the
	 * bookmark has not been set.
	 */
	if (Stricmp(command, "MoveBookmark") == 0 && argCount > 0 && args[0])
	{
		ULONG idx = 0;
		ULONG destY = 0;
		ULONG destX = 0;

		if (!view)
			return FALSE;

		idx = TT_ParseULong(args[0]);
		if (idx > 0)
			idx--;
		if (idx >= TT_MAX_BOOKMARKS)
			idx = TT_MAX_BOOKMARKS - 1;

		if (!view->bookmarkSet[idx])
			return FALSE;

		destY = view->bookmarkY[idx];
		destX = view->bookmarkX[idx];

		/* Clamp to current buffer dimensions */
		if (destY >= buf->lineCount)
			destY = buf->lineCount > 0 ? buf->lineCount - 1 : 0;
		if (destX > buf->lines[destY].length)
			destX = buf->lines[destY].length;

		buf->cursorY = destY;
		buf->cursorX = destX;
		return TRUE;
	}

	/*
	 * ClearBookmark args[0]=1..10
	 *
	 * Marks the bookmark slot as unset without altering the cursor.
	 */
	if (Stricmp(command, "ClearBookmark") == 0 && argCount > 0 && args[0])
	{
		ULONG idx = 0;

		if (!view)
			return FALSE;

		idx = TT_ParseULong(args[0]);
		if (idx > 0)
			idx--;
		if (idx >= TT_MAX_BOOKMARKS)
			idx = TT_MAX_BOOKMARKS - 1;

		view->bookmarkSet[idx] = 0;
		return TRUE;
	}

	/*
	 * MoveAutomark
	 *
	 * Jumps to view->autoMarkX/Y if the autoMark is valid (set by a
	 * previous Find / FindChange / Replace operation).
	 */
	if (Stricmp(command, "MoveAutomark") == 0)
	{
		ULONG destY = 0;
		ULONG destX = 0;

		/* No autoMark yet: stay put (ARexx treats as success). */
		if (!view || !view->autoMarkValid)
			return TRUE;

		destY = view->autoMarkY;
		destX = view->autoMarkX;

		if (destY >= buf->lineCount)
			destY = buf->lineCount > 0 ? buf->lineCount - 1 : 0;
		if (destX > buf->lines[destY].length)
			destX = buf->lines[destY].length;

		buf->cursorY = destY;
		buf->cursorX = destX;
		return TRUE;
	}

	/*
	 * MoveLastChange
	 *
	 * Jumps to the position stored in view->lastChangeX/Y (set after
	 * every modifying command that goes through TT_DoCommandI).
	 */
	if (Stricmp(command, "MoveLastChange") == 0)
	{
		ULONG destY = 0;
		ULONG destX = 0;

		if (!view || !view->lastChangeValid)
			return TRUE;

		destY = view->lastChangeY;
		destX = view->lastChangeX;

		if (destY >= buf->lineCount)
			destY = buf->lineCount > 0 ? buf->lineCount - 1 : 0;
		if (destX > buf->lines[destY].length)
			destX = buf->lines[destY].length;

		buf->cursorY = destY;
		buf->cursorX = destX;
		return TRUE;
	}

	/*
	 * Move args[0]=line  [args[1]=column]
	 *
	 * Jumps to an absolute 1-based line and optional 1-based column.
	 * Values are clamped to the valid buffer range; a line of 0 is
	 * treated as 1.
	 */
	if (Stricmp(command, "Move") == 0 && argCount > 0 && args[0])
	{
		ULONG targetLine = 0;
		ULONG targetCol  = 0;
		ULONG newY       = 0;
		ULONG newX       = 0;

		targetLine = TT_ParseULong(args[0]);
		targetCol  = (argCount > 1 && args[1]) ? TT_ParseULong(args[1]) : 1;

		/* Convert to 0-based; a value of 0 clamps to line 0 */
		newY = (targetLine > 0) ? targetLine - 1 : 0;
		newX = (targetCol  > 0) ? targetCol  - 1 : 0;

		if (newY >= buf->lineCount)
			newY = buf->lineCount > 0 ? buf->lineCount - 1 : 0;
		if (newX > buf->lines[newY].length)
			newX = buf->lines[newY].length;

		buf->cursorY = newY;
		buf->cursorX = newX;
		return TRUE;
	}

	/*
	 * GetCursorPos is a driver ARexx RESULT command - do not claim it here
	 * (returning TRUE would skip TTX_Cmd_GetCursorPos and leave RESULT empty).
	 */

	/*
	 * MoveNextTabStop
	 *
	 * Advances cursorX to the next multiple of TT_DEFAULT_TAB_WIDTH,
	 * clamped to the end of the current line.
	 */
	if (Stricmp(command, "MoveNextTabStop") == 0)
	{
		ULONG tabWidth  = TT_DEFAULT_TAB_WIDTH;
		ULONG nextStop  = 0;
		ULONG lineLen   = 0;

		lineLen  = buf->lines[buf->cursorY].length;
		nextStop = ((buf->cursorX / tabWidth) + 1) * tabWidth;
		if (nextStop > lineLen)
			nextStop = lineLen;
		buf->cursorX = nextStop;
		return TRUE;
	}

	/*
	 * MovePrevTabStop
	 *
	 * Retreats cursorX to the previous multiple of TT_DEFAULT_TAB_WIDTH.
	 * Never goes below column 0.
	 */
	if (Stricmp(command, "MovePrevTabStop") == 0)
	{
		ULONG tabWidth  = TT_DEFAULT_TAB_WIDTH;
		ULONG prevStop  = 0;

		if (buf->cursorX == 0)
			return TRUE; /* already at leftmost tab stop */

		/*
		 * If the cursor is exactly on a tab boundary, step one tab
		 * back; otherwise round down to the current boundary.
		 */
		if (buf->cursorX % tabWidth == 0)
			prevStop = buf->cursorX - tabWidth;
		else
			prevStop = (buf->cursorX / tabWidth) * tabWidth;

		buf->cursorX = prevStop;
		return TRUE;
	}

	/*
	 * MoveMatchBkt
	 *
	 * Finds the bracket character that matches the one at the cursor
	 * (one of () [] {}) and moves the cursor there.  Nesting is handled
	 * correctly.  Searches forward for openers and backward for closers.
	 * Returns FALSE if the cursor is not on a bracket or no match exists.
	 */
	if (Stricmp(command, "MoveMatchBkt") == 0)
	{
		UBYTE ch       = 0;
		UBYTE openBkt  = 0;
		UBYTE closeBkt = 0;
		BOOL  goFwd    = FALSE;
		LONG  depth    = 0;
		BOOL  found    = FALSE;
		ULONG scanY    = 0;
		ULONG scanX    = 0;

		ch = TT_GetCharAtCursor(buf);

		if (ch == '(' || ch == '[' || ch == '{')
		{
			openBkt  = ch;
			closeBkt = (ch == '(') ? ')' : (ch == '[') ? ']' : '}';
			goFwd    = TRUE;
		}
		else if (ch == ')' || ch == ']' || ch == '}')
		{
			closeBkt = ch;
			openBkt  = (ch == ')') ? '(' : (ch == ']') ? '[' : '{';
			goFwd    = FALSE;
		}
		else
			return FALSE;

		depth = 1;
		found = FALSE;

		if (goFwd)
		{
			/* Scan forward from the column after the opener */
			ULONG startX = buf->cursorX + 1;
			UBYTE c      = 0;

			for (scanY = buf->cursorY; scanY < buf->lineCount && !found; scanY++)
			{
				scanX = (scanY == buf->cursorY) ? startX : 0;
				for (; scanX < buf->lines[scanY].length && !found; scanX++)
				{
					c = (UBYTE)buf->lines[scanY].text[scanX];
					if (c == openBkt)
						depth++;
					else if (c == closeBkt)
					{
						depth--;
						if (depth == 0)
						{
							buf->cursorY = scanY;
							buf->cursorX = scanX;
							found = TRUE;
						}
					}
				}
			}
		}
		else
		{
			/* Scan backward from one column before the closer */
			LONG  backX = (LONG)buf->cursorX - 1;
			UBYTE c     = 0;

			scanY = buf->cursorY;
			for (;;)
			{
				while (backX >= 0 && !found)
				{
					c = (UBYTE)buf->lines[scanY].text[backX];
					if (c == closeBkt)
						depth++;
					else if (c == openBkt)
					{
						depth--;
						if (depth == 0)
						{
							buf->cursorY = scanY;
							buf->cursorX = (ULONG)backX;
							found = TRUE;
						}
					}
					backX--;
				}
				if (found || scanY == 0)
					break;
				scanY--;
				backX = (LONG)buf->lines[scanY].length - 1;
			}
		}

		return found;
	}

	/* ================================================================== */
	/* Chunk C - edit extras                                               */
	/* ================================================================== */

	/*
	 * UndeleteLine - re-insert the last DeleteLine/DeleteEOL scrap as a
	 * new line at the cursor (Extras/Undelete Line).
	 */
	if (Stricmp(command, "UndeleteLine") == 0)
	{
		ULONG savedY = 0;
		ULONG i      = 0;

		if (!s_undeleteLine)
			return FALSE;

		if (doc->state.readOnly)
			return FALSE;

		savedY = buf->cursorY;

		buf->cursorX = 0;
		if (!TT_InsertNewline(buf))
			return FALSE;

		buf->cursorY = savedY;
		buf->cursorX = 0;

		for (i = 0; i < s_undeleteLen; i++)
		{
			if (!TT_InsertChar(buf, (UBYTE)s_undeleteLine[i]))
			{
				doc->state.modified = TRUE;
				buf->modified = TRUE;
				return FALSE;
			}
		}

		buf->cursorX = 0;
		doc->state.modified = TRUE;
		buf->modified = TRUE;
		return TRUE;
	}

	/*
	 * UndoLine - restore the last edited line to its pre-edit snapshot
	 * (Extras/Undo Line).  A second UndoLine swaps back (redo).
	 */
	if (Stricmp(command, "UndoLine") == 0)
	{
		if (doc->state.readOnly)
			return FALSE;
		if (!TT_UndoLine(buf))
			return FALSE;
		doc->state.modified = TRUE;
		buf->modified = TRUE;
		return TRUE;
	}

	/*
	 * Center
	 *
	 * Strips surrounding spaces from the current line and re-centers the
	 * content within the page width (pageW or 72 columns).
	 */
	if (Stricmp(command, "Center") == 0)
	{
		if (doc->state.readOnly)
			return FALSE;
		doc->state.modified = TT_CenterLine(buf);
		return doc->state.modified;
	}

	/*
	 * Justify
	 *
	 * Expands inter-word spaces on the current line so that it exactly
	 * fills the page width (pageW or 72 columns).
	 */
	if (Stricmp(command, "Justify") == 0)
	{
		if (doc->state.readOnly)
			return FALSE;
		doc->state.modified = TT_JustifyLine(buf);
		return doc->state.modified;
	}

	/*
	 * FormatParagraph
	 *
	 * Re-flows the paragraph around the cursor to the page width.
	 * A paragraph is a run of non-blank lines bounded by blank lines or
	 * the buffer edges.
	 */
	if (Stricmp(command, "FormatParagraph") == 0)
	{
		if (doc->state.readOnly)
			return FALSE;
		doc->state.modified = TT_FormatParagraph(buf);
		return doc->state.modified;
	}

	/*
	 * GetWord
	 *
	 * Returns TRUE when a word exists at the cursor; the engine has no
	 * string-return channel so callers that need the actual text should
	 * use TT_GetWordAtCursor() directly via the library API.  The
	 * allocated string is freed immediately here.
	 */
	if (Stricmp(command, "GetWord") == 0)
	{
		STRPTR word = TT_GetWordAtCursor(buf);
		if (!word)
			return FALSE;
		TT_Free(word);
		return TRUE;
	}

	/*
	 * ReplaceWord args[0]=new-word
	 *
	 * Replaces the word at the cursor with args[0].
	 */
	if (Stricmp(command, "ReplaceWord") == 0 && argCount > 0 && args[0])
	{
		if (doc->state.readOnly)
			return FALSE;
		doc->state.modified = TT_ReplaceWordAtCursor(buf, args[0]);
		return doc->state.modified;
	}

	/* ================================================================== */
	/* Chunk C - file metadata                                             */
	/* ================================================================== */

	/*
	 * GetFilePath is a driver ARexx RESULT command - do not claim it here.
	 */

	/*
	 * SetFilePath args[0]=path
	 *
	 * Updates doc->state.fileName.  Does not load or save the file;
	 * use OpenFile / SaveFile for that.
	 */
	if (Stricmp(command, "SetFilePath") == 0 && argCount > 0 && args[0])
	{
		if (doc->state.fileName)
			TT_Free(doc->state.fileName);
		doc->state.fileName = TT_DupStr(args[0]);
		return (BOOL)(doc->state.fileName != NULL);
	}

	/* ================================================================== */
	/* Chunk D - multi-view (Split / Switch / Swap / Center)               */
	/* ================================================================== */

	/*
	 * SplitView - ensure a second TTView exists on this document.
	 * Driver owns the on-screen split ratio; engine only manages views.
	 */
	if (Stricmp(command, "SplitView") == 0)
	{
		struct TTView *nv = NULL;

		/*
		 * Prefer the linked list over viewCount alone - a stale count
		 * must not skip creating the second pane view.
		 */
		if (!doc->views || !doc->views->next || doc->viewCount < 2)
		{
			nv = TT_CreateView(doc);
			if (!nv)
				return FALSE;
			if (view)
			{
				nv->cursorX = view->cursorX;
				nv->cursorY = view->cursorY;
				nv->scrollX = view->scrollX;
				nv->scrollY = view->scrollY;
			}
			TT_ActivateViewI(doc, nv);
		}
		TT_SetLastError(TTERR_NONE);
		return TRUE;
	}

	/* SwitchView - cycle to the next view in the document's view list. */
	if (Stricmp(command, "SwitchView") == 0)
	{
		struct TTView *cur = NULL;
		struct TTView *next = NULL;
		struct TTView *nv = NULL;

		if (!doc->views || !doc->views->next)
		{
			nv = TT_CreateView(doc);
			if (!nv)
				return FALSE;
			if (view)
			{
				nv->cursorX = view->cursorX;
				nv->cursorY = view->cursorY;
				nv->scrollX = view->scrollX;
				nv->scrollY = view->scrollY;
			}
		}

		cur = view ? view : doc->activeView;
		if (!cur)
			cur = doc->views;
		if (!cur)
			return FALSE;

		next = cur->next;
		if (!next)
			next = doc->views;
		if (!next)
			return FALSE;

		TT_ActivateViewI(doc, next);
		TT_SetLastError(TTERR_NONE);
		return TRUE;
	}

	/* SwapViews - exchange caret/scroll between the first two views. */
	if (Stricmp(command, "SwapViews") == 0)
	{
		struct TTView *a = NULL;
		struct TTView *b = NULL;
		struct TTView *nv = NULL;
		ULONG tx = 0;
		ULONG ty = 0;
		ULONG sx = 0;
		ULONG sy = 0;

		if (!doc->views || !doc->views->next)
		{
			nv = TT_CreateView(doc);
			if (!nv)
				return FALSE;
			if (view)
			{
				nv->cursorX = view->cursorX;
				nv->cursorY = view->cursorY;
				nv->scrollX = view->scrollX;
				nv->scrollY = view->scrollY;
			}
		}
		a = doc->views;
		if (!a || !a->next)
			return FALSE;
		b = a->next;
		tx = a->cursorX; ty = a->cursorY;
		sx = a->scrollX; sy = a->scrollY;
		a->cursorX = b->cursorX; a->cursorY = b->cursorY;
		a->scrollX = b->scrollX; a->scrollY = b->scrollY;
		b->cursorX = tx; b->cursorY = ty;
		b->scrollX = sx; b->scrollY = sy;
		TT_SetLastError(TTERR_NONE);
		return TRUE;
	}

	/* CenterView - scroll so the caret is mid-pane (pageH from buffer). */
	if (Stricmp(command, "CenterView") == 0)
	{
		ULONG half = 0;

		if (!view)
			return FALSE;
		half = buf->pageH / 2;
		if (half < 1)
			half = 1;
		if (view->cursorY > half)
			view->scrollY = view->cursorY - half;
		else
			view->scrollY = 0;
		if (view->scrollY > buf->maxScrollY)
			view->scrollY = buf->maxScrollY;
		buf->scrollY = view->scrollY;
		return TRUE;
	}

	TT_SetLastError(TTERR_UNKNOWN_COMMAND);
	return FALSE;
}
