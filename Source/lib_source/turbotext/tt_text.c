/*
 * turbotext.library - text buffer and editing (engine)
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "private/tt_internal.h"

#define INITIAL_BUFFER_SIZE 16384

/*
 * Single-line undo (Extras/Undo Line). Kept in the library — not on
 * TTTextBuffer — so driver/library struct layouts stay compatible.
 */
static STRPTR s_lineUndoText = NULL;
static ULONG s_lineUndoLen = 0;
static ULONG s_lineUndoY = 0;
static BOOL s_lineUndoHave = FALSE;

VOID
TT_LineUndoClear(struct TTTextBuffer *buffer)
{
	(void)buffer;
	if (s_lineUndoText) {
		TT_Free(s_lineUndoText);
		s_lineUndoText = NULL;
	}
	s_lineUndoLen = 0;
	s_lineUndoY = 0;
	s_lineUndoHave = FALSE;
}

VOID
TT_LineUndoTouch(struct TTTextBuffer *buffer)
{
	struct TTTextLine *ln;
	ULONG len;

	if (!buffer || !buffer->lines || buffer->cursorY >= buffer->lineCount)
		return;
	if (s_lineUndoHave && s_lineUndoY == buffer->cursorY)
		return;

	ln = &buffer->lines[buffer->cursorY];
	if (!ln->text)
		return;
	len = ln->length;
	if (s_lineUndoText)
		TT_Free(s_lineUndoText);
	s_lineUndoText = (STRPTR)TT_Alloc(len + 1, MEMF_CLEAR);
	if (!s_lineUndoText) {
		s_lineUndoHave = FALSE;
		return;
	}
	if (len > 0)
		CopyMem(ln->text, s_lineUndoText, len);
	s_lineUndoText[len] = '\0';
	s_lineUndoLen = len;
	s_lineUndoY = buffer->cursorY;
	s_lineUndoHave = TRUE;
}

BOOL
TT_UndoLine(struct TTTextBuffer *buffer)
{
	struct TTTextLine *ln;
	STRPTR curCopy;
	ULONG curLen;
	ULONG newAlloc;
	STRPTR nt;

	if (!buffer || !buffer->lines || !s_lineUndoHave || !s_lineUndoText)
		return FALSE;
	if (s_lineUndoY >= buffer->lineCount)
		return FALSE;

	ln = &buffer->lines[s_lineUndoY];
	if (!ln->text)
		return FALSE;

	curLen = ln->length;
	curCopy = (STRPTR)TT_Alloc(curLen + 1, MEMF_CLEAR);
	if (!curCopy)
		return FALSE;
	if (curLen > 0)
		CopyMem(ln->text, curCopy, curLen);
	curCopy[curLen] = '\0';

	newAlloc = s_lineUndoLen + 1;
	if (newAlloc < 256)
		newAlloc = 256;
	if (ln->allocated < newAlloc) {
		nt = (STRPTR)TT_Alloc(newAlloc, MEMF_CLEAR);
		if (!nt) {
			TT_Free(curCopy);
			return FALSE;
		}
		TT_Free(ln->text);
		ln->text = nt;
		ln->allocated = newAlloc;
	}
	if (s_lineUndoLen > 0)
		CopyMem(s_lineUndoText, ln->text, s_lineUndoLen);
	ln->text[s_lineUndoLen] = '\0';
	ln->length = s_lineUndoLen;

	TT_Free(s_lineUndoText);
	s_lineUndoText = curCopy;
	s_lineUndoLen = curLen;
	s_lineUndoHave = TRUE;

	buffer->cursorY = s_lineUndoY;
	buffer->cursorX = 0;
	buffer->modified = TRUE;
	return TRUE;
}

BOOL TT_InitTextBuffer(struct TTTextBuffer *buffer) {
  if (!buffer) {
    return FALSE;
  }

  buffer->maxLines = 1000;
  buffer->lines = (struct TTTextLine *)TT_Alloc(
      buffer->maxLines * sizeof(struct TTTextLine), MEMF_CLEAR);
  if (!buffer->lines) {
    return FALSE;
  }

  /* Initialize first line */
  buffer->lines[0].allocated = 256;
  buffer->lines[0].text =
      (STRPTR)TT_Alloc(buffer->lines[0].allocated, MEMF_CLEAR);
  if (!buffer->lines[0].text) {
    TT_Free(buffer->lines);
    buffer->lines = NULL;
    return FALSE;
  }
  buffer->lines[0].text[0] = '\0';
  buffer->lines[0].length = 0;

  buffer->lineCount = 1;
  buffer->cursorX = 0;
  buffer->cursorY = 0;
  buffer->scrollX = 0;
  buffer->scrollY = 0;
  buffer->leftMargin =
      0; /* No left margin initially - can be set for line numbers, etc. */
  buffer->pageW = 0;        /* Will be calculated when window is available */
  buffer->pageH = 0;        /* Will be calculated when window is available */
  buffer->maxScrollX = 0;   /* Will be calculated based on buffer content */
  buffer->maxScrollY = 0;   /* Will be calculated based on buffer content */
  buffer->scrollXShift = 0; /* No scaling initially */
  buffer->scrollYShift = 0; /* No scaling initially */
  buffer->modified = FALSE;
  buffer->folds = NULL;

  /* Initialize text selection/marking */
  buffer->marking.enabled = FALSE;
  buffer->marking.startY = 0;
  buffer->marking.startX = 0;
  buffer->marking.stopY = 0;
  buffer->marking.stopX = 0;

  return TRUE;
}

VOID TT_FreeTextBuffer(struct TTTextBuffer *buffer) {
  ULONG i = 0;

  if (!buffer) {
    return;
  }

  TT_LineUndoClear(buffer);
  TT_FoldFreeAll(buffer);

  if (buffer->lines) {
    for (i = 0; i < buffer->lineCount; i++) {
      if (buffer->lines[i].text) {
        TT_Free(buffer->lines[i].text);
        buffer->lines[i].text = NULL;
      }
    }
    TT_Free(buffer->lines);
    buffer->lines = NULL;
  }

  buffer->lineCount = 0;
  buffer->maxLines = 0;
}

