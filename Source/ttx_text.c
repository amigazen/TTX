/*
 * TTX - Text Buffer and Editing Functions
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx.h"

/* Initial buffer size constant */
#define INITIAL_BUFFER_SIZE 16384

/* Initialize text buffer */
BOOL InitTextBuffer(struct TextBuffer *buffer, struct CleanupStack *stack)
{
    Printf("[INIT] InitTextBuffer: START (buffer=%lx)\n", (ULONG)buffer);
    if (!buffer || !stack) {
        Printf("[INIT] InitTextBuffer: FAIL (buffer=%lx, stack=%lx)\n", (ULONG)buffer, (ULONG)stack);
        return FALSE;
    }
    
    /* All buffer functions use the provided cleanup stack parameter */
    
    /* Allocate initial line array using cleanup stack */
    buffer->maxLines = 1000;
    buffer->lines = (struct TextLine *)allocVec(buffer->maxLines * sizeof(struct TextLine), MEMF_CLEAR);
    if (!buffer->lines) {
        Printf("[INIT] InitTextBuffer: FAIL (allocVec lines failed)\n");
        return FALSE;
    }
    Printf("[INIT] InitTextBuffer: lines=%lx (maxLines=%lu)\n", (ULONG)buffer->lines, buffer->maxLines);
    
    /* Initialize first line */
    buffer->lines[0].allocated = 256;
    buffer->lines[0].text = (STRPTR)allocVec(buffer->lines[0].allocated, MEMF_CLEAR);
    if (!buffer->lines[0].text) {
        Printf("[INIT] InitTextBuffer: FAIL (allocVec line[0].text failed)\n");
        freeVec(buffer->lines);
        buffer->lines = NULL;
        return FALSE;
    }
    Printf("[INIT] InitTextBuffer: line[0].text=%lx\n", (ULONG)buffer->lines[0].text);
    buffer->lines[0].text[0] = '\0';
    buffer->lines[0].length = 0;
    
    buffer->lineCount = 1;
    buffer->cursorX = 0;
    buffer->cursorY = 0;
    buffer->scrollX = 0;
    buffer->scrollY = 0;
    buffer->leftMargin = 0;  /* No left margin initially - can be set for line numbers, etc. */
    buffer->pageW = 0;       /* Will be calculated when window is available */
    buffer->pageH = 0;       /* Will be calculated when window is available */
    buffer->maxScrollX = 0;  /* Will be calculated based on buffer content */
    buffer->maxScrollY = 0;  /* Will be calculated based on buffer content */
    buffer->scrollXShift = 0;  /* No scaling initially */
    buffer->scrollYShift = 0;  /* No scaling initially */
    buffer->modified = FALSE;
    
    /* Initialize graphics v39+ features */
    buffer->superBitMap = NULL;
    buffer->superWidth = 0;
    buffer->superHeight = 0;
    buffer->lastScrollX = 0;
    buffer->lastScrollY = 0;
    buffer->needsFullRedraw = TRUE;
    
    Printf("[INIT] InitTextBuffer: SUCCESS\n");
    return TRUE;
}

/* Free text buffer */
VOID FreeTextBuffer(struct TextBuffer *buffer, struct CleanupStack *stack)
{
    ULONG i = 0;
    struct CleanupStack *cleanupStack = NULL;
    
    Printf("[CLEANUP] FreeTextBuffer: START (buffer=%lx)\n", (ULONG)buffer);
    if (!buffer) {
        Printf("[CLEANUP] FreeTextBuffer: DONE (buffer=NULL)\n");
        return;
    }
    
    /* Use provided cleanup stack */
    cleanupStack = stack;
    
    if (buffer->lines && cleanupStack) {
        Printf("[CLEANUP] FreeTextBuffer: freeing %lu lines\n", buffer->lineCount);
        for (i = 0; i < buffer->lineCount; i++) {
            if (buffer->lines[i].text) {
                Printf("[CLEANUP] FreeTextBuffer: freeing line[%lu].text=%lx\n", i, (ULONG)buffer->lines[i].text);
                freeVec(buffer->lines[i].text);
                buffer->lines[i].text = NULL;
            }
        }
        Printf("[CLEANUP] FreeTextBuffer: freeing lines array=%lx\n", (ULONG)buffer->lines);
        freeVec(buffer->lines);
        buffer->lines = NULL;
    }
    
    buffer->lineCount = 0;
    buffer->maxLines = 0;
    Printf("[CLEANUP] FreeTextBuffer: DONE\n");
}

