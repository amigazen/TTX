/*
 * turbotext.library - engine command dispatch
 *
 * Editing, cursor, file, and block commands per TurboText manual.
 * UI/window commands remain in the TTX driver.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "private/tt_internal.h"

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

	result = TT_HandleEngineCommand(doc, view, command, args, argCount);
	if (!result)
		TT_SetLastError(TTERR_UNKNOWN_COMMAND);

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

	(void)view;
	(void)args;
	(void)argCount;

	if (!doc || !command)
		return FALSE;

	buf = &doc->buffer;

	if (Stricmp(command, "Insert") == 0 && argCount > 0 && args[0])
	{
		ULONG i = 0;
		while (args[0][i] != '\0')
		{
			if (!TT_InsertChar(buf, (UBYTE)args[0][i]))
				return FALSE;
			i++;
		}
		doc->state.modified = TRUE;
		return TRUE;
	}
	if (Stricmp(command, "InsertLine") == 0)
	{
		doc->state.modified = TT_InsertNewline(buf);
		return doc->state.modified;
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
	if (Stricmp(command, "DeleteLine") == 0)
	{
		doc->state.modified = TT_DeleteLine(buf);
		return doc->state.modified;
	}
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
	if (Stricmp(command, "MoveNextWord") == 0)
		return TT_MoveNextWord(buf);
	if (Stricmp(command, "MovePrevWord") == 0)
		return TT_MovePrevWord(buf);
	if (Stricmp(command, "MarkBlk") == 0)
	{
		TT_MarkAllBlock(buf);
		return TRUE;
	}
	if (Stricmp(command, "DeleteBlk") == 0)
	{
		doc->state.modified = TT_DeleteBlock(buf);
		return doc->state.modified;
	}
	if (Stricmp(command, "Conv2Upper") == 0)
	{
		doc->state.modified = TT_ConvertToUpper(buf);
		return doc->state.modified;
	}
	if (Stricmp(command, "Conv2Lower") == 0)
	{
		doc->state.modified = TT_ConvertToLower(buf);
		return doc->state.modified;
	}
	if (Stricmp(command, "Conv2Tabs") == 0)
	{
		doc->state.modified = TT_ConvertSpacesToTabs(buf);
		return doc->state.modified;
	}
	if (Stricmp(command, "Conv2Spaces") == 0)
	{
		doc->state.modified = TT_ConvertTabsToSpaces(buf);
		return doc->state.modified;
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
		if (doc->state.fileName)
			TT_Free(doc->state.fileName);
		doc->state.fileName = TT_DupStr(args[0]);
		if (TT_LoadFile(args[0], buf))
		{
			doc->state.modified = FALSE;
			return TRUE;
		}
		return FALSE;
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
		doc->state.modified = TT_ToggleCharCase(buf);
		return doc->state.modified;
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

	return FALSE;
}
