/*
 * TTX - Block Operations and Word Navigation
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx.h"

/* Forward declarations */
static BOOL IsWordSeparator(UBYTE c);
static VOID NormalizeMarking(struct TextMarking *marking);

/* Check if character is a word separator */
/* Word separators: space, tab, newline, and punctuation */
static BOOL IsWordSeparator(UBYTE c)
{
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        return TRUE;
    }
    /* Check for punctuation */
    if ((c >= '!' && c <= '/') || 
        (c >= ':' && c <= '@') || 
        (c >= '[' && c <= '`') || 
        (c >= '{' && c <= '~')) {
        return TRUE;
    }
    return FALSE;
}

/* Normalize marking so start is before stop */
static VOID NormalizeMarking(struct TextMarking *marking)
{
    ULONG tempY = 0;
    ULONG tempX = 0;
    
    if (!marking || !marking->enabled) {
        return;
    }
    
    /* If stop is before start, swap them */
    if (marking->stopY < marking->startY ||
        (marking->stopY == marking->startY && marking->stopX < marking->startX)) {
        tempY = marking->startY;
        tempX = marking->startX;
        marking->startY = marking->stopY;
        marking->startX = marking->stopX;
        marking->stopY = tempY;
        marking->stopX = tempX;
    }
}

/* ============================================================================
 * Block Operations
 * ============================================================================ */

/* Get selected text block */
STRPTR GetBlock(struct TextBuffer *buffer, struct CleanupStack *stack)
{
    ULONG startY = 0;
    ULONG startX = 0;
    ULONG stopY = 0;
    ULONG stopX = 0;
    ULONG totalLen = 0;
    ULONG i = 0;
    ULONG lineLen = 0;
    STRPTR result = NULL;
    STRPTR ptr = NULL;
    
    if (!buffer || !stack || !buffer->marking.enabled) {
        return NULL;
    }
    
    /* Normalize marking */
    NormalizeMarking(&buffer->marking);
    startY = buffer->marking.startY;
    startX = buffer->marking.startX;
    stopY = buffer->marking.stopY;
    stopX = buffer->marking.stopX;
    
    /* Calculate total length needed */
    if (startY == stopY) {
        /* Single line selection */
        if (stopX > startX && startY < buffer->lineCount) {
            lineLen = buffer->lines[startY].length;
            if (stopX > lineLen) {
                stopX = lineLen;
            }
            if (startX < lineLen) {
                totalLen = stopX - startX;
            }
        }
    } else {
        /* Multi-line selection */
        /* First line */
        if (startY < buffer->lineCount) {
            lineLen = buffer->lines[startY].length;
            if (startX < lineLen) {
                totalLen += lineLen - startX;
            }
        }
        /* Middle lines */
        for (i = startY + 1; i < stopY && i < buffer->lineCount; i++) {
            totalLen += buffer->lines[i].length;
        }
        /* Last line */
        if (stopY < buffer->lineCount) {
            lineLen = buffer->lines[stopY].length;
            if (stopX > lineLen) {
                stopX = lineLen;
            }
            totalLen += stopX;
        }
    }
    
    if (totalLen == 0) {
        return NULL;
    }
    
    /* Allocate memory for result (add 1 for null terminator) */
    result = (STRPTR)allocVec(totalLen + 1, MEMF_CLEAR);
    if (!result) {
        return NULL;
    }
    
    ptr = result;
    
    /* Copy selected text */
    if (startY == stopY) {
        /* Single line selection */
        if (startY < buffer->lineCount && startX < buffer->lines[startY].length) {
            lineLen = stopX - startX;
            if (lineLen > buffer->lines[startY].length - startX) {
                lineLen = buffer->lines[startY].length - startX;
            }
            if (lineLen > 0) {
                CopyMem(&buffer->lines[startY].text[startX], ptr, lineLen);
                ptr += lineLen;
            }
        }
    } else {
        /* Multi-line selection */
        /* First line */
        if (startY < buffer->lineCount) {
            lineLen = buffer->lines[startY].length;
            if (startX < lineLen) {
                ULONG copyLen = lineLen - startX;
                CopyMem(&buffer->lines[startY].text[startX], ptr, copyLen);
                ptr += copyLen;
            }
        }
        /* Middle lines */
        for (i = startY + 1; i < stopY && i < buffer->lineCount; i++) {
            lineLen = buffer->lines[i].length;
            CopyMem(buffer->lines[i].text, ptr, lineLen);
            ptr += lineLen;
        }
        /* Last line */
        if (stopY < buffer->lineCount) {
            lineLen = buffer->lines[stopY].length;
            if (stopX > lineLen) {
                stopX = lineLen;
            }
            if (stopX > 0) {
                CopyMem(buffer->lines[stopY].text, ptr, stopX);
                ptr += stopX;
            }
        }
    }
    
    /* Null terminate */
    *ptr = '\0';
    
    return result;
}