BOOL TT_LoadFile(STRPTR fileName, struct TTTextBuffer *buffer) {
  BPTR fileHandle = NULL;
  UBYTE *lineBuffer = NULL;
  ULONG lineBufSize = 0;
  ULONG lineLen = 0;
  ULONG i = 0;
  BOOL result = FALSE;
  LONG nRead = 0;
  ULONG pos = 0;
  ULONG cap = 0;
  UBYTE ch = 0;
  BOOL sawContent = FALSE;

  if (!fileName || !buffer) {
    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
    return FALSE;
  }

  /* Heap line buffer — never put TT_MAX_LINE_LENGTH on the stack. */
  lineBufSize = TT_MAX_LINE_LENGTH;
  lineBuffer = (UBYTE *)TT_Alloc(lineBufSize, MEMF_CLEAR);
  if (!lineBuffer) {
    SetIoErr(ERROR_NO_FREE_STORE);
    return FALSE;
  }

  SetIoErr(0);
  fileHandle = Open(fileName, MODE_OLDFILE);
  if (!fileHandle) {
    TT_Free(lineBuffer);
    return FALSE;
  }
  SetIoErr(0);

  TT_FreeTextBuffer(buffer);
  if (!TT_InitTextBuffer(buffer)) {
    Close(fileHandle);
    TT_Free(lineBuffer);
    return FALSE;
  }

  i = 0;
  pos = 0;
  sawContent = FALSE;

  for (;;) {
    nRead = Read(fileHandle, &ch, 1);
    if (nRead < 0) {
      TT_FreeTextBuffer(buffer);
      Close(fileHandle);
      TT_Free(lineBuffer);
      return FALSE;
    }

    if (nRead == 1)
      sawContent = TRUE;

    if (nRead == 0 || ch == '\n') {
      if (nRead == 0 && pos == 0 && i > 0)
        break;

      lineLen = pos;
      if (lineLen > TT_MAX_LINE_LENGTH - 1)
        lineLen = TT_MAX_LINE_LENGTH - 1;

      if (i >= TT_MAX_LINES)
        break;

      if (i >= buffer->maxLines) {
        ULONG newMax = 0;
        ULONG copyIdx = 0;
        struct TTTextLine *newLines = NULL;

        newMax = buffer->maxLines * 2;
        if (newMax > TT_MAX_LINES)
          newMax = TT_MAX_LINES;
        if (newMax <= buffer->maxLines)
          break;
        newLines = (struct TTTextLine *)TT_Alloc(
            newMax * sizeof(struct TTTextLine), MEMF_CLEAR);
        if (!newLines) {
          TT_FreeTextBuffer(buffer);
          Close(fileHandle);
          TT_Free(lineBuffer);
          return FALSE;
        }
        for (copyIdx = 0; copyIdx < i; copyIdx++)
          newLines[copyIdx] = buffer->lines[copyIdx];
        TT_Free(buffer->lines);
        buffer->lines = newLines;
        buffer->maxLines = newMax;
      }

      if (buffer->lines[i].text) {
        TT_Free(buffer->lines[i].text);
        buffer->lines[i].text = NULL;
      }

      cap = lineLen + 256;
      buffer->lines[i].allocated = cap;
      buffer->lines[i].text = (STRPTR)TT_Alloc(cap, MEMF_CLEAR);
      if (!buffer->lines[i].text) {
        TT_FreeTextBuffer(buffer);
        Close(fileHandle);
        TT_Free(lineBuffer);
        return FALSE;
      }
      if (lineLen > 0)
        CopyMem(lineBuffer, buffer->lines[i].text, lineLen);
      buffer->lines[i].text[lineLen] = '\0';
      buffer->lines[i].length = lineLen;
      i++;
      pos = 0;

      if (nRead == 0)
        break;
      continue;
    }

    if (ch == '\r')
      continue;

    if (pos + 1 < lineBufSize) {
      lineBuffer[pos] = ch;
      pos++;
    }
  }

  SetIoErr(0);
  Close(fileHandle);
  SetIoErr(0);
  TT_Free(lineBuffer);

  if (!sawContent || i == 0)
    buffer->lineCount = 1;
  else
    buffer->lineCount = i;

  buffer->cursorX = 0;
  buffer->cursorY = 0;
  buffer->modified = FALSE;
  result = TRUE;
  return result;
}

BOOL TT_SaveFile(STRPTR fileName, struct TTTextBuffer *buffer) {
  BPTR fileHandle = NULL;
  ULONG i = 0;
  BOOL result = FALSE;

  if (!fileName || !buffer) {
    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
    return FALSE;
  }

  /* Open file for writing using cleanup stack */
  fileHandle = Open(fileName, MODE_NEWFILE);
  if (!fileHandle) {
    return FALSE;
  }

  /* Write each line */
  for (i = 0; i < buffer->lineCount; i++) {
    if (buffer->lines[i].text && buffer->lines[i].length > 0) {
      if (Write(fileHandle, buffer->lines[i].text, buffer->lines[i].length) !=
          buffer->lines[i].length) {
        Close(fileHandle);
        return FALSE;
      }
    }
    /* Write newline (except for last line if empty) */
    if (i < buffer->lineCount - 1 ||
        (buffer->lines[i].text && buffer->lines[i].length > 0)) {
      if (Write(fileHandle, "\n", 1) != 1) {
        Close(fileHandle);
        return FALSE;
      }
    }
  }

  /* Close file using cleanup stack */
  Close(fileHandle);
  buffer->modified = FALSE;
  result = TRUE;
  return result;
}

BOOL TT_InsertChar(struct TTTextBuffer *buffer, UBYTE ch) {
  struct TTTextLine *line = NULL;
  STRPTR newText = NULL;
  ULONG newAlloc = 0;

  if (!buffer || !buffer->lines || buffer->cursorY >= buffer->lineCount) {
    return FALSE;
  }

  TT_LineUndoTouch(buffer);

  line = &buffer->lines[buffer->cursorY];

  /* Expand line buffer if needed */
  if (line->length + 1 >= line->allocated) {
    newAlloc = line->allocated * 2;
    if (newAlloc < 256) {
      newAlloc = 256;
    }
    newText = (STRPTR)TT_Alloc(newAlloc, MEMF_CLEAR);
    if (!newText) {
      return FALSE;
    }
    if (line->text && line->length > 0) {
      CopyMem(line->text, newText, line->length);
    }
    if (line->text) {
      TT_Free(line->text);
    }
    line->text = newText;
    line->allocated = newAlloc;
  }

  /* Insert character */
  if (buffer->cursorX < line->length) {
    /* Shift existing characters right - copy backwards for overlapping memory
     */
    ULONG moveLen = 0;
    ULONG i = 0;
    moveLen = line->length - buffer->cursorX;
    for (i = moveLen; i > 0; i--) {
      line->text[buffer->cursorX + i] = line->text[buffer->cursorX + i - 1];
    }
  }
  line->text[buffer->cursorX] = ch;
  line->length++;
  line->text[line->length] = '\0';
  buffer->cursorX++;
  buffer->modified = TRUE;

  return TRUE;
}

BOOL TT_DeleteChar(struct TTTextBuffer *buffer) {
  struct TTTextLine *line = NULL;

  if (!buffer || !buffer->lines || buffer->cursorY >= buffer->lineCount) {
    return FALSE;
  }

  TT_LineUndoTouch(buffer);

  line = &buffer->lines[buffer->cursorY];

  /* Delete character before cursor */
  if (buffer->cursorX > 0) {
    if (buffer->cursorX < line->length) {
      /* Shift characters left */
      CopyMem(&line->text[buffer->cursorX], &line->text[buffer->cursorX - 1],
              line->length - buffer->cursorX);
    }
    line->length--;
    line->text[line->length] = '\0';
    buffer->cursorX--;
    buffer->modified = TRUE;
    return TRUE;
  } else if (buffer->cursorY > 0) {
    /* Merge with previous line */
    struct TTTextLine *prevLine = &buffer->lines[buffer->cursorY - 1];
    ULONG prevLen = prevLine->length;
    ULONG currLen = line->length;
    STRPTR newText = NULL;
    ULONG newAlloc = 0;

    if (prevLen + currLen + 1 > prevLine->allocated) {
      newAlloc = prevLen + currLen + 256;
      newText = (STRPTR)TT_Alloc(newAlloc, MEMF_CLEAR);
      if (!newText) {
        return FALSE;
      }
      if (prevLine->text && prevLen > 0) {
        CopyMem(prevLine->text, newText, prevLen);
      }
      if (line->text && currLen > 0) {
        CopyMem(line->text, &newText[prevLen], currLen);
      }
      if (prevLine->text) {
        TT_Free(prevLine->text);
      }
      prevLine->text = newText;
      prevLine->allocated = newAlloc;
    } else {
      if (line->text && currLen > 0) {
        CopyMem(line->text, &prevLine->text[prevLen], currLen);
      }
    }
    prevLine->length = prevLen + currLen;
    prevLine->text[prevLine->length] = '\0';

    /* Remove current line */
    if (line->text) {
      TT_Free(line->text);
    }
    if (buffer->cursorY < buffer->lineCount - 1) {
      /* Move lines down - copy backwards for overlapping memory */
      ULONG moveCount = 0;
      ULONG i = 0;
      moveCount = buffer->lineCount - buffer->cursorY - 1;
      for (i = 0; i < moveCount; i++) {
        buffer->lines[buffer->cursorY + i] =
            buffer->lines[buffer->cursorY + i + 1];
      }
    }
    buffer->lineCount--;
    buffer->cursorY--;
    buffer->cursorX = prevLen;
    buffer->modified = TRUE;
    return TRUE;
  }

  return FALSE;
}

