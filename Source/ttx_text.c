/*
 * TTX - Text Buffer and Editing Functions
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx.h"

/* Initial buffer size constant */
#define INITIAL_BUFFER_SIZE 65536

/* Initialize text buffer */
BOOL InitTextBuffer(struct TextBuffer *buffer)
{
    if (!buffer) {
        return FALSE;
    }
    
    /* Allocate initial line array */
    buffer->maxLines = 1000;
    buffer->lines = (struct TextLine *)AllocVec(buffer->maxLines * sizeof(struct TextLine), MEMF_CLEAR);
    if (!buffer->lines) {
        return FALSE;
    }
    
    /* Initialize first line */
    buffer->lines[0].allocated = 256;
    buffer->lines[0].text = (STRPTR)AllocVec(buffer->lines[0].allocated, MEMF_CLEAR);
    if (!buffer->lines[0].text) {
        FreeVec(buffer->lines);
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
    buffer->modified = FALSE;
    
    return TRUE;
}

/* Free text buffer */
VOID FreeTextBuffer(struct TextBuffer *buffer)
{
    ULONG i = 0;
    
    if (!buffer) {
        return;
    }
    
    if (buffer->lines) {
        for (i = 0; i < buffer->lineCount; i++) {
            if (buffer->lines[i].text) {
                FreeVec(buffer->lines[i].text);
                buffer->lines[i].text = NULL;
            }
        }
        FreeVec(buffer->lines);
        buffer->lines = NULL;
    }
    
    buffer->lineCount = 0;
    buffer->maxLines = 0;
}

/* Load file into text buffer */
BOOL LoadFile(STRPTR fileName, struct TextBuffer *buffer)
{
    BPTR fileHandle = NULL;
    UBYTE lineBuffer[MAX_LINE_LENGTH];
    ULONG lineLen = 0;
    ULONG i = 0;
    BOOL result = FALSE;
    
    if (!fileName || !buffer) {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }
    
    /* Open file for reading - if file doesn't exist, create empty buffer */
    fileHandle = Open(fileName, MODE_OLDFILE);
    if (!fileHandle) {
        /* File doesn't exist - create empty buffer */
        FreeTextBuffer(buffer);
        if (!InitTextBuffer(buffer)) {
            return FALSE;
        }
        return TRUE;
    }
    
    /* Clear existing buffer */
    FreeTextBuffer(buffer);
    if (!InitTextBuffer(buffer)) {
        Close(fileHandle);
        return FALSE;
    }
    
    /* Read file line by line */
    while (FGets(fileHandle, lineBuffer, sizeof(lineBuffer) - 1)) {
        lineLen = 0;
        while (lineBuffer[lineLen] != '\0' && lineBuffer[lineLen] != '\n' && lineLen < sizeof(lineBuffer) - 1) {
            lineLen++;
        }
        
        /* Remove trailing newline if present */
        if (lineLen > 0 && lineBuffer[lineLen - 1] == '\n') {
            lineLen--;
        }
        
        /* Expand line array if needed */
        if (i >= buffer->maxLines) {
            ULONG newMax = 0;
            ULONG copyIdx = 0;
            struct TextLine *newLines = NULL;
            
            newMax = buffer->maxLines * 2;
            newLines = (struct TextLine *)AllocVec(newMax * sizeof(struct TextLine), MEMF_CLEAR);
            if (!newLines) {
                FreeTextBuffer(buffer);
                Close(fileHandle);
                return FALSE;
            }
            for (copyIdx = 0; copyIdx < buffer->lineCount; copyIdx++) {
                newLines[copyIdx] = buffer->lines[copyIdx];
            }
            FreeVec(buffer->lines);
            buffer->lines = newLines;
            buffer->maxLines = newMax;
        }
        
        /* Allocate line text buffer */
        buffer->lines[i].allocated = lineLen + 256;
        buffer->lines[i].text = (STRPTR)AllocVec(buffer->lines[i].allocated, MEMF_CLEAR);
        if (!buffer->lines[i].text) {
            FreeTextBuffer(buffer);
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
        
        /* Add new line if file continues */
        if (i >= buffer->maxLines) {
            break;
        }
        
        buffer->lines[i].allocated = 256;
        buffer->lines[i].text = (STRPTR)AllocVec(buffer->lines[i].allocated, MEMF_CLEAR);
        if (!buffer->lines[i].text) {
            buffer->lineCount = i;
            break;
        }
        buffer->lines[i].text[0] = '\0';
        buffer->lines[i].length = 0;
    }
    
    buffer->lineCount = i;
    if (buffer->lineCount == 0) {
        buffer->lineCount = 1;
    }
    
    buffer->cursorX = 0;
    buffer->cursorY = 0;
    buffer->modified = FALSE;
    
    Close(fileHandle);
    result = TRUE;
    return result;
}

/* Save text buffer to file */
BOOL SaveFile(STRPTR fileName, struct TextBuffer *buffer)
{
    BPTR fileHandle = NULL;
    ULONG i = 0;
    BOOL result = FALSE;
    
    if (!fileName || !buffer) {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }
    
    /* Open file for writing */
    fileHandle = Open(fileName, MODE_NEWFILE);
    if (!fileHandle) {
        return FALSE;
    }
    
    /* Write each line */
    for (i = 0; i < buffer->lineCount; i++) {
        if (buffer->lines[i].text && buffer->lines[i].length > 0) {
            if (Write(fileHandle, buffer->lines[i].text, buffer->lines[i].length) != buffer->lines[i].length) {
                Close(fileHandle);
                return FALSE;
            }
        }
        /* Write newline (except for last line if empty) */
        if (i < buffer->lineCount - 1 || (buffer->lines[i].text && buffer->lines[i].length > 0)) {
            if (Write(fileHandle, "\n", 1) != 1) {
                Close(fileHandle);
                return FALSE;
            }
        }
    }
    
    Close(fileHandle);
    buffer->modified = FALSE;
    result = TRUE;
    return result;
}

/* Insert character at cursor position */
BOOL InsertChar(struct TextBuffer *buffer, UBYTE ch)
{
    struct TextLine *line = NULL;
    STRPTR newText = NULL;
    ULONG newAlloc = 0;
    
    if (!buffer || buffer->cursorY >= buffer->lineCount) {
        return FALSE;
    }
    
    line = &buffer->lines[buffer->cursorY];
    
    /* Expand line buffer if needed */
    if (line->length + 1 >= line->allocated) {
        newAlloc = line->allocated * 2;
        if (newAlloc < 256) {
            newAlloc = 256;
        }
        newText = (STRPTR)AllocVec(newAlloc, MEMF_CLEAR);
        if (!newText) {
            return FALSE;
        }
        if (line->text && line->length > 0) {
            CopyMem(line->text, newText, line->length);
        }
        if (line->text) {
            FreeVec(line->text);
        }
        line->text = newText;
        line->allocated = newAlloc;
    }
    
    /* Insert character */
    if (buffer->cursorX < line->length) {
        /* Shift existing characters right - copy backwards for overlapping memory */
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

/* Delete character at cursor position */
BOOL DeleteChar(struct TextBuffer *buffer)
{
    struct TextLine *line = NULL;
    
    if (!buffer || buffer->cursorY >= buffer->lineCount) {
        return FALSE;
    }
    
    line = &buffer->lines[buffer->cursorY];
    
    /* Delete character before cursor */
    if (buffer->cursorX > 0) {
        if (buffer->cursorX < line->length) {
            /* Shift characters left */
            CopyMem(&line->text[buffer->cursorX], &line->text[buffer->cursorX - 1], line->length - buffer->cursorX);
        }
        line->length--;
        line->text[line->length] = '\0';
        buffer->cursorX--;
        buffer->modified = TRUE;
        return TRUE;
    } else if (buffer->cursorY > 0) {
        /* Merge with previous line */
        struct TextLine *prevLine = &buffer->lines[buffer->cursorY - 1];
        ULONG prevLen = prevLine->length;
        ULONG currLen = line->length;
        STRPTR newText = NULL;
        ULONG newAlloc = 0;
        
        if (prevLen + currLen + 1 > prevLine->allocated) {
            newAlloc = prevLen + currLen + 256;
            newText = (STRPTR)AllocVec(newAlloc, MEMF_CLEAR);
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
                FreeVec(prevLine->text);
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
            FreeVec(line->text);
        }
        if (buffer->cursorY < buffer->lineCount - 1) {
            /* Move lines down - copy backwards for overlapping memory */
            ULONG moveCount = 0;
            ULONG i = 0;
            moveCount = buffer->lineCount - buffer->cursorY - 1;
            for (i = 0; i < moveCount; i++) {
                buffer->lines[buffer->cursorY + i] = buffer->lines[buffer->cursorY + i + 1];
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

/* Insert newline at cursor position */
BOOL InsertNewline(struct TextBuffer *buffer)
{
    struct TextLine *line = NULL;
    struct TextLine *newLine = NULL;
    ULONG splitPos = 0;
    ULONG remainingLen = 0;
    ULONG i = 0;
    
    if (!buffer || buffer->cursorY >= buffer->lineCount) {
        return FALSE;
    }
    
    /* Expand line array if needed */
    if (buffer->lineCount >= buffer->maxLines) {
        ULONG newMax = 0;
        struct TextLine *newLines = NULL;
        
        newMax = buffer->maxLines * 2;
        newLines = (struct TextLine *)AllocVec(newMax * sizeof(struct TextLine), MEMF_CLEAR);
        if (newLines) {
            for (i = 0; i < buffer->lineCount; i++) {
                newLines[i] = buffer->lines[i];
            }
            FreeVec(buffer->lines);
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
    newLine->text = (STRPTR)AllocVec(newLine->allocated, MEMF_CLEAR);
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

/* Get character width in pixels */
ULONG GetCharWidth(struct RastPort *rp, UBYTE ch)
{
    struct TextFont *font = NULL;
    ULONG width = 0;
    
    if (!rp) {
        return 8;
    }
    
    font = rp->Font;
    if (!font) {
        return 8;
    }
    
    /* Use TextLength for accurate width */
    width = TextLength(rp, (STRPTR)&ch, 1);
    return width;
}

/* Get line height in pixels */
ULONG GetLineHeight(struct RastPort *rp)
{
    struct TextFont *font = NULL;
    ULONG height = 0;
    
    if (!rp || !rp->Font) {
        return 8UL;
    }
    
    font = rp->Font;
    height = (ULONG)font->tf_YSize + (ULONG)font->tf_Baseline;
    return height;
}

/* Scroll to ensure cursor is visible */
VOID ScrollToCursor(struct TextBuffer *buffer, struct Window *window)
{
    struct RastPort *rp = NULL;
    ULONG lineHeight = 0;
    ULONG visibleLines = 0;
    ULONG charWidth = 0;
    ULONG visibleChars = 0;
    ULONG cursorScreenX = 0;
    ULONG cursorScreenY = 0;
    ULONG i = 0;
    
    if (!buffer || !window) {
        return;
    }
    
    rp = window->RPort;
    if (!rp) {
        return;
    }
    
    lineHeight = GetLineHeight(rp);
    visibleLines = (window->Height - window->BorderTop - window->BorderBottom) / lineHeight;
    charWidth = GetCharWidth(rp, 'M');
    visibleChars = (window->Width - window->BorderLeft - window->BorderRight) / charWidth;
    
    /* Calculate cursor screen position */
    cursorScreenY = buffer->cursorY - buffer->scrollY;
    if (buffer->cursorY < buffer->lineCount) {
        for (i = 0; i < buffer->cursorX && i < buffer->lines[buffer->cursorY].length; i++) {
            cursorScreenX += GetCharWidth(rp, (UBYTE)buffer->lines[buffer->cursorY].text[i]);
        }
    }
    cursorScreenX = cursorScreenX / charWidth - buffer->scrollX;
    
    /* Adjust vertical scroll */
    if (cursorScreenY < 0) {
        buffer->scrollY = buffer->cursorY;
    } else if (cursorScreenY >= (LONG)visibleLines) {
        buffer->scrollY = buffer->cursorY - visibleLines + 1;
        if (buffer->scrollY < 0) {
            buffer->scrollY = 0;
        }
    }
    
    /* Adjust horizontal scroll */
    if (cursorScreenX < 0) {
        buffer->scrollX = 0;
        if (buffer->cursorX > 0) {
            buffer->scrollX = buffer->cursorX - visibleChars / 2;
            if (buffer->scrollX < 0) {
                buffer->scrollX = 0;
            }
        }
    } else if (cursorScreenX >= (LONG)visibleChars) {
        buffer->scrollX = buffer->cursorX - visibleChars + 1;
        if (buffer->scrollX < 0) {
            buffer->scrollX = 0;
        }
    }
}

/* Render text to window */
VOID RenderText(struct Window *window, struct TextBuffer *buffer)
{
    struct RastPort *rp = NULL;
    ULONG startY = 0;
    ULONG endY = 0;
    ULONG lineHeight = 0;
    ULONG charWidth = 0;
    ULONG visibleLines = 0;
    ULONG i = 0;
    ULONG y = 0;
    STRPTR lineText = NULL;
    ULONG lineLen = 0;
    ULONG j = 0;
    ULONG textX = 0;
    
    if (!window || !buffer) {
        return;
    }
    
    rp = window->RPort;
    if (!rp) {
        return;
    }
    
    /* Clear window background with pen 2 (grey background) */
    /* On Workbench screen: pen 1 = black, pen 2 = grey */
    /* Use SetBPen and JAM2 mode to fill with background color */
    SetBPen(rp, 2);  /* Background pen (grey) */
    SetAPen(rp, 2);  /* Also set A pen for compatibility */
    SetDrMd(rp, JAM2);  /* Fill mode - use background pen */
    RectFill(rp, window->BorderLeft, window->BorderTop,
             window->Width - 1 - window->BorderRight,
             window->Height - 1 - window->BorderBottom);
    SetDrMd(rp, JAM1);  /* Restore normal text mode */

    lineHeight = GetLineHeight(rp);
    charWidth = GetCharWidth(rp, 'M');
    visibleLines = (window->Height - window->BorderTop - window->BorderBottom) / lineHeight;

    startY = buffer->scrollY;
    endY = startY + visibleLines + 1;
    if (endY > buffer->lineCount) {
        endY = buffer->lineCount;
    }

    /* Render visible lines with pen 1 (black text) */
    SetAPen(rp, 1);
    y = window->BorderTop;
    for (i = startY; i < endY; i++) {
        if (i < buffer->lineCount) {
            lineText = buffer->lines[i].text;
            lineLen = buffer->lines[i].length;
            
            /* Calculate starting X position accounting for scroll */
            textX = window->BorderLeft;
            if (buffer->scrollX > 0 && lineLen > buffer->scrollX) {
                /* Skip scrolled characters */
                for (j = 0; j < buffer->scrollX && j < lineLen; j++) {
                    textX += GetCharWidth(rp, (UBYTE)lineText[j]);
                }
            }
            
            /* Render line text */
            if (lineText && lineLen > 0) {
                Move(rp, textX, y + rp->Font->tf_Baseline);
                Text(rp, &lineText[buffer->scrollX > lineLen ? lineLen : buffer->scrollX], 
                     lineLen - (buffer->scrollX > lineLen ? lineLen : buffer->scrollX));
            }
        }
        y += lineHeight;
    }
}

/* Update cursor display */
VOID UpdateCursor(struct Window *window, struct TextBuffer *buffer)
{
    struct RastPort *rp = NULL;
    ULONG lineHeight = 0;
    ULONG charWidth = 0;
    ULONG i = 0;
    ULONG screenX = 0;
    ULONG screenY = 0;
    ULONG scrollOffset = 0;
    
    if (!window || !buffer) {
        return;
    }
    
    rp = window->RPort;
    if (!rp) {
        return;
    }
    
    lineHeight = GetLineHeight(rp);
    charWidth = GetCharWidth(rp, 'M');
    
    /* Calculate cursor screen position */
    screenY = window->BorderTop + (buffer->cursorY - buffer->scrollY) * lineHeight;
    screenX = window->BorderLeft;
    
    if (buffer->cursorY < buffer->lineCount) {
        /* Calculate X position of cursor in line */
        for (i = 0; i < buffer->cursorX && i < buffer->lines[buffer->cursorY].length; i++) {
            screenX += GetCharWidth(rp, (UBYTE)buffer->lines[buffer->cursorY].text[i]);
        }
        /* Account for horizontal scroll */
        if (buffer->scrollX > 0) {
            scrollOffset = 0;
            for (i = 0; i < buffer->scrollX && i < buffer->lines[buffer->cursorY].length; i++) {
                scrollOffset += GetCharWidth(rp, (UBYTE)buffer->lines[buffer->cursorY].text[i]);
            }
            screenX -= scrollOffset;
        }
    }
    
    /* Draw cursor using XOR mode for visibility */
    SetDrMd(rp, JAM2);
    SetAPen(rp, 1);  /* Use pen 1 (black) for cursor */
    Move(rp, screenX, screenY);
    Draw(rp, screenX, screenY + lineHeight - 1);
    SetDrMd(rp, JAM1);
}

