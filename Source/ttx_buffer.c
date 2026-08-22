/*
 * TTX driver - local text buffer helpers
 *
 * Read-only / simple cursor helpers on the engine-owned TTTextBuffer.
 * The driver does not link turbotext.library; editing goes through TT_DoCommand.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_driver.h"

UBYTE GetCharAtCursor(struct TTTextBuffer *buffer)
{
	if (!buffer || buffer->cursorY >= buffer->lineCount)
		return 0;

	if (buffer->cursorX < buffer->lines[buffer->cursorY].length)
		return (UBYTE)buffer->lines[buffer->cursorY].text[buffer->cursorX];

	return (UBYTE)'\n';
}

STRPTR GetCurrentLine(struct TTTextBuffer *buffer)
{
	STRPTR lineText = NULL;
	ULONG lineLen = 0;

	

	if (!buffer || buffer->cursorY >= buffer->lineCount)
		return NULL;

	lineLen = buffer->lines[buffer->cursorY].length;
	lineText = (STRPTR)TTX_Alloc(lineLen + 1, MEMF_CLEAR);
	if (!lineText)
		return NULL;

	if (lineLen > 0 && buffer->lines[buffer->cursorY].text)
		CopyMem(buffer->lines[buffer->cursorY].text, lineText, lineLen);

	lineText[lineLen] = '\0';
	return lineText;
}

BOOL SetCharAtCursor(struct TTTextBuffer *buffer, UBYTE ch)
{
	if (!buffer || buffer->cursorY >= buffer->lineCount)
		return FALSE;

	if (buffer->cursorX < buffer->lines[buffer->cursorY].length &&
	    buffer->lines[buffer->cursorY].text)
	{
		buffer->lines[buffer->cursorY].text[buffer->cursorX] = (TEXT)ch;
		buffer->modified = TRUE;
		return TRUE;
	}

	return FALSE;
}