BOOL TT_InsertNewline(struct TTTextBuffer *buffer) {
  struct TTTextLine *line = NULL;
  struct TTTextLine *newLine = NULL;
  ULONG splitPos = 0;
  ULONG remainingLen = 0;
  ULONG i = 0;

  if (!buffer || !buffer->lines || buffer->cursorY >= buffer->lineCount) {
    return FALSE;
  }

  TT_LineUndoClear(buffer);

  line = &buffer->lines[buffer->cursorY];
  if (!line->text) {
    line->allocated = 256;
    line->text = (STRPTR)TT_Alloc(line->allocated, MEMF_CLEAR);
    if (!line->text)
      return FALSE;
    line->length = 0;
  }
  if (buffer->cursorX > line->length)
    buffer->cursorX = line->length;

  /* Expand line array if needed */
  if (buffer->lineCount >= buffer->maxLines) {
    ULONG newMax = 0;
    struct TTTextLine *newLines = NULL;

    newMax = buffer->maxLines * 2;
    newLines = (struct TTTextLine *)TT_Alloc(newMax * sizeof(struct TTTextLine),
                                           MEMF_CLEAR);
    if (newLines) {
      for (i = 0; i < buffer->lineCount; i++) {
        newLines[i] = buffer->lines[i];
      }
      TT_Free(buffer->lines);
      buffer->lines = newLines;
      buffer->maxLines = newMax;
      line = &buffer->lines[buffer->cursorY];
    } else {
      return FALSE;
    }
  }

  splitPos = buffer->cursorX;
  remainingLen = line->length - splitPos;

  /* Shift lines down */
  for (i = buffer->lineCount; i > buffer->cursorY + 1; i--) {
    buffer->lines[i] = buffer->lines[i - 1];
  }
  /* Vacated slot must not keep a shallow-copied pointer. */
  buffer->lines[buffer->cursorY + 1].text = NULL;
  buffer->lines[buffer->cursorY + 1].length = 0;
  buffer->lines[buffer->cursorY + 1].allocated = 0;

  /* Create new line */
  newLine = &buffer->lines[buffer->cursorY + 1];
  newLine->allocated = remainingLen + 256;
  newLine->text = (STRPTR)TT_Alloc(newLine->allocated, MEMF_CLEAR);
  if (!newLine->text) {
    /* Restore line array */
    for (i = buffer->cursorY + 1; i < buffer->lineCount; i++) {
      buffer->lines[i] = buffer->lines[i + 1];
    }
    return FALSE;
  }

  /* Split line */
  if (remainingLen > 0) {
    CopyMem(&line->text[splitPos], newLine->text, remainingLen);
  }
  newLine->text[remainingLen] = '\0';
  newLine->length = remainingLen;

  /* Truncate current line */
  line->text[splitPos] = '\0';
  line->length = splitPos;

  buffer->lineCount++;
  buffer->cursorY++;
  buffer->cursorX = 0;
  buffer->modified = TRUE;
  TT_FoldAdjustInsert(buffer, buffer->cursorY);

  return TRUE;
}

BOOL TT_DeleteForward(struct TTTextBuffer *buffer) {
  struct TTTextLine *line = NULL;
  struct TTTextLine *nextLine = NULL;

  if (!buffer || !buffer->lines || buffer->cursorY >= buffer->lineCount) {
    return FALSE;
  }

  TT_LineUndoTouch(buffer);

  line = &buffer->lines[buffer->cursorY];

  /* Delete character after cursor */
  if (buffer->cursorX < line->length) {
    /* Shift characters left */
    if (buffer->cursorX + 1 < line->length) {
      CopyMem(&line->text[buffer->cursorX + 1], &line->text[buffer->cursorX],
              line->length - buffer->cursorX - 1);
    }
    line->length--;
    line->text[line->length] = '\0';
    buffer->modified = TRUE;
    return TRUE;
  } else if (buffer->cursorY < buffer->lineCount - 1) {
    ULONG currLen = line->length;
    ULONG nextLen = 0;
    STRPTR newText = NULL;
    ULONG newAlloc = 0;

    /* Merge with next line */
    nextLine = &buffer->lines[buffer->cursorY + 1];
    nextLen = nextLine->length;

    if (currLen + nextLen + 1 > line->allocated) {
      newAlloc = currLen + nextLen + 256;
      newText = (STRPTR)TT_Alloc(newAlloc, MEMF_CLEAR);
      if (!newText) {
        return FALSE;
      }
      if (line->text && currLen > 0) {
        CopyMem(line->text, newText, currLen);
      }
      if (nextLine->text && nextLen > 0) {
        CopyMem(nextLine->text, &newText[currLen], nextLen);
      }
      if (line->text) {
        TT_Free(line->text);
      }
      line->text = newText;
      line->allocated = newAlloc;
    } else {
      if (nextLine->text && nextLen > 0) {
        CopyMem(nextLine->text, &line->text[currLen], nextLen);
      }
    }
    line->length = currLen + nextLen;
    line->text[line->length] = '\0';

    /* Remove next line */
    if (nextLine->text) {
      TT_Free(nextLine->text);
    }
    if (buffer->cursorY + 1 < buffer->lineCount - 1) {
      /* Move lines up */
      ULONG moveCount = 0;
      ULONG i = 0;
      moveCount = buffer->lineCount - buffer->cursorY - 2;
      for (i = 0; i < moveCount; i++) {
        buffer->lines[buffer->cursorY + 1 + i] =
            buffer->lines[buffer->cursorY + 2 + i];
      }
    }
    buffer->lineCount--;
    buffer->modified = TRUE;
    return TRUE;
  }

  return FALSE;
}

BOOL TT_DeleteEOL(struct TTTextBuffer *buffer) {
  ULONG startX = 0;
  ULONG endX = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount ) {
    return FALSE;
  }

  TT_LineUndoTouch(buffer);

  startX = buffer->cursorX;
  endX = buffer->lines[buffer->cursorY].length;

  if (startX >= endX) {
    return FALSE; /* Nothing to delete */
  }

  /* Set marking and delete */
  TT_SetMarking(buffer, buffer->cursorY, startX, buffer->cursorY, endX);
  return TT_DeleteBlock(buffer);
}

BOOL TT_DeleteEOW(struct TTTextBuffer *buffer) {
  ULONG startX = 0;
  ULONG startY = 0;
  ULONG endX = 0;
  ULONG endY = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount ) {
    return FALSE;
  }

  startX = buffer->cursorX;
  startY = buffer->cursorY;

  /* Move to end of word */
  if (!TT_MoveEndOfWord(buffer)) {
    return FALSE;
  }

  endX = buffer->cursorX;
  endY = buffer->cursorY;

  /* Restore cursor and delete */
  buffer->cursorX = startX;
  buffer->cursorY = startY;

  if (startY == endY && startX < endX) {
    TT_SetMarking(buffer, startY, startX, endY, endX);
    return TT_DeleteBlock(buffer);
  }

  return FALSE;
}

BOOL TT_DeleteSOL(struct TTTextBuffer *buffer) {
  ULONG startX = 0;
  ULONG endX = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount ) {
    return FALSE;
  }

  TT_LineUndoTouch(buffer);

  endX = buffer->cursorX;
  startX = 0;

  if (startX >= endX) {
    return FALSE; /* Nothing to delete */
  }

  /* Set marking and delete */
  TT_SetMarking(buffer, buffer->cursorY, startX, buffer->cursorY, endX);
  if (TT_DeleteBlock(buffer)) {
    buffer->cursorX = 0;
    return TRUE;
  }

  return FALSE;
}

BOOL TT_DeleteSOW(struct TTTextBuffer *buffer) {
  ULONG startX = 0;
  ULONG startY = 0;
  ULONG endX = 0;
  ULONG endY = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount ) {
    return FALSE;
  }

  endX = buffer->cursorX;
  endY = buffer->cursorY;

  /* Move to start of word */
  if (!TT_MoveStartOfWord(buffer)) {
    return FALSE;
  }

  startX = buffer->cursorX;
  startY = buffer->cursorY;

  if (startY == endY && startX < endX) {
    TT_SetMarking(buffer, startY, startX, endY, endX);
    if (TT_DeleteBlock(buffer)) {
      buffer->cursorX = startX;
      return TRUE;
    }
  }

  return FALSE;
}

