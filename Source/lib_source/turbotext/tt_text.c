/*
 * turbotext.library - text buffer and editing (engine)
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "private/tt_internal.h"

#define INITIAL_BUFFER_SIZE 16384

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
  UBYTE lineBuffer[TT_MAX_LINE_LENGTH];
  ULONG lineLen = 0;
  ULONG i = 0;
  BOOL result = FALSE;

  if (!fileName || !buffer) {
    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
    return FALSE;
  }

  /* Open file for reading using cleanup stack - if file doesn't exist, create
   * empty buffer */
  /* Clear IoErr() before file operations to ensure clean state */
  SetIoErr(0);
  fileHandle = Open(fileName, MODE_OLDFILE);
  if (!fileHandle) {
    /* File doesn't exist or open failed */
    LONG errorCode = IoErr();
    if (errorCode != 0) {
      SetIoErr(0);
    }
    return FALSE;
  } else {
    /* File opened successfully - clear any error code that may have been set */
    SetIoErr(0);
  }

  /* Clear existing buffer */
  TT_FreeTextBuffer(buffer);
  if (!TT_InitTextBuffer(buffer)) {
    Close(fileHandle);
    return FALSE;
  }

  /* Read file line by line */
  /* Clear IoErr() before reading to ensure clean state */
  SetIoErr(0);
  while (FGets(fileHandle, lineBuffer, sizeof(lineBuffer) - 1) != NULL) {
    /* Cap line count to prevent overflow and unbounded allocation */
    if (i >= TT_MAX_LINES) {
      break;
    }

    lineLen = 0;
    while (lineBuffer[lineLen] != '\0' && lineBuffer[lineLen] != '\n' &&
           lineLen < sizeof(lineBuffer) - 1) {
      lineLen++;
    }

    /* Defensive: ensure lineLen is within bounds (FGets already limits input) */
    if (lineLen > TT_MAX_LINE_LENGTH - 1) {
      lineLen = TT_MAX_LINE_LENGTH - 1;
    }

    /* Remove trailing newline if present */
    if (lineLen > 0 && lineBuffer[lineLen - 1] == '\n') {
      lineLen--;
    }

    /* Expand line array if needed (capped at TT_MAX_LINES) */
    if (i >= buffer->maxLines) {
      ULONG newMax = 0;
      ULONG copyIdx = 0;
      struct TTTextLine *newLines = NULL;

      newMax = buffer->maxLines * 2;
      if (newMax > TT_MAX_LINES) {
        newMax = TT_MAX_LINES;
      }
      if (newMax <= buffer->maxLines) {
        /* Already at max - stop loading more lines to prevent overflow */
        break;
      }
      newLines = (struct TTTextLine *)TT_Alloc(newMax * sizeof(struct TTTextLine),
                                             MEMF_CLEAR);
      if (!newLines) {
        TT_FreeTextBuffer(buffer);
        Close(fileHandle);
        return FALSE;
      }
      for (copyIdx = 0; copyIdx < buffer->lineCount; copyIdx++) {
        newLines[copyIdx] = buffer->lines[copyIdx];
      }
      TT_Free(buffer->lines);
      buffer->lines = newLines;
      buffer->maxLines = newMax;
    }

    /* Allocate line text buffer */
    /* CRITICAL: If this line already has a pre-allocated text buffer, free it
     * first */
    /* This happens when LoadFile pre-allocates the next line's buffer but then
     * replaces it */
    if (buffer->lines[i].text) {
      TT_Free(buffer->lines[i].text);
      buffer->lines[i].text = NULL;
    }

    buffer->lines[i].allocated = lineLen + 256;
    buffer->lines[i].text =
        (STRPTR)TT_Alloc(buffer->lines[i].allocated, MEMF_CLEAR);
    if (!buffer->lines[i].text) {
      TT_FreeTextBuffer(buffer);
      Close(fileHandle);
      return FALSE;
    }

    /* Copy line text */
    if (lineLen > 0) {
      CopyMem(lineBuffer, buffer->lines[i].text, lineLen);
    }
    buffer->lines[i].text[lineLen] = '\0';
    buffer->lines[i].length = lineLen;

    i++;

    /* Stop if we've reached max line count (prevents overflow) */
    if (i >= buffer->maxLines || i >= TT_MAX_LINES) {
      break;
    }

    /* Pre-allocate next line's text buffer */
    /* CRITICAL: Check if this line already has a buffer (from previous
     * iteration) and free it first */
    if (buffer->lines[i].text) {
      TT_Free(buffer->lines[i].text);
      buffer->lines[i].text = NULL;
    }

    buffer->lines[i].allocated = 256;
    buffer->lines[i].text =
        (STRPTR)TT_Alloc(buffer->lines[i].allocated, MEMF_CLEAR);
    if (!buffer->lines[i].text) {
      buffer->lineCount = i;
      break;
    }
    buffer->lines[i].text[0] = '\0';
    buffer->lines[i].length = 0;
  }

  /* FGets loop ended - clear any error codes to prevent dos.library corruption
   */
  /* FGets returns NULL on both EOF and error, so we clear IoErr() regardless */
  SetIoErr(0);

  /* CRITICAL FIX: Free orphaned pre-allocation for lines[i] if present. */
  /* The loop pre-allocates lines[i+1].text at the end of each iteration, but */
  /* when FGets returns NULL (EOF), the pre-alloc for lines[i] is never filled
   */
  /* and FreeTextBuffer only frees lines[0]..lines[lineCount-1], missing it. */
  /* Leaving it tracked causes a double-free when AmigaOS reuses the address. */
  if (i < buffer->maxLines && buffer->lines[i].text) {
    TT_Free(buffer->lines[i].text);
    buffer->lines[i].text = NULL;
    buffer->lines[i].allocated = 0;
  }

  buffer->lineCount = i;
  if (buffer->lineCount == 0) {
    buffer->lineCount = 1;
  }

  buffer->cursorX = 0;
  buffer->cursorY = 0;
  buffer->modified = FALSE;

  /* Close file using cleanup stack */
  /* Clear IoErr() before closing to ensure clean state */
  SetIoErr(0);
  Close(fileHandle);
  /* Clear IoErr() after closing to prevent dos.library from being left in
   * undefined state */
  SetIoErr(0);
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
    } else {
      return FALSE;
    }
  }

  line = &buffer->lines[buffer->cursorY];
  splitPos = buffer->cursorX;
  remainingLen = line->length - splitPos;

  /* Shift lines down */
  for (i = buffer->lineCount; i > buffer->cursorY + 1; i--) {
    buffer->lines[i] = buffer->lines[i - 1];
  }

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

  return TRUE;
}