/* Load file into text buffer */
BOOL LoadFile(STRPTR fileName, struct TextBuffer *buffer, struct CleanupStack *stack)
{
    BPTR fileHandle = NULL;
    UBYTE lineBuffer[MAX_LINE_LENGTH];
    ULONG lineLen = 0;
    ULONG i = 0;
    BOOL result = FALSE;
    
    if (!fileName || !buffer || !stack) {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }
    
    /* Open file for reading using cleanup stack - if file doesn't exist, create empty buffer */
    /* Clear IoErr() before file operations to ensure clean state */
    SetIoErr(0);
    fileHandle = openFile(fileName, MODE_OLDFILE);
    if (!fileHandle) {
        /* File doesn't exist or open failed - check error and clear it */
        LONG errorCode = IoErr();
        if (errorCode != 0) {
            /* Clear error to prevent dos.library from being left in undefined state */
            SetIoErr(0);
        }
        /* Create empty buffer - this is OK if file doesn't exist */
        FreeTextBuffer(buffer, stack);
        if (!InitTextBuffer(buffer, stack)) {
            return FALSE;
        }
        return TRUE;
    } else {
        /* File opened successfully - clear any error code that may have been set */
        SetIoErr(0);
    }
    
    /* Clear existing buffer */
    FreeTextBuffer(buffer, stack);
    if (!InitTextBuffer(buffer, stack)) {
        closeFile(fileHandle);
        return FALSE;
    }
    
    /* Read file line by line */
    /* Clear IoErr() before reading to ensure clean state */
    SetIoErr(0);
    while (FGets(fileHandle, lineBuffer, sizeof(lineBuffer) - 1) != NULL) {
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
            newLines = (struct TextLine *)allocVec(newMax * sizeof(struct TextLine), MEMF_CLEAR);
            if (!newLines) {
                FreeTextBuffer(buffer, stack);
                closeFile(fileHandle);
                return FALSE;
            }
            for (copyIdx = 0; copyIdx < buffer->lineCount; copyIdx++) {
                newLines[copyIdx] = buffer->lines[copyIdx];
            }
            freeVec(buffer->lines);
            buffer->lines = newLines;
            buffer->maxLines = newMax;
        }
        
        /* Allocate line text buffer */
        buffer->lines[i].allocated = lineLen + 256;
        buffer->lines[i].text = (STRPTR)allocVec(buffer->lines[i].allocated, MEMF_CLEAR);
        if (!buffer->lines[i].text) {
            FreeTextBuffer(buffer, stack);
            closeFile(fileHandle);
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
        buffer->lines[i].text = (STRPTR)allocVec(buffer->lines[i].allocated, MEMF_CLEAR);
        if (!buffer->lines[i].text) {
            buffer->lineCount = i;
            break;
        }
        buffer->lines[i].text[0] = '\0';
        buffer->lines[i].length = 0;
    }
    
    /* FGets loop ended - clear any error codes to prevent dos.library corruption */
    /* FGets returns NULL on both EOF and error, so we clear IoErr() regardless */
    SetIoErr(0);
    
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
    closeFile(fileHandle);
    /* Clear IoErr() after closing to prevent dos.library from being left in undefined state */
    SetIoErr(0);
    result = TRUE;
    return result;
}

/* Save text buffer to file */
BOOL SaveFile(STRPTR fileName, struct TextBuffer *buffer, struct CleanupStack *stack)
{
    BPTR fileHandle = NULL;
    ULONG i = 0;
    BOOL result = FALSE;
    
    if (!fileName || !buffer || !stack) {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }
    
    /* Open file for writing using cleanup stack */
    fileHandle = openFile(fileName, MODE_NEWFILE);
    if (!fileHandle) {
        return FALSE;
    }
    
    /* Write each line */
    for (i = 0; i < buffer->lineCount; i++) {
        if (buffer->lines[i].text && buffer->lines[i].length > 0) {
            if (Write(fileHandle, buffer->lines[i].text, buffer->lines[i].length) != buffer->lines[i].length) {
                closeFile(fileHandle);
                return FALSE;
            }
        }
        /* Write newline (except for last line if empty) */
        if (i < buffer->lineCount - 1 || (buffer->lines[i].text && buffer->lines[i].length > 0)) {
            if (Write(fileHandle, "\n", 1) != 1) {
                closeFile(fileHandle);
                return FALSE;
            }
        }
    }
    
    /* Close file using cleanup stack */
    closeFile(fileHandle);
    buffer->modified = FALSE;
    result = TRUE;
    return result;
}