BOOL TT_DeleteLine(struct TTTextBuffer *buffer) {
  ULONG lineY = 0;
  ULONG i = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount ) {
    return FALSE;
  }

  TT_LineUndoClear(buffer);

  lineY = buffer->cursorY;
  TT_FoldAdjustDelete(buffer, lineY);

  /* Free line text */
  if (buffer->lines[lineY].text) {
    TT_Free(buffer->lines[lineY].text);
    buffer->lines[lineY].text = NULL;
  }

  /* Shift lines up */
  for (i = lineY; i < buffer->lineCount - 1; i++) {
    buffer->lines[i] = buffer->lines[i + 1];
  }

  buffer->lineCount--;
  if (buffer->lineCount < buffer->maxLines) {
    buffer->lines[buffer->lineCount].text = NULL;
    buffer->lines[buffer->lineCount].length = 0;
    buffer->lines[buffer->lineCount].allocated = 0;
  }

  /* Ensure at least one empty line */
  if (buffer->lineCount == 0) {
    buffer->lines[0].allocated = 256;
    buffer->lines[0].text = (STRPTR)TT_Alloc(256, MEMF_CLEAR);
    if (!buffer->lines[0].text) {
      return FALSE;
    }
    buffer->lines[0].text[0] = '\0';
    buffer->lines[0].length = 0;
    buffer->lineCount = 1;
  }

  /* Adjust cursor */
  if (buffer->cursorY >= buffer->lineCount) {
    buffer->cursorY = buffer->lineCount - 1;
  }
  if (buffer->cursorY == lineY && buffer->cursorY < buffer->lineCount) {
    buffer->cursorX = 0;
    if (buffer->cursorX > buffer->lines[buffer->cursorY].length) {
      buffer->cursorX = buffer->lines[buffer->cursorY].length;
    }
  }

  buffer->modified = TRUE;
  return TRUE;
}

BOOL TT_InsertText(struct TTTextBuffer *buffer, STRPTR text) {
  ULONG i = 0;
  UBYTE ch = 0;

  if (!buffer || !text ) {
    return FALSE;
  }

  /* Insert each character */
  i = 0;
  while (text[i] != '\0') {
    ch = (UBYTE)text[i];
    if (ch == '\n') {
      if (!TT_InsertNewline(buffer)) {
        return FALSE;
      }
    } else {
      if (!TT_InsertChar(buffer, ch)) {
        return FALSE;
      }
    }
    i++;
  }

  return TRUE;
}

UBYTE TT_GetCharAtCursor(struct TTTextBuffer *buffer) {
  if (!buffer || buffer->cursorY >= buffer->lineCount) {
    return 0;
  }

  if (buffer->cursorX < buffer->lines[buffer->cursorY].length) {
    return (UBYTE)buffer->lines[buffer->cursorY].text[buffer->cursorX];
  }

  return 0;
}

STRPTR TT_GetCurrentLine(struct TTTextBuffer *buffer) {
  STRPTR result = NULL;
  ULONG len = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount ) {
    return NULL;
  }

  len = buffer->lines[buffer->cursorY].length;
  result = (STRPTR)TT_Alloc(len + 1, MEMF_CLEAR);
  if (!result) {
    return NULL;
  }

  if (len > 0) {
    CopyMem(buffer->lines[buffer->cursorY].text, result, len);
  }
  result[len] = '\0';

  return result;
}

BOOL TT_SetCharAtCursor(struct TTTextBuffer *buffer, UBYTE ch) {
  if (!buffer || buffer->cursorY >= buffer->lineCount ) {
    return FALSE;
  }

  if (!buffer->lines[buffer->cursorY].text)
    return FALSE;

  if (buffer->cursorX < buffer->lines[buffer->cursorY].length) {
    TT_LineUndoTouch(buffer);
    buffer->lines[buffer->cursorY].text[buffer->cursorX] = (char)ch;
    buffer->modified = TRUE;
    return TRUE;
  } else {
    /* Insert at end of line */
    return TT_InsertChar(buffer, ch);
  }
}

BOOL TT_SwapChars(struct TTTextBuffer *buffer) {
  UBYTE currCh = 0;
  UBYTE prevCh = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount ) {
    return FALSE;
  }

  /* Get current character */
  currCh = TT_GetCharAtCursor(buffer);
  if (currCh == 0) {
    return FALSE;
  }

  /* Get previous character */
  if (buffer->cursorX > 0) {
    prevCh = (UBYTE)buffer->lines[buffer->cursorY].text[buffer->cursorX - 1];
  } else if (buffer->cursorY > 0) {
    ULONG prevLen = buffer->lines[buffer->cursorY - 1].length;
    if (prevLen > 0) {
      prevCh = (UBYTE)buffer->lines[buffer->cursorY - 1].text[prevLen - 1];
    } else {
      return FALSE;
    }
  } else {
    return FALSE;
  }

  /* Swap */
  if (buffer->cursorX > 0) {
    buffer->lines[buffer->cursorY].text[buffer->cursorX - 1] = (char)currCh;
    buffer->lines[buffer->cursorY].text[buffer->cursorX] = (char)prevCh;
    buffer->modified = TRUE;
    return TRUE;
  } else {
    /* Cross-line swap - move cursor back, swap, move forward */
    ULONG prevLen = 0;
    buffer->cursorY--;
    prevLen = buffer->lines[buffer->cursorY].length;
    buffer->cursorX = prevLen - 1;
    buffer->lines[buffer->cursorY].text[prevLen - 1] = (char)currCh;
    buffer->cursorY++;
    buffer->cursorX = 0;
    if (!TT_InsertChar(buffer, prevCh)) {
      return FALSE;
    }
    buffer->cursorX = 1;
    if (!TT_DeleteChar(buffer)) {
      return FALSE;
    }
    buffer->cursorX = 0;
    return TRUE;
  }
}

BOOL TT_ToggleCharCase(struct TTTextBuffer *buffer) {
  UBYTE ch = 0;
  UBYTE newCh = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount ) {
    return FALSE;
  }

  ch = TT_GetCharAtCursor(buffer);
  if (ch == 0) {
    return FALSE;
  }

  if (ch >= 'a' && ch <= 'z') {
    newCh = ch - 'a' + 'A';
  } else if (ch >= 'A' && ch <= 'Z') {
    newCh = ch - 'A' + 'a';
  } else {
    return FALSE; /* Not a letter */
  }

  return TT_SetCharAtCursor(buffer, newCh);
}

STRPTR TT_GetWordAtCursor(struct TTTextBuffer *buffer) {
  ULONG startX = 0;
  ULONG endX = 0;
  ULONG wordLen = 0;
  STRPTR result = NULL;
  ULONG savedX = 0;
  ULONG savedY = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount ) {
    return NULL;
  }

  savedX = buffer->cursorX;
  savedY = buffer->cursorY;

  /* Find word boundaries */
  if (!TT_MoveStartOfWord(buffer)) {
    buffer->cursorX = savedX;
    buffer->cursorY = savedY;
    return NULL;
  }
  startX = buffer->cursorX;

  if (!TT_MoveEndOfWord(buffer)) {
    buffer->cursorX = savedX;
    buffer->cursorY = savedY;
    return NULL;
  }
  endX = buffer->cursorX;

  /* Restore cursor */
  buffer->cursorX = savedX;
  buffer->cursorY = savedY;

  if (startX >= endX || buffer->cursorY != savedY) {
    return NULL; /* Word spans multiple lines - not supported */
  }

  wordLen = endX - startX;
  result = (STRPTR)TT_Alloc(wordLen + 1, MEMF_CLEAR);
  if (!result) {
    return NULL;
  }

  CopyMem(&buffer->lines[buffer->cursorY].text[startX], result, wordLen);
  result[wordLen] = '\0';

  return result;
}