/* Delete selected block */
BOOL DeleteBlock(struct TextBuffer *buffer, struct CleanupStack *stack)
{
    ULONG startY = 0;
    ULONG startX = 0;
    ULONG stopY = 0;
    ULONG stopX = 0;
    ULONG i = 0;
    ULONG lineLen = 0;
    ULONG newLen = 0;
    STRPTR newText = NULL;
    
    if (!buffer || !stack || !buffer->marking.enabled) {
        return FALSE;
    }
    
    /* Normalize marking */
    NormalizeMarking(&buffer->marking);
    startY = buffer->marking.startY;
    startX = buffer->marking.startX;
    stopY = buffer->marking.stopY;
    stopX = buffer->marking.stopX;
    
    if (startY >= buffer->lineCount || stopY >= buffer->lineCount) {
        return FALSE;
    }
    
    if (startY == stopY) {
        /* Single line deletion */
        lineLen = buffer->lines[startY].length;
        if (stopX > lineLen) {
            stopX = lineLen;
        }
        if (startX >= lineLen) {
            /* Nothing to delete */
            buffer->marking.enabled = FALSE;
            return TRUE;
        }
        
        /* Calculate new length */
        newLen = startX + (lineLen - stopX);
        
        /* Allocate new text */
        newText = (STRPTR)allocVec(newLen + 1, MEMF_CLEAR);
        if (!newText) {
            return FALSE;
        }
        
        /* Copy before selection */
        if (startX > 0) {
            CopyMem(buffer->lines[startY].text, newText, startX);
        }
        
        /* Copy after selection */
        if (stopX < lineLen) {
            CopyMem(&buffer->lines[startY].text[stopX], &newText[startX], lineLen - stopX);
        }
        
        /* Replace line text */
        freeVec(buffer->lines[startY].text);
        buffer->lines[startY].text = newText;
        buffer->lines[startY].length = newLen;
        buffer->lines[startY].allocated = newLen + 1;
        
        /* Move cursor to start of deletion */
        buffer->cursorX = startX;
        buffer->cursorY = startY;
    } else {
        /* Multi-line deletion */
        /* Modify first line */
        lineLen = buffer->lines[startY].length;
        if (startX < lineLen) {
            newLen = startX;
            newText = (STRPTR)allocVec(newLen + 1, MEMF_CLEAR);
            if (newText) {
                if (startX > 0) {
                    CopyMem(buffer->lines[startY].text, newText, startX);
                }
                freeVec(buffer->lines[startY].text);
                buffer->lines[startY].text = newText;
                buffer->lines[startY].length = newLen;
                buffer->lines[startY].allocated = newLen + 1;
            }
        }
        
        /* Append last line to first line if needed */
        if (stopY < buffer->lineCount) {
            ULONG appendLen;
            lineLen = buffer->lines[startY].length;
            appendLen = buffer->lines[stopY].length;
            if (stopX < appendLen) {
                appendLen = stopX;
            }
            if (appendLen > 0) {
                ULONG newTotalLen;
                newTotalLen = lineLen + appendLen;
                newText = (STRPTR)allocVec(newTotalLen + 1, MEMF_CLEAR);
                if (newText) {
                    if (lineLen > 0) {
                        CopyMem(buffer->lines[startY].text, newText, lineLen);
                    }
                    CopyMem(buffer->lines[stopY].text, &newText[lineLen], appendLen);
                    freeVec(buffer->lines[startY].text);
                    buffer->lines[startY].text = newText;
                    buffer->lines[startY].length = newTotalLen;
                    buffer->lines[startY].allocated = newTotalLen + 1;
                }
            }
        }
        
        /* Delete middle lines */
        for (i = startY + 1; i <= stopY && i < buffer->lineCount; i++) {
            if (buffer->lines[i].text) {
                freeVec(buffer->lines[i].text);
                buffer->lines[i].text = NULL;
                buffer->lines[i].length = 0;
            }
        }
        
        /* Shift remaining lines up */
        for (i = stopY + 1; i < buffer->lineCount; i++) {
            buffer->lines[startY + 1 + (i - stopY - 1)] = buffer->lines[i];
        }
        
        /* Update line count */
        buffer->lineCount -= (stopY - startY);
        
        /* Move cursor to start of deletion */
        buffer->cursorX = startX;
        buffer->cursorY = startY;
    }
    
    /* Clear marking */
    buffer->marking.enabled = FALSE;
    buffer->modified = TRUE;
    
    return TRUE;
}