/* Insert character at cursor position */
BOOL InsertChar(struct TextBuffer *buffer, UBYTE ch, struct CleanupStack *stack)
{
    struct TextLine *line = NULL;
    STRPTR newText = NULL;
    ULONG newAlloc = 0;
    
    if (!buffer || !buffer->lines || buffer->cursorY >= buffer->lineCount || !stack) {
        return FALSE;
    }
    
    line = &buffer->lines[buffer->cursorY];
    
    /* Expand line buffer if needed */
    if (line->length + 1 >= line->allocated) {
        newAlloc = line->allocated * 2;
        if (newAlloc < 256) {
            newAlloc = 256;
        }
        newText = (STRPTR)allocVec(newAlloc, MEMF_CLEAR);
        if (!newText) {
            return FALSE;
        }
        if (line->text && line->length > 0) {
            CopyMem(line->text, newText, line->length);
        }
        if (line->text) {
            freeVec(line->text);
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
BOOL DeleteChar(struct TextBuffer *buffer, struct CleanupStack *stack)
{
    struct TextLine *line = NULL;
    
    if (!buffer || !buffer->lines || buffer->cursorY >= buffer->lineCount || !stack) {
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
            newText = (STRPTR)allocVec(newAlloc, MEMF_CLEAR);
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
                freeVec(prevLine->text);
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
            freeVec(line->text);
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
BOOL InsertNewline(struct TextBuffer *buffer, struct CleanupStack *stack)
{
    struct TextLine *line = NULL;
    struct TextLine *newLine = NULL;
    ULONG splitPos = 0;
    ULONG remainingLen = 0;
    ULONG i = 0;
    
    if (!buffer || !buffer->lines || buffer->cursorY >= buffer->lineCount || !stack) {
        return FALSE;
    }
    
    /* Expand line array if needed */
    if (buffer->lineCount >= buffer->maxLines) {
        ULONG newMax = 0;
        struct TextLine *newLines = NULL;
        
        newMax = buffer->maxLines * 2;
        newLines = (struct TextLine *)allocVec(newMax * sizeof(struct TextLine), MEMF_CLEAR);
        if (newLines) {
            for (i = 0; i < buffer->lineCount; i++) {
                newLines[i] = buffer->lines[i];
            }
            freeVec(buffer->lines);
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
    newLine->text = (STRPTR)allocVec(newLine->allocated, MEMF_CLEAR);
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
    LONG cursorScreenX = 0;  /* Can be negative if cursor is to the left of visible area */
    LONG cursorScreenY = 0;  /* Can be negative if cursor is above visible area */
    ULONG i = 0;
    
    if (!buffer || !window) {
        return;
    }
    
    rp = window->RPort;
    if (!rp) {
        return;
    }
    
    lineHeight = GetLineHeight(rp);
    if (lineHeight == 0) {
        return;  /* Can't calculate without valid line height */
    }
    visibleLines = (window->Height - window->BorderTop - window->BorderBottom) / lineHeight;
    if (visibleLines == 0) {
        visibleLines = 1;  /* At least one line visible */
    }
    charWidth = GetCharWidth(rp, 'M');
    
    /* Calculate text area boundaries (accounting for left margin) */
    {
        ULONG textStartX = window->BorderLeft + buffer->leftMargin + 1;
        ULONG textEndX = window->Width - (window->BorderRight + 1);
        ULONG textWidth = textEndX - textStartX + 1;
        visibleChars = textWidth / charWidth;
    }
    
    /* Calculate cursor screen position (relative to visible area) */
    /* cursorScreenY = cursor line - scroll position */
    /* Negative means cursor is above visible area, >= visibleLines means below */
    cursorScreenY = (LONG)buffer->cursorY - (LONG)buffer->scrollY;
    if (buffer->lines && buffer->cursorY < buffer->lineCount) {
        for (i = 0; i < buffer->cursorX && i < buffer->lines[buffer->cursorY].length; i++) {
            cursorScreenX += GetCharWidth(rp, (UBYTE)buffer->lines[buffer->cursorY].text[i]);
        }
    }
    if (charWidth > 0) {
        cursorScreenX = cursorScreenX / charWidth - buffer->scrollX;
    } else {
        cursorScreenX = 0;
    }
    
    /* Adjust vertical scroll to keep cursor visible */
    /* Check if cursor is above visible area (cursorScreenY < 0) */
    if (cursorScreenY < 0) {
        /* Cursor is above visible area - scroll up to show cursor at top */
        ULONG oldScrollY = buffer->scrollY;
        buffer->scrollY = buffer->cursorY;
        if (buffer->scrollY < 0) {
            buffer->scrollY = 0;
        }
        /* Clamp to max scroll */
        if (buffer->maxScrollY > 0 && buffer->scrollY > buffer->maxScrollY) {
            buffer->scrollY = buffer->maxScrollY;
        }
        /* Ensure scroll position actually changed */
        if (buffer->scrollY != oldScrollY) {
            /* Scroll position was updated - this will trigger a redraw */
        }
    } else if (visibleLines > 0 && cursorScreenY >= (LONG)visibleLines) {
        /* Cursor is below visible area - scroll down to show cursor at bottom */
        ULONG oldScrollY = buffer->scrollY;
        buffer->scrollY = buffer->cursorY - visibleLines + 1;
        if (buffer->scrollY < 0) {
            buffer->scrollY = 0;
        }
        /* Clamp to max scroll */
        if (buffer->maxScrollY > 0 && buffer->scrollY > buffer->maxScrollY) {
            buffer->scrollY = buffer->maxScrollY;
        }
        /* Ensure scroll position actually changed */
        if (buffer->scrollY != oldScrollY) {
            /* Scroll position was updated - this will trigger a redraw */
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

/* Create super bitmap for off-screen rendering (Graphics v39+ feature) */
BOOL CreateSuperBitMap(struct TextBuffer *buffer, struct Window *window)
{
    struct BitMap *displayBitMap = NULL;
    ULONG depth = 0;
    ULONG flags = 0;
    ULONG windowWidth = 0;
    ULONG windowHeight = 0;
    ULONG tryWidth = 0;
    ULONG tryHeight = 0;
    ULONG multiplier = 0;
    
    if (!buffer || !window) {
        return FALSE;
    }
    
    /* Check if graphics.library v39+ is available */
    /* GfxBase is a struct GfxBase * which starts with struct Library */
    if (!GfxBase || ((struct Library *)GfxBase)->lib_Version < 39) {
        Printf("[GFX] CreateSuperBitMap: graphics.library v39+ required\n");
        return FALSE;
    }
    
    /* Free existing super bitmap if any */
    if (buffer->superBitMap) {
        FreeBitMap(buffer->superBitMap);
        buffer->superBitMap = NULL;
    }
    
    displayBitMap = window->RPort->BitMap;
    if (!displayBitMap) {
        return FALSE;
    }
    
    /* Get bitmap depth */
    depth = GetBitMapAttr(displayBitMap, BMA_DEPTH);
    if (depth == 0) {
        depth = 4; /* Default to 16 colors if we can't determine */
    }
    
    /* Get window dimensions */
    windowWidth = window->Width;
    windowHeight = window->Height;
    
    /* Progressive allocation strategy: try multiple sizes and memory types */
    /* Strategy 1: Try 1.5x window size with graphics board memory (BMF_DISPLAYABLE) */
    /* Strategy 2: Try 1.5x window size with chip RAM (no BMF_DISPLAYABLE) */
    /* Strategy 3: Try 1.25x window size with chip RAM */
    /* Strategy 4: Try window size + 50% padding with chip RAM */
    /* Strategy 5: Try just window size with chip RAM */
    
    multiplier = 150; /* Start with 1.5x (150%) */
    flags = BMF_DISPLAYABLE; /* Try graphics board first */
    
    while (multiplier >= 100) {
        /* Calculate size based on multiplier (percentage) */
        tryWidth = (windowWidth * multiplier) / 100;
        tryHeight = (windowHeight * multiplier) / 100;
        
        /* Ensure we have at least window size */
        if (tryWidth < windowWidth) {
            tryWidth = windowWidth;
        }
        if (tryHeight < windowHeight) {
            tryHeight = windowHeight;
        }
        
        /* Try allocation */
        buffer->superBitMap = AllocBitMap(tryWidth, tryHeight, depth, flags, displayBitMap);
        
        if (buffer->superBitMap) {
            /* Success! */
            buffer->superWidth = tryWidth;
            buffer->superHeight = tryHeight;
            Printf("[GFX] CreateSuperBitMap: SUCCESS (w=%lu, h=%lu, d=%lu, multiplier=%lu%%, flags=0x%08lx, bitmap=%lx)\n",
                   buffer->superWidth, buffer->superHeight, depth, multiplier, flags, (ULONG)buffer->superBitMap);
            buffer->needsFullRedraw = TRUE;
            return TRUE;
        }
        
        /* If BMF_DISPLAYABLE failed, try without it (chip RAM) */
        if (flags & BMF_DISPLAYABLE) {
            flags = 0; /* Try chip RAM instead */
            /* Retry same size with chip RAM */
            buffer->superBitMap = AllocBitMap(tryWidth, tryHeight, depth, flags, displayBitMap);
            if (buffer->superBitMap) {
                buffer->superWidth = tryWidth;
                buffer->superHeight = tryHeight;
                Printf("[GFX] CreateSuperBitMap: SUCCESS (w=%lu, h=%lu, d=%lu, multiplier=%lu%%, chip RAM, bitmap=%lx)\n",
                       buffer->superWidth, buffer->superHeight, depth, multiplier, (ULONG)buffer->superBitMap);
                buffer->needsFullRedraw = TRUE;
                return TRUE;
            }
            /* Reset flags for next iteration */
            flags = BMF_DISPLAYABLE;
        }
        
        /* Reduce multiplier and try again */
        if (multiplier == 150) {
            multiplier = 125; /* Try 1.25x */
        } else if (multiplier == 125) {
            multiplier = 110; /* Try 1.1x (window + 10%) */
        } else if (multiplier == 110) {
            multiplier = 100; /* Try 1.0x (just window size) */
        } else {
            break; /* Tried all sizes */
        }
    }
    
    /* All allocation attempts failed */
    Printf("[GFX] CreateSuperBitMap: AllocBitMap failed for all sizes (window=%lux%lu, depth=%lu)\n", 
           windowWidth, windowHeight, depth);
    buffer->superWidth = 0;
    buffer->superHeight = 0;
    return FALSE;
}

/* Free super bitmap */
VOID FreeSuperBitMap(struct TextBuffer *buffer)
{
    if (buffer && buffer->superBitMap) {
        FreeBitMap(buffer->superBitMap);
        buffer->superBitMap = NULL;
        buffer->superWidth = 0;
        buffer->superHeight = 0;
    }
}

/* Render text to window using ScrollLayer for optimized scrolling (Graphics v39+) */
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
    ULONG textStartX = 0;
    ULONG textEndX = 0;
    ULONG maxChars = 0;
    ULONG charsToRender = 0;
    ULONG renderStart = 0;
    ULONG textEndPixel = 0;
    ULONG charIdx = 0;
    ULONG maxVisibleChar = 0;
    ULONG actualChars = 0;
    ULONG testX = 0;
    ULONG linesRendered = 0;
    BOOL useScrollLayer = FALSE;
    LONG scrollDeltaX = 0;
    LONG scrollDeltaY = 0;
    ULONG textAreaHeight = 0;  /* Text area height for calculating visible lines */
    ULONG maxY = 0;  /* Maximum Y coordinate for text (stops before bottom border) */
    
    if (!window || !buffer) {
        return;
    }
    
    rp = window->RPort;
    if (!rp) {
        return;
    }
    
    /* Disable ScrollLayer for now - it causes display corruption */
    /* TODO: Properly implement ScrollLayer with correct clipping and exposed area rendering */
    useScrollLayer = FALSE;
    
    lineHeight = GetLineHeight(rp);
    charWidth = GetCharWidth(rp, 'M');
    
    /* Calculate text area boundaries  */
    textStartX = window->BorderLeft + buffer->leftMargin + 1;
    textEndX = window->Width - (window->BorderRight + 1);
    
    /* Calculate maximum Y coordinate for text rendering - must stop before horizontal scroll bar */
    /* The horizontal scroll bar is positioned at the bottom border */
    /* maxY is the maximum Y coordinate where text can be rendered (exclusive) */
    maxY = window->Height - window->BorderBottom;  /* Maximum Y coordinate for text (before scroll bar) */
    
    /* Calculate maximum characters per line (PageW) */
    /* PageW = (Width - BorderRight - (BorderLeft + leftMargin + 1)) / FontX - 1 */
    if (charWidth > 0) {
        ULONG textWidth = 0;
        /* Calculate available text width in pixels */
        if (textEndX >= textStartX) {
            textWidth = textEndX - textStartX + 1;
        } else {
            textWidth = 0;
        }
        /* Convert to characters, subtract 1 for safety */
        maxChars = textWidth / charWidth;
        if (maxChars > 0) {
            maxChars--; 
        }
        buffer->pageW = maxChars;
    } else {
        buffer->pageW = 0;
    }
    
    /* Calculate visible lines - ensure we don't render into bottom border or scroll bar */
    /* Text area height = maxY - top border */
    textAreaHeight = maxY - window->BorderTop;  /* Actual text area height */
    if (textAreaHeight < 0) {
        textAreaHeight = 0;
    }
    visibleLines = textAreaHeight / lineHeight;
    if (visibleLines == 0 && textAreaHeight > 0) {
        visibleLines = 1;  /* At least show one line if there's any space */
    }

    startY = buffer->scrollY;
    endY = startY + visibleLines;
    if (endY > buffer->lineCount) {
        endY = buffer->lineCount;
    }

    /* Set clipping rectangle to prevent rendering outside text area */
    /* This ensures text never renders into window borders */
    /* Note: We'll rely on careful character counting instead of clipping regions */
    /* Clipping regions require SetClipRegion which may not be available in all AmigaOS versions */
    /* We'll measure each character and stop before exceeding textEndX */
    
    /* Clear window background with pen 2 (grey background) */
    /* On Workbench screen: pen 1 = black, pen 2 = grey */
    /* Important: Stop before bottom border to avoid painting over horizontal scroll bar */
    /* maxY was already calculated above */
    SetBPen(rp, 2);  /* Background pen (grey) */
    SetAPen(rp, 2);  /* Also set A pen for compatibility */
    SetDrMd(rp, JAM2);  /* Fill mode - use background pen */
    /* Clear text area - use maxY to ensure we don't paint over scroll bar */
    if (maxY > window->BorderTop) {
        RectFill(rp, textStartX - 1, window->BorderTop,
                 textEndX, maxY - 1);
    }
    SetDrMd(rp, JAM1);  /* Restore normal text mode */
    SetAPen(rp, 1);  /* Set text pen for rendering */
    
    /* Render visible lines with pen 1 (black text) */
    /* Stop rendering before bottom border to avoid painting over scroll bar */
    /* maxY was already calculated above */
    SetAPen(rp, 1);
    y = window->BorderTop;
    for (i = startY; i < endY && y < maxY; i++) {
        if (i < buffer->lineCount) {
            lineText = buffer->lines[i].text;
            lineLen = buffer->lines[i].length;
            
            /* ScollX_PageW = ScrollX + PageW + 1 (maximum character index that should be visible) */
            maxVisibleChar = 0;
            if (buffer->pageW > 0 && buffer->scrollX + buffer->pageW + 1 < lineLen) {
                maxVisibleChar = buffer->scrollX + buffer->pageW + 1;
            } else {
                maxVisibleChar = lineLen;
            }
            
            /* Calculate starting X position (text starts at BorderLeft + leftMargin + 1) */
            /* Calculate pixel offset for horizontal scroll */
            {
                ULONG scrollXPixels = 0;
                ULONG charIdx = 0;
                
                /* Measure pixel width of scrolled characters */
                for (charIdx = 0; charIdx < buffer->scrollX && charIdx < lineLen; charIdx++) {
                    scrollXPixels += GetCharWidth(rp, (UBYTE)lineText[charIdx]);
                }
                
                /* Start text rendering at textStartX minus the scroll offset */
                /* This allows horizontal scrolling by pixel offset */
                if (scrollXPixels < textStartX) {
                    textX = textStartX - scrollXPixels;
                } else {
                    /* Scrolled too far - text starts off-screen */
                    textX = textStartX;
                }
            }
            
            /* Calculate how many characters to render */
            renderStart = buffer->scrollX;
            if (renderStart > lineLen) {
                renderStart = lineLen;
            }
            
            /* Clip to maximum visible character */
            if (renderStart >= maxVisibleChar) {
                /* All text is to the right of visible area - clear entire line */
                charsToRender = 0;
                textEndPixel = textStartX;
            } else {
                /* Calculate how many characters fit */
                charsToRender = lineLen - renderStart;
                if (renderStart + charsToRender > maxVisibleChar) {
                    charsToRender = maxVisibleChar - renderStart;
                }
                
                /* Render line text, clipping to boundary */
                /* Render text up to PageW, then clear remaining area */
                if (lineText && charsToRender > 0 && renderStart < lineLen) {
                    /* Calculate actual characters to render by measuring pixel width */
                    /* We need to ensure text never exceeds textEndX */
                    actualChars = 0;
                    testX = textX;
                    
                    /* Measure how many characters actually fit */
                    for (charIdx = 0; charIdx < charsToRender && (renderStart + charIdx) < lineLen; charIdx++) {
                        ULONG charW = GetCharWidth(rp, (UBYTE)lineText[renderStart + charIdx]);
                        /* Check if adding this character would exceed boundary */
                        if (testX + charW > textEndX) {
                            /* Stop - this character would exceed boundary */
                            break;
                        }
                        testX += charW;
                        actualChars++;
                    }
                    
                    /* Render only the characters that fit */
                    if (actualChars > 0) {
                        Move(rp, textX, y + rp->Font->tf_Baseline);
                        Text(rp, &lineText[renderStart], actualChars);
                        textEndPixel = testX;  /* Use measured width */
                    } else {
                        textEndPixel = textStartX;
                    }
                } else {
                    /* No text to render - start position is textStartX */
                    textEndPixel = textStartX;
                }
            }
            
            /* Clear remaining area after text to exact boundary */
            /* Only clear if we haven't reached the right boundary */
            if (textEndPixel <= textEndX) {
                SetBPen(rp, 2);  /* Background pen (grey) */
                SetAPen(rp, 2);
                SetDrMd(rp, JAM2);
                RectFill(rp, textEndPixel, y,
                         textEndX, y + lineHeight - 1);
                SetDrMd(rp, JAM1);
                SetAPen(rp, 1);  /* Restore text pen */
            }
        }
        y += lineHeight;
    }
    
    /* Clear remaining area below all rendered lines */
    /* Important: Stop before bottom border to avoid painting over horizontal scroll bar */
    {
        linesRendered = endY - startY;
        if (linesRendered > 0 && y < maxY) {
            ULONG clearBottom = maxY - 1;  /* Stop before scroll bar */
            if (clearBottom >= y) {
                SetBPen(rp, 2);
                SetAPen(rp, 2);
                SetDrMd(rp, JAM2);
                RectFill(rp, textStartX - 1, y,
                         textEndX, clearBottom);
                SetDrMd(rp, JAM1);
                SetAPen(rp, 1);
            }
        }
    }
    
    /* Clipping region cleanup - not needed since we're not using it */
    /* We rely on careful character counting to prevent rendering outside boundaries */
    
    /* Update last scroll position if we did a full render */
    if (!useScrollLayer || buffer->needsFullRedraw) {
        buffer->lastScrollX = buffer->scrollX;
        buffer->lastScrollY = buffer->scrollY;
        buffer->needsFullRedraw = FALSE;
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
    
    /* Calculate text start position (accounting for left margin) */
    {
        ULONG textStartX = window->BorderLeft + buffer->leftMargin + 1;
        
        /* Calculate cursor screen position */
        screenY = window->BorderTop + (buffer->cursorY - buffer->scrollY) * lineHeight;
        screenX = textStartX;
    
    if (buffer->lines && buffer->cursorY < buffer->lineCount) {
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
    }  /* End textStartX scope */
    
    /* Draw cursor using XOR mode for visibility */
    SetDrMd(rp, JAM2);
    SetAPen(rp, 1);  /* Use pen 1 (black) for cursor */
    Move(rp, screenX, screenY);
    Draw(rp, screenX, screenY + lineHeight - 1);
    SetDrMd(rp, JAM1);
}

/* Convert mouse pixel coordinates to cursor position */
VOID MouseToCursor(struct TextBuffer *buffer, struct Window *window, LONG mouseX, LONG mouseY, ULONG *cursorX, ULONG *cursorY)
{
    struct RastPort *rp = NULL;
    ULONG lineHeight = 0;
    ULONG charWidth = 0;
    ULONG visibleLines = 0;
    ULONG textAreaX = 0;
    ULONG textAreaY = 0;
    ULONG textAreaWidth = 0;
    ULONG textAreaHeight = 0;
    ULONG lineIndex = 0;
    ULONG charIndex = 0;
    ULONG pixelX = 0;
    ULONG pixelY = 0;
    ULONG i = 0;
    ULONG currentX = 0;
    
    if (!buffer || !window || !cursorX || !cursorY) {
        return;
    }
    
    rp = window->RPort;
    if (!rp) {
        return;
    }
    
    lineHeight = GetLineHeight(rp);
    charWidth = GetCharWidth(rp, 'M');
    
    /* Calculate text area bounds (accounting for left margin) */
    textAreaX = window->BorderLeft + buffer->leftMargin + 1;  /* Text starts after left margin */
    textAreaY = window->BorderTop;
    textAreaWidth = window->Width - window->BorderLeft - window->BorderRight - buffer->leftMargin - 1;
    textAreaHeight = window->Height - window->BorderTop - window->BorderBottom;
    visibleLines = textAreaHeight / lineHeight;
    
    /* Convert mouse coordinates relative to text area */
    {
        LONG relX = mouseX - (LONG)textAreaX;
        LONG relY = mouseY - (LONG)textAreaY;
        
        if (relX < 0) {
            pixelX = 0;
        } else {
            pixelX = (ULONG)relX;
        }
        
        if (relY < 0) {
            pixelY = 0;
        } else {
            pixelY = (ULONG)relY;
        }
    }
    
    /* Calculate line index */
    {
        LONG calcLine = (LONG)buffer->scrollY + ((LONG)pixelY / (LONG)lineHeight);
        if (calcLine < 0) {
            lineIndex = 0;
        } else if ((ULONG)calcLine >= buffer->lineCount) {
            lineIndex = buffer->lineCount > 0 ? buffer->lineCount - 1 : 0;
        } else {
            lineIndex = (ULONG)calcLine;
        }
    }
    
    *cursorY = lineIndex;
    
    /* Calculate character index within line */
    if (lineIndex < buffer->lineCount) {
        /* Account for horizontal scroll */
        pixelX += buffer->scrollX * charWidth;
        
        /* Find character position by measuring text width */
        currentX = 0;
        charIndex = 0;
        
        if (buffer->lines[lineIndex].text && buffer->lines[lineIndex].length > 0) {
            for (i = 0; i < buffer->lines[lineIndex].length; i++) {
                ULONG charW = GetCharWidth(rp, (UBYTE)buffer->lines[lineIndex].text[i]);
                if (currentX + charW / 2 > pixelX) {
                    break;
                }
                currentX += charW;
                charIndex++;
            }
        }
        
        *cursorX = charIndex;
    } else {
        *cursorX = 0;
    }
}

/* Delete character after cursor (Delete key) */
BOOL DeleteForward(struct TextBuffer *buffer, struct CleanupStack *stack)
{
    struct TextLine *line = NULL;
    struct TextLine *nextLine = NULL;
    
    if (!buffer || !buffer->lines || buffer->cursorY >= buffer->lineCount || !stack) {
        return FALSE;
    }
    
    line = &buffer->lines[buffer->cursorY];
    
    /* Delete character after cursor */
    if (buffer->cursorX < line->length) {
        /* Shift characters left */
        if (buffer->cursorX + 1 < line->length) {
            CopyMem(&line->text[buffer->cursorX + 1], &line->text[buffer->cursorX], line->length - buffer->cursorX - 1);
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
            newText = (STRPTR)allocVec(newAlloc, MEMF_CLEAR);
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
                freeVec(line->text);
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
                freeVec(nextLine->text);
        }
        if (buffer->cursorY + 1 < buffer->lineCount - 1) {
            /* Move lines up */
            ULONG moveCount = 0;
            ULONG i = 0;
            moveCount = buffer->lineCount - buffer->cursorY - 2;
            for (i = 0; i < moveCount; i++) {
                buffer->lines[buffer->cursorY + 1 + i] = buffer->lines[buffer->cursorY + 2 + i];
            }
        }
        buffer->lineCount--;
        buffer->modified = TRUE;
        return TRUE;
    }
    
    return FALSE;
}