BOOL TT_ReplaceWordAtCursor(struct TTTextBuffer *buffer, STRPTR newWord)
{
  ULONG startX = 0;
  ULONG endX = 0;
  ULONG savedX = 0;
  ULONG savedY = 0;
  ULONG newWordLen = 0;
  ULONG i = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount || !newWord ) {
    return FALSE;
  }

  savedX = buffer->cursorX;
  savedY = buffer->cursorY;

  /* Find word boundaries */
  if (!TT_MoveStartOfWord(buffer)) {
    buffer->cursorX = savedX;
    buffer->cursorY = savedY;
    return FALSE;
  }
  startX = buffer->cursorX;

  if (!TT_MoveEndOfWord(buffer)) {
    buffer->cursorX = savedX;
    buffer->cursorY = savedY;
    return FALSE;
  }
  endX = buffer->cursorX;

  if (startX >= endX || buffer->cursorY != savedY) {
    buffer->cursorX = savedX;
    buffer->cursorY = savedY;
    return FALSE;
  }

  /* Calculate new word length */
  newWordLen = 0;
  while (newWord[newWordLen] != '\0') {
    newWordLen++;
  }

  /* Delete old word */
  buffer->cursorX = startX;
  TT_SetMarking(buffer, buffer->cursorY, startX, buffer->cursorY, endX);
  if (!TT_DeleteBlock(buffer)) {
    buffer->cursorX = savedX;
    buffer->cursorY = savedY;
    return FALSE;
  }

  /* Insert new word */
  for (i = 0; i < newWordLen; i++) {
    if (!TT_InsertChar(buffer, (UBYTE)newWord[i])) {
      buffer->cursorX = savedX;
      buffer->cursorY = savedY;
      return FALSE;
    }
  }

  return TRUE;
}

BOOL TT_ConvertToUpper(struct TTTextBuffer *buffer) {
  ULONG startY = 0;
  ULONG startX = 0;
  ULONG stopY = 0;
  ULONG stopX = 0;
  ULONG i = 0;
  ULONG j = 0;
  UBYTE ch = 0;

  if (!buffer) {
    return FALSE;
  }

  if (!buffer->marking.enabled) {
    return FALSE;
  }

  startY = buffer->marking.startY;
  startX = buffer->marking.startX;
  stopY = buffer->marking.stopY;
  stopX = buffer->marking.stopX;

  /* Normalize */
  if (stopY < startY || (stopY == startY && stopX < startX)) {
    ULONG temp = startY;
    startY = stopY;
    stopY = temp;
    temp = startX;
    startX = stopX;
    stopX = temp;
  }

  /* Convert characters */
  for (i = startY; i <= stopY && i < buffer->lineCount; i++) {
    ULONG lineStart = (i == startY) ? startX : 0;
    ULONG lineEnd = (i == stopY) ? stopX : buffer->lines[i].length;

    for (j = lineStart; j < lineEnd && j < buffer->lines[i].length; j++) {
      ch = (UBYTE)buffer->lines[i].text[j];
      if (ch >= 'a' && ch <= 'z') {
        buffer->lines[i].text[j] = (char)(ch - 'a' + 'A');
        buffer->modified = TRUE;
      }
    }
  }

  return TRUE;
}

BOOL TT_ConvertToLower(struct TTTextBuffer *buffer) {
  ULONG startY = 0;
  ULONG startX = 0;
  ULONG stopY = 0;
  ULONG stopX = 0;
  ULONG i = 0;
  ULONG j = 0;
  UBYTE ch = 0;

  if (!buffer) {
    return FALSE;
  }

  if (!buffer->marking.enabled) {
    return FALSE;
  }

  startY = buffer->marking.startY;
  startX = buffer->marking.startX;
  stopY = buffer->marking.stopY;
  stopX = buffer->marking.stopX;

  /* Normalize */
  if (stopY < startY || (stopY == startY && stopX < startX)) {
    ULONG temp = startY;
    startY = stopY;
    stopY = temp;
    temp = startX;
    startX = stopX;
    stopX = temp;
  }

  /* Convert characters */
  for (i = startY; i <= stopY && i < buffer->lineCount; i++) {
    ULONG lineStart = (i == startY) ? startX : 0;
    ULONG lineEnd = (i == stopY) ? stopX : buffer->lines[i].length;

    for (j = lineStart; j < lineEnd && j < buffer->lines[i].length; j++) {
      ch = (UBYTE)buffer->lines[i].text[j];
      if (ch >= 'A' && ch <= 'Z') {
        buffer->lines[i].text[j] = (char)(ch - 'A' + 'a');
        buffer->modified = TRUE;
      }
    }
  }

  return TRUE;
}

BOOL TT_ShiftLeft(struct TTTextBuffer *buffer) {
  ULONG startY = 0;
  ULONG stopY = 0;
  ULONG i = 0;
  ULONG j = 0;
  ULONG removeCount = 0;

  if (!buffer) {
    return FALSE;
  }

  if (buffer->marking.enabled) {
    startY = buffer->marking.startY;
    stopY = buffer->marking.stopY;
    if (stopY < startY) {
      ULONG temp = startY;
      startY = stopY;
      stopY = temp;
    }
  } else {
    startY = buffer->cursorY;
    stopY = buffer->cursorY;
  }

  /* Remove leading spaces/tabs from each line */
  for (i = startY; i <= stopY && i < buffer->lineCount; i++) {
    removeCount = 0;
    while (removeCount < buffer->lines[i].length &&
           (buffer->lines[i].text[removeCount] == ' ' ||
            buffer->lines[i].text[removeCount] == '\t')) {
      removeCount++;
    }

    if (removeCount > 0) {
      /* Shift characters left */
      for (j = removeCount; j <= buffer->lines[i].length; j++) {
        buffer->lines[i].text[j - removeCount] = buffer->lines[i].text[j];
      }
      buffer->lines[i].length -= removeCount;
      buffer->modified = TRUE;
    }
  }

  return TRUE;
}

BOOL TT_ShiftRight(struct TTTextBuffer *buffer) {
  ULONG startY = 0;
  ULONG stopY = 0;
  ULONG i = 0;
  ULONG j = 0;
  ULONG tabSize = 4; /* Default tab size */
  STRPTR newText = NULL;
  ULONG newAlloc = 0;

  if (!buffer) {
    return FALSE;
  }

  if (buffer->marking.enabled) {
    startY = buffer->marking.startY;
    stopY = buffer->marking.stopY;
    if (stopY < startY) {
      ULONG temp = startY;
      startY = stopY;
      stopY = temp;
    }
  } else {
    startY = buffer->cursorY;
    stopY = buffer->cursorY;
  }

  /* Add leading spaces to each line */
  for (i = startY; i <= stopY && i < buffer->lineCount; i++) {
    if (buffer->lines[i].length + tabSize >= buffer->lines[i].allocated) {
      newAlloc = buffer->lines[i].allocated * 2;
      if (newAlloc < buffer->lines[i].length + tabSize + 256) {
        newAlloc = buffer->lines[i].length + tabSize + 256;
      }
      newText = (STRPTR)TT_Alloc(newAlloc, MEMF_CLEAR);
      if (!newText) {
        continue;
      }
      if (buffer->lines[i].text && buffer->lines[i].length > 0) {
        CopyMem(buffer->lines[i].text, newText, buffer->lines[i].length);
      }
      if (buffer->lines[i].text) {
        TT_Free(buffer->lines[i].text);
      }
      buffer->lines[i].text = newText;
      buffer->lines[i].allocated = newAlloc;
    }

    /* Shift characters right */
    for (j = buffer->lines[i].length; j > 0; j--) {
      buffer->lines[i].text[j + tabSize - 1] = buffer->lines[i].text[j - 1];
    }

    /* Add spaces */
    for (j = 0; j < tabSize; j++) {
      buffer->lines[i].text[j] = ' ';
    }

    buffer->lines[i].length += tabSize;
    buffer->lines[i].text[buffer->lines[i].length] = '\0';
    buffer->modified = TRUE;
  }

  return TRUE;
}