/* Mark all text */
VOID MarkAllBlock(struct TextBuffer *buffer)
{
    if (!buffer) {
        return;
    }
    
    buffer->marking.enabled = TRUE;
    buffer->marking.startY = 0;
    buffer->marking.startX = 0;
    if (buffer->lineCount > 0) {
        buffer->marking.stopY = buffer->lineCount - 1;
        buffer->marking.stopX = buffer->lines[buffer->lineCount - 1].length;
    } else {
        buffer->marking.stopY = 0;
        buffer->marking.stopX = 0;
    }
}

/* Set marking from cursor positions */
VOID SetMarking(struct TextBuffer *buffer, ULONG startY, ULONG startX, ULONG stopY, ULONG stopX)
{
    if (!buffer) {
        return;
    }
    
    buffer->marking.enabled = TRUE;
    buffer->marking.startY = startY;
    buffer->marking.startX = startX;
    buffer->marking.stopY = stopY;
    buffer->marking.stopX = stopX;
}

/* Clear marking */
VOID ClearMarking(struct TextBuffer *buffer)
{
    if (!buffer) {
        return;
    }
    
    buffer->marking.enabled = FALSE;
}

/* ============================================================================
 * Word Navigation
 * ============================================================================ */

/* Move to next word */
BOOL MoveNextWord(struct TextBuffer *buffer)
{
    ULONG lineLen = 0;
    
    if (!buffer || buffer->cursorY >= buffer->lineCount) {
        return FALSE;
    }
    
    lineLen = buffer->lines[buffer->cursorY].length;
    
    /* Skip current word if we're in the middle of one */
    while (buffer->cursorX < lineLen && 
           !IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX])) {
        buffer->cursorX++;
    }
    
    /* Skip separators */
    while (buffer->cursorX < lineLen && 
           IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX])) {
        buffer->cursorX++;
    }
    
    /* If we reached end of line, move to next line */
    if (buffer->cursorX >= lineLen) {
        if (buffer->cursorY + 1 < buffer->lineCount) {
            buffer->cursorY++;
            buffer->cursorX = 0;
            /* Skip separators on new line */
            lineLen = buffer->lines[buffer->cursorY].length;
            while (buffer->cursorX < lineLen && 
                   IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX])) {
                buffer->cursorX++;
            }
        } else {
            /* At end of file, stay at end of last line */
            buffer->cursorX = lineLen;
        }
    }
    
    return TRUE;
}