BOOL TT_DeleteForward(struct TTTextBuffer *buffer) {
  struct TTTextLine *line = NULL;
  struct TTTextLine *nextLine = NULL;

  if (!buffer || !buffer->lines || buffer->cursorY >= buffer->lineCount) {
    return FALSE;
  }

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

  lineY = buffer->cursorY;

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

  if (buffer->cursorX < buffer->lines[buffer->cursorY].length) {
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
  ULONG startY = 0;
  ULONG startX = 0;
  ULONG stopY = 0;
  ULONG stopX = 0;
  ULONG i = 0;
  ULONG j = 0;
  ULONG tabSize = 4;
  STRPTR newText = NULL;
  ULONG newAlloc = 0;
  ULONG newLen = 0;
  ULONG tabCount = 0;

  if (!buffer) {
    return FALSE;
  }

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
    ULONG temp = startY;
    startY = stopY;
    stopY = temp;
    temp = startX;
    startX = stopX;
    stopX = temp;
  }

  /* Convert tabs to spaces */
  for (i = startY; i <= stopY && i < buffer->lineCount; i++) {
    ULONG lineStart = (i == startY) ? startX : 0;
    ULONG lineEnd = (i == stopY) ? stopX : buffer->lines[i].length;

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
          ULONG k = 0;
          for (k = 0; k < tabSize; k++) {
            newText[newLen++] = ' ';
          }
        } else {
          newText[newLen++] = buffer->lines[i].text[j];
        }
      }

      /* Copy rest of line */
      if (j < buffer->lines[i].length) {
        ULONG restLen = 0;
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
  /* TODO: Implement spaces to tabs conversion */
  /* This is more complex as it requires detecting tab stops */
  return FALSE;
}