BOOL TT_ConvertTabsToSpaces(struct TTTextBuffer *buffer) {
  return TT_ConvertTabsToSpacesEx(buffer, 8);
}

BOOL TT_ConvertTabsToSpacesEx(struct TTTextBuffer *buffer, ULONG tabSize) {
  ULONG startY = 0;
  ULONG startX = 0;
  ULONG stopY = 0;
  ULONG stopX = 0;
  ULONG i = 0;
  ULONG j = 0;
  STRPTR newText = NULL;
  ULONG newAlloc = 0;
  ULONG newLen = 0;
  ULONG tabCount = 0;
  ULONG temp;
  ULONG lineStart;
  ULONG lineEnd;
  ULONG k;
  ULONG restLen;

  if (!buffer) {
    return FALSE;
  }
  if (tabSize < 1)
    tabSize = 8;

  if (buffer->marking.enabled) {
    startY = buffer->marking.startY;
    startX = buffer->marking.startX;
    stopY = buffer->marking.stopY;
    stopX = buffer->marking.stopX;
  } else {
    startY = 0;
    startX = 0;
    stopY = buffer->lineCount - 1;
    stopX = 0;
  }

  /* Normalize */
  if (stopY < startY || (stopY == startY && stopX < startX)) {
    temp = startY;
    startY = stopY;
    stopY = temp;
    temp = startX;
    startX = stopX;
    stopX = temp;
  }

  /* Convert tabs to spaces */
  for (i = startY; i <= stopY && i < buffer->lineCount; i++) {
    lineStart = (i == startY) ? startX : 0;
    lineEnd = (i == stopY) ? stopX : buffer->lines[i].length;

    /* Count tabs in this range */
    tabCount = 0;
    for (j = lineStart; j < lineEnd && j < buffer->lines[i].length; j++) {
      if (buffer->lines[i].text[j] == '\t') {
        tabCount++;
      }
    }

    if (tabCount > 0) {
      /* Calculate new length */
      newLen = buffer->lines[i].length + (tabCount * (tabSize - 1));

      /* Allocate new buffer if needed */
      if (newLen >= buffer->lines[i].allocated) {
        newAlloc = newLen + 256;
        newText = (STRPTR)TT_Alloc(newAlloc, MEMF_CLEAR);
        if (!newText) {
          continue;
        }
        if (buffer->lines[i].text && lineStart > 0) {
          CopyMem(buffer->lines[i].text, newText, lineStart);
        }
      } else {
        newText = buffer->lines[i].text;
        newAlloc = buffer->lines[i].allocated;
      }

      /* Convert */
      newLen = lineStart;
      for (j = lineStart; j < lineEnd && j < buffer->lines[i].length; j++) {
        if (buffer->lines[i].text[j] == '\t') {
          for (k = 0; k < tabSize; k++) {
            newText[newLen++] = ' ';
          }
        } else {
          newText[newLen++] = buffer->lines[i].text[j];
        }
      }

      /* Copy rest of line */
      if (j < buffer->lines[i].length) {
        restLen = buffer->lines[i].length - j;
        CopyMem(&buffer->lines[i].text[j], &newText[newLen], restLen);
        newLen += restLen;
      }
      newText[newLen] = '\0';

      if (newText != buffer->lines[i].text) {
        if (buffer->lines[i].text) {
          TT_Free(buffer->lines[i].text);
        }
        buffer->lines[i].text = newText;
        buffer->lines[i].allocated = newAlloc;
      }
      buffer->lines[i].length = newLen;
      buffer->modified = TRUE;
    }
  }

  return TRUE;
}

BOOL TT_ConvertSpacesToTabs(struct TTTextBuffer *buffer) {
  return TT_ConvertSpacesToTabsEx(buffer, 8);
}

BOOL TT_ConvertSpacesToTabsEx(struct TTTextBuffer *buffer, ULONG tabSize) {
  ULONG i;
  ULONG j;
  ULONG col;
  ULONG run;
  ULONG out;
  ULONG newLen;
  ULONG newAlloc;
  STRPTR newText;
  STRPTR text;
  ULONG len;
  ULONG startY;
  ULONG stopY;
  BOOL changed;
  ULONG toStop;
  ULONG take;
  ULONG tmp;

  if (!buffer || !buffer->lines)
    return FALSE;
  if (tabSize < 1)
    tabSize = 8;

  changed = FALSE;

  if (buffer->marking.enabled) {
    startY = buffer->marking.startY;
    stopY = buffer->marking.stopY;
    if (stopY < startY) {
      tmp = startY;
      startY = stopY;
      stopY = tmp;
    }
  } else {
    startY = 0;
    stopY = buffer->lineCount > 0 ? buffer->lineCount - 1 : 0;
  }

  for (i = startY; i <= stopY && i < buffer->lineCount; i++) {
    text = buffer->lines[i].text;
    len = buffer->lines[i].length;
    if (!text || len == 0)
      continue;

    newAlloc = len + 1;
    newText = (STRPTR)TT_Alloc(newAlloc, MEMF_CLEAR);
    if (!newText)
      continue;

    col = 0;
    out = 0;
    j = 0;
    while (j < len) {
      if (text[j] == ' ') {
        run = 0;
        while (j + run < len && text[j + run] == ' ')
          run++;
        while (run > 0) {
          toStop = tabSize - (col % tabSize);
          if (toStop == 0)
            toStop = tabSize;
          if (run >= toStop && toStop > 1) {
            newText[out++] = '\t';
            col += toStop;
            j += toStop;
            run -= toStop;
            changed = TRUE;
          } else {
            take = run;
            while (take > 0) {
              newText[out++] = ' ';
              col++;
              j++;
              take--;
              run--;
            }
          }
        }
      } else if (text[j] == '\t') {
        toStop = tabSize - (col % tabSize);
        if (toStop == 0)
          toStop = tabSize;
        newText[out++] = '\t';
        col += toStop;
        j++;
      } else {
        newText[out++] = text[j];
        col++;
        j++;
      }
    }
    newText[out] = '\0';
    newLen = out;

    TT_Free(buffer->lines[i].text);
    buffer->lines[i].text = newText;
    buffer->lines[i].allocated = newAlloc;
    buffer->lines[i].length = newLen;
  }

  if (changed)
    buffer->modified = TRUE;
  return changed;
}

/****************************************************************************/
/* TT_FindText
 *
 * Forward or backward search with optional case fold and whole-word.
 */
static UBYTE
TT_FindFoldChar(UBYTE ch)
{
  if (ch >= (UBYTE)'A' && ch <= (UBYTE)'Z')
    return (UBYTE)(ch - (UBYTE)'A' + (UBYTE)'a');
  return ch;
}

static BOOL
TT_FindIsWordChar(UBYTE ch)
{
  if ((ch >= (UBYTE)'A' && ch <= (UBYTE)'Z') ||
      (ch >= (UBYTE)'a' && ch <= (UBYTE)'z') ||
      (ch >= (UBYTE)'0' && ch <= (UBYTE)'9') ||
      ch == (UBYTE)'_')
    return TRUE;
  return FALSE;
}

static BOOL
TT_FindCharsEqual(UBYTE a, UBYTE b, BOOL ignoreCase)
{
  if (ignoreCase)
    return (BOOL)(TT_FindFoldChar(a) == TT_FindFoldChar(b));
  return (BOOL)(a == b);
}

static BOOL
TT_FindWholeWordOk(STRPTR lineText, ULONG lineLen, ULONG x, ULONG searchLen)
{
  UBYTE before;
  UBYTE after;

  if (x > 0) {
    before = (UBYTE)lineText[x - 1];
    if (TT_FindIsWordChar(before))
      return FALSE;
  }
  if (x + searchLen < lineLen) {
    after = (UBYTE)lineText[x + searchLen];
    if (TT_FindIsWordChar(after))
      return FALSE;
  }
  return TRUE;
}

