/*
 * TTX - Text Editor for AmigaOS (driver)
 *
 * Thin compatibility header - driver-local buffer helpers and legacy aliases.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TTX_H
#define TTX_H

#include "ttx_driver.h"

typedef struct TTTextBuffer TextBuffer;
typedef struct TTTextLine TextLine;
typedef struct TTTextMarking TextMarking;
typedef struct TTDocumentState DocumentState;

/* Driver-local buffer helpers (ttx_block.c, ttx_buffer.c) */
STRPTR GetBlock(struct TTTextBuffer *buffer);
BOOL DeleteBlock(struct TTTextBuffer *buffer);
VOID MarkAllBlock(struct TTTextBuffer *buffer);
VOID SetMarking(
	struct TTTextBuffer *buffer,
	ULONG startY, ULONG startX, ULONG stopY, ULONG stopX);
BOOL MoveNextWord(struct TTTextBuffer *buffer);
BOOL MovePrevWord(struct TTTextBuffer *buffer);
BOOL MoveEndOfLine(struct TTTextBuffer *buffer);
BOOL MoveStartOfLine(struct TTTextBuffer *buffer);
UBYTE GetCharAtCursor(struct TTTextBuffer *buffer);
STRPTR GetCurrentLine(struct TTTextBuffer *buffer);
BOOL SetCharAtCursor(struct TTTextBuffer *buffer, UBYTE ch);

#endif /* TTX_H */