/* Move to previous word */
BOOL MovePrevWord(struct TextBuffer *buffer)
{
    ULONG lineLen = 0;
    BOOL moved = FALSE;
    
    if (!buffer || buffer->cursorY >= buffer->lineCount) {
        return FALSE;
    }
    
    lineLen = buffer->lines[buffer->cursorY].length;
    
    /* If at start of line, move to previous line */
    if (buffer->cursorX == 0) {
        if (buffer->cursorY > 0) {
            buffer->cursorY--;
            lineLen = buffer->lines[buffer->cursorY].length;
            buffer->cursorX = lineLen;
            moved = TRUE;
        } else {
            /* At start of file */
            return FALSE;
        }
    }
    
    /* Skip separators going backwards */
    while (buffer->cursorX > 0 && 
           IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX - 1])) {
        buffer->cursorX--;
        moved = TRUE;
    }
    
    /* Skip word characters going backwards */
    while (buffer->cursorX > 0 && 
           !IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX - 1])) {
        buffer->cursorX--;
        moved = TRUE;
    }
    
    /* If we moved to previous line and are at a separator, try again */
    if (moved && buffer->cursorX > 0 && 
        IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX - 1])) {
        /* Continue searching on previous line */
        if (buffer->cursorY > 0) {
            buffer->cursorY--;
            lineLen = buffer->lines[buffer->cursorY].length;
            buffer->cursorX = lineLen;
            /* Skip separators */
            while (buffer->cursorX > 0 && 
                   IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX - 1])) {
                buffer->cursorX--;
            }
            /* Skip word */
            while (buffer->cursorX > 0 && 
                   !IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX - 1])) {
                buffer->cursorX--;
            }
        }
    }
    
    return TRUE;
}

/* Move to end of line */
BOOL MoveEndOfLine(struct TextBuffer *buffer)
{
    if (!buffer || buffer->cursorY >= buffer->lineCount) {
        return FALSE;
    }
    
    buffer->cursorX = buffer->lines[buffer->cursorY].length;
    return TRUE;
}

/* Move to start of line */
BOOL MoveStartOfLine(struct TextBuffer *buffer)
{
    if (!buffer || buffer->cursorY >= buffer->lineCount) {
        return FALSE;
    }
    
    buffer->cursorX = 0;
    return TRUE;
}

/* Move to end of word */
BOOL MoveEndOfWord(struct TextBuffer *buffer)
{
    ULONG lineLen = 0;
    
    if (!buffer || buffer->cursorY >= buffer->lineCount) {
        return FALSE;
    }
    
    lineLen = buffer->lines[buffer->cursorY].length;
    
    /* If we're in a word, move to end of it */
    if (buffer->cursorX < lineLen && 
        !IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX])) {
        while (buffer->cursorX < lineLen && 
               !IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX])) {
            buffer->cursorX++;
        }
    } else {
        /* We're at a separator, move to next word start then end */
        while (buffer->cursorX < lineLen && 
               IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX])) {
            buffer->cursorX++;
        }
        while (buffer->cursorX < lineLen && 
               !IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX])) {
            buffer->cursorX++;
        }
    }
    
    return TRUE;
}

/* Move to start of word */
BOOL MoveStartOfWord(struct TextBuffer *buffer)
{
    ULONG lineLen = 0;
    
    if (!buffer || buffer->cursorY >= buffer->lineCount) {
        return FALSE;
    }
    
    lineLen = buffer->lines[buffer->cursorY].length;
    
    /* If at start of line, move to previous line */
    if (buffer->cursorX == 0) {
        if (buffer->cursorY > 0) {
            buffer->cursorY--;
            lineLen = buffer->lines[buffer->cursorY].length;
            buffer->cursorX = lineLen;
        } else {
            return FALSE;
        }
    }
    
    /* Skip separators going backwards */
    while (buffer->cursorX > 0 && 
           IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX - 1])) {
        buffer->cursorX--;
    }
    
    /* Skip word characters going backwards */
    while (buffer->cursorX > 0 && 
           !IsWordSeparator(buffer->lines[buffer->cursorY].text[buffer->cursorX - 1])) {
        buffer->cursorX--;
    }
    
    return TRUE;
}