BOOL
TT_FindTextAtEx(struct TTTextBuffer *buffer, STRPTR searchStr,
                ULONG *outY, ULONG *outX, BOOL skipIfOnMatch,
                BOOL ignoreCase, BOOL wholeWords, BOOL scanBackwards)
{
  ULONG searchLen = 0;
  ULONG startX = 0;
  ULONG y = 0;
  ULONG x = 0;
  ULONG j = 0;
  STRPTR lineText = NULL;
  ULONG lineLen = 0;
  BOOL match = FALSE;
  LONG yi;
  LONG xi;

  if (!buffer || !searchStr || !outY || !outX)
    return FALSE;

  while (searchStr[searchLen] != '\0')
    searchLen++;

  if (searchLen == 0)
    return FALSE;

  if (buffer->cursorY >= buffer->lineCount || !buffer->lines)
    return FALSE;

  startX = buffer->cursorX;

  if (scanBackwards) {
    if (skipIfOnMatch && startX > 0)
      startX--;
    else if (skipIfOnMatch && startX == 0 && buffer->cursorY > 0) {
      /* fall through to previous line end in loop */
    }

    for (yi = (LONG)buffer->cursorY; yi >= 0; yi--) {
      lineText = buffer->lines[yi].text;
      lineLen = buffer->lines[yi].length;
      if (!lineText || lineLen < searchLen)
        continue;

      if ((ULONG)yi == buffer->cursorY)
        xi = (LONG)startX;
      else
        xi = (LONG)lineLen;

      if (xi > (LONG)lineLen)
        xi = (LONG)lineLen;
      if (xi >= (LONG)searchLen)
        xi = xi - (LONG)searchLen;
      else
        continue;

      for (; xi >= 0; xi--) {
        match = TRUE;
        for (j = 0; j < searchLen; j++) {
          if (!TT_FindCharsEqual((UBYTE)lineText[(ULONG)xi + j],
                                 (UBYTE)searchStr[j], ignoreCase)) {
            match = FALSE;
            break;
          }
        }
        if (match && wholeWords &&
            !TT_FindWholeWordOk(lineText, lineLen, (ULONG)xi, searchLen))
          match = FALSE;
        if (match) {
          *outY = (ULONG)yi;
          *outX = (ULONG)xi;
          return TRUE;
        }
      }
    }
    return FALSE;
  }

  if (skipIfOnMatch) {
    ULONG k = 0;
    BOOL onMatch = TRUE;

    lineText = buffer->lines[buffer->cursorY].text;
    lineLen = buffer->lines[buffer->cursorY].length;
    if (!lineText || startX + searchLen > lineLen)
      onMatch = FALSE;
    else {
      for (k = 0; k < searchLen; k++) {
        if (!TT_FindCharsEqual((UBYTE)lineText[startX + k],
                               (UBYTE)searchStr[k], ignoreCase)) {
          onMatch = FALSE;
          break;
        }
      }
      if (onMatch && wholeWords &&
          !TT_FindWholeWordOk(lineText, lineLen, startX, searchLen))
        onMatch = FALSE;
    }
    if (onMatch)
      startX = buffer->cursorX + 1;
  }

  for (y = buffer->cursorY; y < buffer->lineCount; y++) {
    lineText = buffer->lines[y].text;
    lineLen = buffer->lines[y].length;

    x = (y == buffer->cursorY) ? startX : 0;

    if (!lineText || lineLen < searchLen)
      continue;

    for (; x + searchLen <= lineLen; x++) {
      match = TRUE;
      for (j = 0; j < searchLen; j++) {
        if (!TT_FindCharsEqual((UBYTE)lineText[x + j],
                               (UBYTE)searchStr[j], ignoreCase)) {
          match = FALSE;
          break;
        }
      }
      if (match && wholeWords &&
          !TT_FindWholeWordOk(lineText, lineLen, x, searchLen))
        match = FALSE;
      if (match) {
        *outY = y;
        *outX = x;
        return TRUE;
      }
    }
  }

  return FALSE;
}

BOOL
TT_FindTextAt(struct TTTextBuffer *buffer, STRPTR searchStr,
              ULONG *outY, ULONG *outX, BOOL skipIfOnMatch)
{
  return TT_FindTextAtEx(buffer, searchStr, outY, outX, skipIfOnMatch,
                         TRUE, FALSE, FALSE);
}

BOOL
TT_FindText(struct TTTextBuffer *buffer, STRPTR searchStr,
            ULONG *outY, ULONG *outX)
{
  return TT_FindTextAt(buffer, searchStr, outY, outX, TRUE);
}

/****************************************************************************/
/* TT_CenterLine
 *
 * Strips leading and trailing spaces from the current line and re-inserts
 * the trimmed content with enough leading spaces to center it within
 * buffer->pageW columns (defaulting to 72 when pageW is 0).
 * Leaves the cursor at the first non-space character.
 */
BOOL
TT_CenterLine(struct TTTextBuffer *buffer)
{
  struct TTTextLine *line   = NULL;
  ULONG width      = 0;
  ULONG trimStart  = 0;
  ULONG trimEnd    = 0;
  ULONG trimLen    = 0;
  ULONG leftPad    = 0;
  ULONG newLen     = 0;
  STRPTR newText   = NULL;
  ULONG i          = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount)
    return FALSE;

  line = &buffer->lines[buffer->cursorY];
  if (!line->text)
    return FALSE;

  width = (buffer->pageW > 0) ? buffer->pageW : 72;

  /* Find content boundaries after stripping surrounding spaces */
  trimStart = 0;
  while (trimStart < line->length && line->text[trimStart] == ' ')
    trimStart++;

  trimEnd = line->length;
  while (trimEnd > trimStart && line->text[trimEnd - 1] == ' ')
    trimEnd--;

  trimLen = trimEnd - trimStart;

  leftPad = (trimLen < width) ? (width - trimLen) / 2 : 0;
  newLen  = leftPad + trimLen;

  newText = (STRPTR)TT_Alloc(newLen + 1, MEMF_CLEAR);
  if (!newText)
    return FALSE;

  for (i = 0; i < leftPad; i++)
    newText[i] = ' ';

  if (trimLen > 0)
    CopyMem(&line->text[trimStart], &newText[leftPad], trimLen);

  newText[newLen] = '\0';

  TT_Free(line->text);
  line->text      = newText;
  line->length    = newLen;
  line->allocated = newLen + 1;

  /* Place cursor at start of visible content */
  buffer->cursorX  = leftPad;
  buffer->modified = TRUE;
  return TRUE;
}

/****************************************************************************/
/* TT_JustifyLine
 *
 * Distributes inter-word space on the current line so that it reaches
 * exactly pageW (or 72) columns.  Requires at least two words.
 * Extra space is spread left-to-right across the (wordCount-1) gaps;
 * any remainder modulo gapCount is given one extra space each to the
 * leftmost gaps.
 */
BOOL
TT_JustifyLine(struct TTTextBuffer *buffer)
{
  struct TTTextLine *line    = NULL;
  ULONG width               = 0;
  ULONG i                   = 0;
  ULONG w                   = 0;
  ULONG inWord              = 0;
  ULONG wStart              = 0;
  ULONG wordCount           = 0;
  ULONG totalWordLen        = 0;
  ULONG gapCount            = 0;
  ULONG extraSpaces         = 0;
  ULONG spaceEach           = 0;
  ULONG spaceExtra          = 0;
  ULONG destPos             = 0;
  ULONG wordStarts[128];
  ULONG wordLens[128];
  STRPTR newText            = NULL;
  UBYTE ch                  = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount)
    return FALSE;

  line = &buffer->lines[buffer->cursorY];
  if (!line->text || line->length == 0)
    return FALSE;

  width = (buffer->pageW > 0) ? buffer->pageW : 72;

  /* Extract word extents; cap at array size */
  wordCount = 0;
  inWord    = 0;
  for (i = 0; i <= line->length && wordCount < 128; i++)
  {
    ch = (i < line->length) ? (UBYTE)line->text[i] : 0;
    if (ch != ' ' && ch != '\t' && ch != '\0')
    {
      if (!inWord)
      {
        wStart = i;
        inWord = 1;
      }
    }
    else
    {
      if (inWord)
      {
        wordStarts[wordCount] = wStart;
        wordLens[wordCount]   = i - wStart;
        wordCount++;
        inWord = 0;
      }
    }
  }

  if (wordCount < 2)
    return FALSE;

  totalWordLen = 0;
  for (w = 0; w < wordCount; w++)
    totalWordLen += wordLens[w];

  if (totalWordLen >= width)
    return FALSE; /* line already at or beyond target width */

  gapCount   = wordCount - 1;
  extraSpaces = width - totalWordLen;
  spaceEach  = extraSpaces / gapCount;
  spaceExtra = extraSpaces % gapCount;

  /* width+2 gives a small safety margin */
  newText = (STRPTR)TT_Alloc(width + 2, MEMF_CLEAR);
  if (!newText)
    return FALSE;

  destPos = 0;
  for (w = 0; w < wordCount; w++)
  {
    ULONG spaces = 0;
    ULONG si     = 0;

    if (destPos + wordLens[w] >= width + 2)
      break;
    CopyMem(&line->text[wordStarts[w]], &newText[destPos], wordLens[w]);
    destPos += wordLens[w];

    if (w < gapCount)
    {
      spaces = spaceEach + ((w < spaceExtra) ? 1 : 0);
      for (si = 0; si < spaces; si++)
      {
        if (destPos + 1 >= width + 2)
          break;
        newText[destPos++] = ' ';
      }
    }
  }
  if (destPos >= width + 2)
    destPos = width + 1;
  newText[destPos] = '\0';

  TT_Free(line->text);
  line->text      = newText;
  line->length    = destPos;
  line->allocated = width + 2;

  buffer->modified = TRUE;
  return TRUE;
}

/****************************************************************************/
/* TT_FormatParagraph
 *
 * Re-flows the paragraph around the cursor.  A paragraph is a run of
 * consecutive non-blank lines bounded above and below by blank lines or
 * the buffer edges.
 *
 * Algorithm:
 *   1. Find paragraph bounds (paraStart..paraEnd).
 *   2. Copy all paragraph text into a scratch heap buffer, joining lines
 *      with a single space.
 *   3. Remove all but the first paragraph line from buffer->lines[].
 *   4. Clear the remaining paraStart line for fresh content.
 *   5. Walk the scratch buffer word by word, inserting characters and
 *      inserting newlines at the wrap column (pageW, defaulting to 72).
 */
BOOL
TT_FormatParagraph(struct TTTextBuffer *buffer)
{
  ULONG paraStart  = 0;
  ULONG paraEnd    = 0;
  ULONG y          = 0;
  ULONG totalTextLen = 0;
  STRPTR textBuf   = NULL;
  ULONG pos        = 0;
  ULONG i          = 0;
  ULONG lineWidth  = 72;
  ULONG curLineLen = 0;
  ULONG wordStart  = 0;
  ULONG wordLen    = 0;
  ULONG delCount   = 0;
  ULONG wi         = 0;
  UBYTE wc         = 0;
  ULONG j          = 0;

  if (!buffer || buffer->cursorY >= buffer->lineCount)
    return FALSE;

  /* Locate paragraph: extend upward while lines are non-blank */
  paraStart = buffer->cursorY;
  while (paraStart > 0 && buffer->lines[paraStart - 1].length > 0)
    paraStart--;

  /* Extend downward while lines are non-blank */
  paraEnd = buffer->cursorY;
  while (paraEnd + 1 < buffer->lineCount && buffer->lines[paraEnd + 1].length > 0)
    paraEnd++;

  if (buffer->pageW > 0)
    lineWidth = buffer->pageW;

  /* Sum characters needed for the scratch buffer */
  totalTextLen = 0;
  for (y = paraStart; y <= paraEnd; y++)
    totalTextLen += buffer->lines[y].length + 1; /* +1 for joining space */

  if (totalTextLen == 0)
    return FALSE;

  textBuf = (STRPTR)TT_Alloc(totalTextLen + 1, MEMF_CLEAR);
  if (!textBuf)
    return FALSE;

  /* Copy paragraph text into scratch, joining lines with a space */
  pos = 0;
  for (y = paraStart; y <= paraEnd; y++)
  {
    ULONG len = buffer->lines[y].length;
    if (len > 0 && buffer->lines[y].text)
    {
      CopyMem(buffer->lines[y].text, &textBuf[pos], len);
      pos += len;
    }
    if (y < paraEnd)
      textBuf[pos++] = ' ';
  }
  textBuf[pos] = '\0';

  /*
   * Remove lines paraStart+1 .. paraEnd from the buffer by freeing each
   * and shifting the lines array upward.  Work from the bottom up so
   * index arithmetic stays stable.
   */
  delCount = paraEnd - paraStart;
  for (i = 0; i < delCount; i++)
  {
    ULONG delY = paraEnd - i;

    if (buffer->lines[delY].text)
    {
      TT_Free(buffer->lines[delY].text);
      buffer->lines[delY].text = NULL;
    }
    for (j = delY; j < buffer->lineCount - 1; j++)
      buffer->lines[j] = buffer->lines[j + 1];

    /* Clear the vacated trailing slot */
    buffer->lines[buffer->lineCount - 1].text      = NULL;
    buffer->lines[buffer->lineCount - 1].length    = 0;
    buffer->lines[buffer->lineCount - 1].allocated = 0;
    buffer->lineCount--;
  }

  /* Reset the paraStart line to an empty buffer ready for re-insertion */
  if (buffer->lines[paraStart].text)
  {
    TT_Free(buffer->lines[paraStart].text);
    buffer->lines[paraStart].text = NULL;
  }
  buffer->lines[paraStart].allocated = 256;
  buffer->lines[paraStart].text = (STRPTR)TT_Alloc(256, MEMF_CLEAR);
  if (!buffer->lines[paraStart].text)
  {
    TT_Free(textBuf);
    return FALSE;
  }
  buffer->lines[paraStart].text[0] = '\0';
  buffer->lines[paraStart].length  = 0;

  buffer->cursorY = paraStart;
  buffer->cursorX = 0;

  /*
   * Re-flow: walk the scratch buffer, inserting words and breaking
   * lines at lineWidth columns.
   */
  curLineLen = 0;
  i          = 0;
  while (textBuf[i] != '\0')
  {
    /* Skip whitespace between words */
    while (textBuf[i] == ' ' || textBuf[i] == '\t')
      i++;

    if (textBuf[i] == '\0')
      break;

    /* Measure next word */
    wordStart = i;
    wordLen   = 0;
    while (textBuf[i + wordLen] != '\0' &&
           textBuf[i + wordLen] != ' '  &&
           textBuf[i + wordLen] != '\t')
      wordLen++;

    /*
     * If this word does not fit on the current line (and we are not
     * at the start of a fresh line), emit a newline before it.
     */
    if (curLineLen > 0 && curLineLen + 1 + wordLen > lineWidth)
    {
      if (!TT_InsertNewline(buffer))
      {
        TT_Free(textBuf);
        buffer->modified = TRUE;
        return FALSE;
      }
      curLineLen = 0;
    }
    else if (curLineLen > 0)
    {
      /* Separate from the preceding word on the same line */
      if (!TT_InsertChar(buffer, ' '))
      {
        TT_Free(textBuf);
        buffer->modified = TRUE;
        return FALSE;
      }
      curLineLen++;
    }

    /* Insert the word character by character */
    for (wi = 0; wi < wordLen; wi++)
    {
      wc = (UBYTE)textBuf[wordStart + wi];
      if (!TT_InsertChar(buffer, wc))
      {
        TT_Free(textBuf);
        buffer->modified = TRUE;
        return FALSE;
      }
    }
    curLineLen += wordLen;
    i = wordStart + wordLen;
  }

  TT_Free(textBuf);
  buffer->modified = TRUE;
  return TRUE;
}

