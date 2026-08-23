/*
 * TTX driver - text rendering (Intuition layer)
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_driver.h"
#include "ttx_boopsi.h"

#define BUF(s) ((s) && (s)->document ? &((s)->document->buffer) : NULL)

ULONG GetCharWidth(struct RastPort *rp, UBYTE ch) {
  static UBYTE chBuf[2];
  ULONG width = 0;

  if (!rp) {
    return 8;
  }

  if (!rp->Font) {
    return 8;
  }

  chBuf[0] = ch;
  chBuf[1] = '\0';
  width = TextLength(rp, (STRPTR)chBuf, 1);
  if (width == 0)
    width = 8;
  return width;
}

ULONG GetLineHeight(struct RastPort *rp) {
  if (!rp || !rp->Font) {
    return 8UL;
  }

  /* tf_YSize is the line cell height; do not add baseline. */
  return (ULONG)rp->Font->tf_YSize;
}

ULONG
TTX_TabWidth(struct TTTextBuffer *buffer)
{
  (void)buffer;
  return TT_DEFAULT_TAB_WIDTH;
}

ULONG
TTX_VisualColumn(STRPTR text, ULONG charIndex, ULONG tabWidth)
{
  ULONG col;
  ULONG i;

  col = 0;
  if (!text || tabWidth < 1)
    return charIndex;

  for (i = 0; i < charIndex && text[i] != '\0'; i++) {
    if ((UBYTE)text[i] == '\t')
      col += tabWidth - (col % tabWidth);
    else
      col++;
  }
  return col;
}

ULONG
TTX_MeasureChars(
  struct RastPort *rp,
  STRPTR text,
  ULONG start,
  ULONG count,
  ULONG tabWidth)
{
  ULONG width;
  ULONG col;
  ULONG i;
  ULONG spaces;
  UBYTE ch;

  width = 0;
  if (!rp || !text || count == 0)
    return 0;
  if (tabWidth < 1)
    tabWidth = TT_DEFAULT_TAB_WIDTH;

  col = TTX_VisualColumn(text, start, tabWidth);
  for (i = 0; i < count && text[start + i] != '\0'; i++) {
    ch = (UBYTE)text[start + i];
    if (ch == '\t') {
      spaces = tabWidth - (col % tabWidth);
      if (spaces == 0)
        spaces = tabWidth;
      width += spaces * GetCharWidth(rp, ' ');
      col += spaces;
    } else {
      width += GetCharWidth(rp, ch);
      col++;
    }
  }
  return width;
}

VOID
TTX_DrawChars(
  struct RastPort *rp,
  LONG x,
  LONG baselineY,
  STRPTR text,
  ULONG start,
  ULONG count,
  ULONG tabWidth,
  ULONG maxX)
{
  ULONG col;
  ULONG i;
  ULONG spaces;
  ULONG j;
  ULONG charW;
  UBYTE ch;
  UBYTE spaceCh;
  LONG cx;

  if (!rp || !text || count == 0 || !rp->Font)
    return;
  if (tabWidth < 1)
    tabWidth = TT_DEFAULT_TAB_WIDTH;

  spaceCh = ' ';
  col = TTX_VisualColumn(text, start, tabWidth);
  cx = x;
  Move(rp, cx, baselineY);

  for (i = 0; i < count && text[start + i] != '\0'; i++) {
    ch = (UBYTE)text[start + i];
    if (ch == '\t') {
      spaces = tabWidth - (col % tabWidth);
      if (spaces == 0)
        spaces = tabWidth;
      charW = GetCharWidth(rp, ' ');
      for (j = 0; j < spaces; j++) {
        if ((ULONG)cx + charW > maxX)
          return;
        Text(rp, (STRPTR)&spaceCh, 1);
        cx += (LONG)charW;
        col++;
      }
      Move(rp, cx, baselineY);
    } else {
      charW = GetCharWidth(rp, ch);
      if ((ULONG)cx + charW > maxX)
        return;
      Text(rp, &text[start + i], 1);
      cx += (LONG)charW;
      col++;
    }
  }
}

VOID ScrollToCursor(struct TTTextBuffer *buffer, struct Window *window) {
  struct RastPort *rp = NULL;
  ULONG lineHeight = 0;
  ULONG visibleLines = 0;
  ULONG charWidth = 0;
  ULONG visibleChars = 0;
  LONG cursorScreenX =
      0; /* Can be negative if cursor is to the left of visible area */
  LONG cursorScreenY = 0; /* Can be negative if cursor is above visible area */
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
    return; /* Can't calculate without valid line height */
  }
  visibleLines =
      (window->Height - window->BorderTop - window->BorderBottom) / lineHeight;
  if (visibleLines == 0) {
    visibleLines = 1; /* At least one line visible */
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
    cursorScreenX = (LONG)TTX_MeasureChars(
        rp, buffer->lines[buffer->cursorY].text, 0, buffer->cursorX,
        TTX_TabWidth(buffer));
  }
  if (charWidth > 0) {
    cursorScreenX = cursorScreenX / (LONG)charWidth - (LONG)buffer->scrollX;
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

BOOL CreateSuperBitMap(struct Session *session, struct Window *window) {
  (void)session;
  (void)window;
  return FALSE;
}

VOID FreeSuperBitMap(struct Session *session) {
  if (session) {
    session->render.superBitMap = NULL;
    session->render.superWidth = 0;
    session->render.superHeight = 0;
  }
}

VOID RenderText(struct Window *window, struct Session *session) {
  struct TTTextBuffer *buffer = NULL;
  struct RastPort *rp = NULL;
  struct DrawInfo *dri = NULL;
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
  ULONG textAreaHeight = 0;
  ULONG maxY = 0;
  UBYTE penText = 1;
  UBYTE penBack = 0;

  (void)scrollDeltaX;
  (void)scrollDeltaY;
  (void)j;
  (void)useScrollLayer;

  if (!window || !session || !session->document) {
    return;
  }

  buffer = BUF(session);
  if (!buffer) {
    return;
  }

  rp = window->RPort;
  if (!rp) {
    return;
  }

  /* Use screen DrawInfo pens (correct for WB/CGX; not hard-coded 0/1/2). */
  dri = GetScreenDrawInfo(window->WScreen);
  if (dri && dri->dri_Pens) {
    penBack = (UBYTE)dri->dri_Pens[BACKGROUNDPEN];
    penText = (UBYTE)dri->dri_Pens[TEXTPEN];
  }
  if (window->RPort && window->WScreen && window->WScreen->RastPort.Font)
    SetFont(rp, window->WScreen->RastPort.Font);

  /* Disable ScrollLayer for now - it causes display corruption */
  /* TODO: Properly implement ScrollLayer with correct clipping and exposed area
   * rendering */
  useScrollLayer = FALSE;

  lineHeight = GetLineHeight(rp);
  if (lineHeight < 1)
    lineHeight = 8;
  charWidth = GetCharWidth(rp, 'M');
  if (charWidth < 1)
    charWidth = 8;

  /* In-window prop scrollers sit inside the client area (not Border*). */
  textStartX = window->BorderLeft + buffer->leftMargin + 1;
  textEndX = window->Width - (window->BorderRight + 18);
  if (textEndX <= textStartX + 8)
    textEndX = window->Width - (window->BorderRight + 1);

  maxY = window->Height - window->BorderBottom - 10;
  if (maxY <= window->BorderTop + 8)
    maxY = window->Height - window->BorderBottom;

  /* Calculate maximum characters per line (PageW) */
  /* PageW = (Width - BorderRight - (BorderLeft + leftMargin + 1)) / FontX - 1
   */
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

  /* Calculate visible lines - ensure we don't render into bottom border or
   * scroll bar */
  /* Text area height = maxY - top border */
  textAreaHeight = maxY - window->BorderTop; /* Actual text area height */
  if (textAreaHeight < 0) {
    textAreaHeight = 0;
  }
  visibleLines = textAreaHeight / lineHeight;
  if (visibleLines == 0 && textAreaHeight > 0) {
    visibleLines = 1; /* At least show one line if there's any space */
  }

  startY = buffer->scrollY;
  endY = startY + visibleLines;
  if (endY > buffer->lineCount) {
    endY = buffer->lineCount;
  }

  /* Set clipping rectangle to prevent rendering outside text area */
  /* This ensures text never renders into window borders */
  /* Note: We'll rely on careful character counting instead of clipping regions
   */
  /* Clipping regions require SetClipRegion which may not be available in all
   * AmigaOS versions */
  /* We'll measure each character and stop before exceeding textEndX */

  /* Clear text area with DrawInfo paper colour. */
  SetBPen(rp, penBack);
  SetAPen(rp, penBack);
  SetDrMd(rp, JAM2);
  if (maxY > window->BorderTop) {
    RectFill(rp, textStartX - 1, window->BorderTop, textEndX, maxY - 1);
  }
  SetAPen(rp, penText);
  SetBPen(rp, penBack);
  SetDrMd(rp, JAM2);

  y = window->BorderTop;
  for (i = startY; i < endY && y < maxY; i++) {
    if (i < buffer->lineCount) {
      ULONG selectStartX = 0;
      ULONG selectStopX = 0;
      BOOL lineHasSelection = FALSE;
      ULONG selectStartPixel = 0;
      ULONG selectStopPixel = 0;

      lineText = buffer->lines[i].text;
      lineLen = buffer->lines[i].length;

      /* Check if this line has selection */
      if (buffer->marking.enabled) {
        ULONG markStartY = buffer->marking.startY;
        ULONG markStartX = buffer->marking.startX;
        ULONG markStopY = buffer->marking.stopY;
        ULONG markStopX = buffer->marking.stopX;

        /* Normalize marking (ensure start is before stop) */
        if (markStopY < markStartY ||
            (markStopY == markStartY && markStopX < markStartX)) {
          ULONG tempY = markStartY;
          ULONG tempX = markStartX;
          markStartY = markStopY;
          markStartX = markStopX;
          markStopY = tempY;
          markStopX = tempX;
        }

        /* Check if this line is within selection */
        if (i >= markStartY && i <= markStopY) {
          if (i == markStartY && i == markStopY) {
            /* Single line selection */
            if (markStartX < lineLen && markStopX > 0) {
              selectStartX = markStartX;
              selectStopX = markStopX;
              if (selectStopX > lineLen) {
                selectStopX = lineLen;
              }
              lineHasSelection = TRUE;
            }
          } else if (i == markStartY) {
            /* First line of multi-line selection */
            if (markStartX < lineLen) {
              selectStartX = markStartX;
              selectStopX = lineLen;
              lineHasSelection = TRUE;
            }
          } else if (i == markStopY) {
            /* Last line of multi-line selection */
            if (markStopX > 0) {
              selectStartX = 0;
              selectStopX = markStopX;
              if (selectStopX > lineLen) {
                selectStopX = lineLen;
              }
              lineHasSelection = TRUE;
            }
          } else {
            /* Middle line of multi-line selection */
            selectStartX = 0;
            selectStopX = lineLen;
            lineHasSelection = TRUE;
          }
        }
      }

      /* ScollX_PageW = ScrollX + PageW + 1 (maximum character index that should
       * be visible) */
      maxVisibleChar = 0;
      if (buffer->pageW > 0 && buffer->scrollX + buffer->pageW + 1 < lineLen) {
        maxVisibleChar = buffer->scrollX + buffer->pageW + 1;
      } else {
        maxVisibleChar = lineLen;
      }

      /* Tab-aware pixel offset for horizontal scroll. */
      {
        ULONG scrollXPixels = 0;

        scrollXPixels = TTX_MeasureChars(
            rp, lineText, 0, buffer->scrollX, TTX_TabWidth(buffer));
        if (scrollXPixels < textStartX)
          textX = textStartX - scrollXPixels;
        else
          textX = textStartX;
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
          for (charIdx = 0;
               charIdx < charsToRender && (renderStart + charIdx) < lineLen;
               charIdx++) {
            ULONG charW = TTX_MeasureChars(
                rp, lineText, renderStart + charIdx, 1, TTX_TabWidth(buffer));
            if (testX + charW > textEndX)
              break;
            testX += charW;
            actualChars++;
          }

          /* Render text in segments if there's a selection on this line */
          if (lineHasSelection && selectStartX < renderStart + actualChars &&
              selectStopX > renderStart) {
            /* Render text with selection highlighting */
            ULONG segStart = renderStart;
            ULONG segEnd = renderStart + actualChars;
            ULONG currentX = textX;
            ULONG charIdx = 0;

            /* Segment 1: Before selection (if any) */
            if (selectStartX > renderStart) {
              ULONG beforeLen = (selectStartX < segEnd)
                                    ? (selectStartX - renderStart)
                                    : actualChars;
              if (beforeLen > 0) {
                SetAPen(rp, penText);
                TTX_DrawChars(
                    rp, (LONG)currentX, (LONG)(y + rp->Font->tf_Baseline),
                    lineText, renderStart, beforeLen, TTX_TabWidth(buffer),
                    textEndX);
                currentX += TTX_MeasureChars(
                    rp, lineText, renderStart, beforeLen, TTX_TabWidth(buffer));
              }
            }

            /* Segment 2: Selection (inverted colors) */
            if (selectStopX > renderStart && selectStartX < segEnd) {
              ULONG selStart =
                  (selectStartX > renderStart) ? selectStartX : renderStart;
              ULONG selEnd = (selectStopX < segEnd) ? selectStopX : segEnd;
              ULONG selLen = selEnd - selStart;

              if (selLen > 0 && selStart < lineLen) {
                /* Draw selection background */
                ULONG selStartPixel = currentX;
                ULONG selStopPixel = currentX;
                ULONG measureIdx = 0;

                selStopPixel += TTX_MeasureChars(
                    rp, lineText, selStart, selLen, TTX_TabWidth(buffer));

                SetBPen(rp, penText);
                SetAPen(rp, penBack);
                SetDrMd(rp, JAM2);
                RectFill(rp, selStartPixel, y, selStopPixel - 1,
                         y + lineHeight - 1);

                SetAPen(rp, penBack);
                TTX_DrawChars(
                    rp, (LONG)selStartPixel, (LONG)(y + rp->Font->tf_Baseline),
                    lineText, selStart, selLen, TTX_TabWidth(buffer), textEndX);

                currentX = selStopPixel;
                SetAPen(rp, penText);
                SetBPen(rp, penBack);
                SetDrMd(rp, JAM2);
              }
            }

            /* Segment 3: After selection (if any) */
            if (selectStopX < segEnd) {
              ULONG afterStart = selectStopX;
              ULONG afterLen = segEnd - afterStart;
              if (afterLen > 0 && afterStart < lineLen) {
                SetAPen(rp, penText);
                TTX_DrawChars(
                    rp, (LONG)currentX, (LONG)(y + rp->Font->tf_Baseline),
                    lineText, afterStart, afterLen, TTX_TabWidth(buffer),
                    textEndX);
                currentX += TTX_MeasureChars(
                    rp, lineText, afterStart, afterLen, TTX_TabWidth(buffer));
              }
            }

            textEndPixel = currentX;
          } else {
            /* No selection on this line - render normally */
            if (actualChars > 0) {
              TTX_DrawChars(
                  rp, (LONG)textX, (LONG)(y + rp->Font->tf_Baseline), lineText,
                  renderStart, actualChars, TTX_TabWidth(buffer), textEndX);
              textEndPixel = testX;
            } else {
              textEndPixel = textStartX;
            }
          }
        } else {
          /* No text to render - start position is textStartX */
          textEndPixel = textStartX;
        }
      }

      /* Clear remaining area after text to exact boundary */
      /* Only clear if we haven't reached the right boundary */
      if (textEndPixel <= textEndX) {
        SetBPen(rp, penBack);
        SetAPen(rp, penBack);
        SetDrMd(rp, JAM2);
        RectFill(rp, textEndPixel, y, textEndX, y + lineHeight - 1);
        SetDrMd(rp, JAM2);
        SetAPen(rp, penText);
        SetBPen(rp, penBack);
      }
    }
    y += lineHeight;
  }

  /* Clear remaining area below all rendered lines */
  /* Important: Stop before bottom border to avoid painting over horizontal
   * scroll bar */
  {
    linesRendered = endY - startY;
    if (linesRendered > 0 && y < maxY) {
      ULONG clearBottom = maxY - 1; /* Stop before scroll bar */
      if (clearBottom >= y) {
        SetBPen(rp, penBack);
        SetAPen(rp, penBack);
        SetDrMd(rp, JAM2);
        RectFill(rp, textStartX - 1, y, textEndX, clearBottom);
        SetDrMd(rp, JAM2);
        SetAPen(rp, penText);
        SetBPen(rp, penBack);
      }
    }
  }

  /* Clipping region cleanup - not needed since we're not using it */
  /* We rely on careful character counting to prevent rendering outside
   * boundaries */

  /* Update last scroll position if we did a full render */
  if (!useScrollLayer || session->render.needsFullRedraw) {
    session->render.lastScrollX = buffer->scrollX;
    session->render.lastScrollY = buffer->scrollY;
    session->render.needsFullRedraw = FALSE;
  }

  if (dri)
    FreeScreenDrawInfo(window->WScreen, dri);
}

/*
 * Draw text and cursor to the window RPort.
 */
VOID
TTX_DrawSession(struct Session *session)
{
  if (!session || !session->window || session->window == INVALID_RESOURCE)
    return;
  if (!TT_SessionBuffer(session))
    return;

  RenderText(session->window, session);
  UpdateCursor(session->window, session);
}

/*
 * Repaint after a buffer change. SIMPLE_REFRESH windows accept direct
 * RPort output; SMART_REFRESH would need BeginRefresh/EndRefresh only
 * inside IDCMP_REFRESHWINDOW (see intuition.doc).
 */
VOID
TTX_RequestRedraw(struct Session *session)
{
  TTX_DrawSession(session);
}

/*
 * Redraw a single buffer line and the cursor. Used for ordinary typing so we
 * do not clear the entire text area on every keystroke.
 */
VOID
TTX_RequestLineRedraw(struct Session *session, ULONG lineY)
{
  struct Window *window;
  struct TTTextBuffer *buffer;
  struct RastPort *rp;
  struct DrawInfo *dri;
  ULONG lineHeight;
  ULONG textStartX;
  ULONG textEndX;
  ULONG maxY;
  ULONG y;
  ULONG textX;
  ULONG scrollXPixels;
  ULONG charIdx;
  ULONG lineLen;
  ULONG renderStart;
  ULONG actualChars;
  ULONG testX;
  ULONG charW;
  STRPTR lineText;
  UBYTE penText;
  UBYTE penBack;

  if (!session || !session->window || session->window == INVALID_RESOURCE)
    return;

  window = session->window;
  buffer = TT_SessionBuffer(session);
  if (!buffer || !buffer->lines || lineY >= buffer->lineCount)
    return;

  /* Line not visible — nothing to paint. */
  if (lineY < buffer->scrollY)
    return;

  rp = window->RPort;
  if (!rp)
    return;

  penText = 1;
  penBack = 0;
  dri = GetScreenDrawInfo(window->WScreen);
  if (dri && dri->dri_Pens) {
    penBack = (UBYTE)dri->dri_Pens[BACKGROUNDPEN];
    penText = (UBYTE)dri->dri_Pens[TEXTPEN];
  }
  if (window->WScreen && window->WScreen->RastPort.Font)
    SetFont(rp, window->WScreen->RastPort.Font);

  lineHeight = GetLineHeight(rp);
  if (lineHeight < 1)
    lineHeight = 8;

  textStartX = window->BorderLeft + buffer->leftMargin + 1;
  textEndX = window->Width - (window->BorderRight + 18);
  if (textEndX <= textStartX + 8)
    textEndX = window->Width - (window->BorderRight + 1);

  maxY = window->Height - window->BorderBottom - 10;
  if (maxY <= window->BorderTop + 8)
    maxY = window->Height - window->BorderBottom;

  y = window->BorderTop + (lineY - buffer->scrollY) * lineHeight;
  if (y + lineHeight > maxY) {
    if (dri)
      FreeScreenDrawInfo(window->WScreen, dri);
    return;
  }

  /* Clear just this line cell. */
  SetBPen(rp, penBack);
  SetAPen(rp, penBack);
  SetDrMd(rp, JAM2);
  RectFill(rp, textStartX - 1, y, textEndX, y + lineHeight - 1);

  lineText = buffer->lines[lineY].text;
  lineLen = buffer->lines[lineY].length;
  scrollXPixels = TTX_MeasureChars(
      rp, lineText, 0, buffer->scrollX, TTX_TabWidth(buffer));

  if (scrollXPixels < textStartX)
    textX = textStartX - scrollXPixels;
  else
    textX = textStartX;

  renderStart = buffer->scrollX;
  if (renderStart > lineLen)
    renderStart = lineLen;

  actualChars = 0;
  testX = textX;
  if (lineText && renderStart < lineLen) {
    for (charIdx = 0; (renderStart + charIdx) < lineLen; charIdx++) {
      charW = TTX_MeasureChars(
          rp, lineText, renderStart + charIdx, 1, TTX_TabWidth(buffer));
      if (testX + charW > textEndX)
        break;
      testX += charW;
      actualChars++;
    }
  }

  SetAPen(rp, penText);
  SetBPen(rp, penBack);
  SetDrMd(rp, JAM2);
  if (actualChars > 0) {
    TTX_DrawChars(
        rp, (LONG)textX, (LONG)(y + rp->Font->tf_Baseline), lineText,
        renderStart, actualChars, TTX_TabWidth(buffer), textEndX);
  }

  if (dri)
    FreeScreenDrawInfo(window->WScreen, dri);

  UpdateCursor(window, session);
}

VOID UpdateCursor(struct Window *window, struct Session *session) {
  struct TTTextBuffer *buffer = NULL;
  struct RastPort *rp = NULL;
  struct DrawInfo *cdri = NULL;
  ULONG lineHeight = 0;
  ULONG charWidth = 0;
  ULONG i = 0;
  ULONG screenX = 0;
  ULONG screenY = 0;
  ULONG scrollOffset = 0;

  if (!window || !session || !session->document) {
    return;
  }

  buffer = BUF(session);
  if (!buffer) {
    return;
  }

  rp = window->RPort;
  if (!rp) {
    return;
  }

  lineHeight = GetLineHeight(rp);
  if (lineHeight < 1)
    lineHeight = 8;
  charWidth = GetCharWidth(rp, 'M');
  (void)charWidth;

  /* Calculate text start position (accounting for left margin) */
  {
    ULONG textStartX = window->BorderLeft + buffer->leftMargin + 1;

    /* Calculate cursor screen position */
    screenY =
        window->BorderTop + (buffer->cursorY - buffer->scrollY) * lineHeight;
    screenX = textStartX;

    if (buffer->lines && buffer->cursorY < buffer->lineCount) {
      screenX += TTX_MeasureChars(
          rp, buffer->lines[buffer->cursorY].text, 0, buffer->cursorX,
          TTX_TabWidth(buffer));
      if (buffer->scrollX > 0) {
        scrollOffset = TTX_MeasureChars(
            rp, buffer->lines[buffer->cursorY].text, 0, buffer->scrollX,
            TTX_TabWidth(buffer));
        screenX -= scrollOffset;
      }
    }
  } /* End textStartX scope */

  SetDrMd(rp, JAM2);
  SetAPen(rp, 1);
  if (window->WScreen) {
    cdri = GetScreenDrawInfo(window->WScreen);
    if (cdri && cdri->dri_Pens) {
      SetAPen(rp, cdri->dri_Pens[TEXTPEN]);
      FreeScreenDrawInfo(window->WScreen, cdri);
    }
  }
  Move(rp, screenX, screenY);
  Draw(rp, screenX, screenY + lineHeight - 1);
  SetDrMd(rp, JAM1);
}

VOID MouseToCursor(struct TTTextBuffer *buffer, struct Window *window,
                   LONG mouseX, LONG mouseY, ULONG *cursorX, ULONG *cursorY) {
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
  textAreaX = window->BorderLeft + buffer->leftMargin +
              1; /* Text starts after left margin */
  textAreaY = window->BorderTop;
  textAreaWidth = window->Width - window->BorderLeft - window->BorderRight -
                  buffer->leftMargin - 1;
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
        ULONG charW = TTX_MeasureChars(
            rp, buffer->lines[lineIndex].text, i, 1, TTX_TabWidth(buffer));
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

VOID CalculateMaxScroll(struct TTTextBuffer *buffer, struct Window *window) {
  ULONG i = 0;
  ULONG maxLineLen = 0;
  ULONG lineHeight = 0;
  ULONG visibleLines = 0;
  ULONG charWidth = 0;
  ULONG textStartX = 0;
  ULONG textEndX = 0;
  ULONG textWidth = 0;
  ULONG maxChars = 0;
  ULONG scrollBarW = 0;
  ULONG lineLimit = 0;

  if (!buffer) {
    return;
  }

  if (!window || !window->RPort) {
    if (buffer->pageH < 1)
      buffer->pageH = 1;
    if (buffer->pageW < 1)
      buffer->pageW = 1;
    return;
  }

  scrollBarW = (ULONG)window->BorderRight;
  if (scrollBarW < TTX_ARROW_SIZE)
    scrollBarW = TTX_ARROW_SIZE;

  lineHeight = GetLineHeight(window->RPort);
  if (lineHeight > 0) {
    visibleLines = (ULONG)(window->Height - window->BorderTop -
                           window->BorderBottom) / lineHeight;
    if (visibleLines > 0)
      visibleLines--;
    if (visibleLines < 1)
      visibleLines = 1;
    buffer->pageH = visibleLines;
  } else {
    buffer->pageH = 1;
  }

  if (buffer->lineCount > buffer->pageH) {
    buffer->maxScrollY = buffer->lineCount - buffer->pageH;
  } else {
    buffer->maxScrollY = 0;
  }

  charWidth = GetCharWidth(window->RPort, 'M');
  textStartX = (ULONG)window->BorderLeft + buffer->leftMargin + 1;
  textEndX = (ULONG)window->Width - scrollBarW - 1;

  if (charWidth > 0 && textEndX >= textStartX) {
    textWidth = textEndX - textStartX + 1;
    maxChars = textWidth / charWidth;
    if (maxChars > 0)
      maxChars--;
    if (maxChars < 1)
      maxChars = 1;
    buffer->pageW = maxChars;
  } else {
    buffer->pageW = 1;
  }

  maxLineLen = 0;
  if (buffer->lines && buffer->lineCount > 0) {
    lineLimit = buffer->lineCount;
    if (lineLimit > buffer->maxLines)
      lineLimit = buffer->maxLines;
    if (lineLimit > TT_MAX_LINES)
      lineLimit = TT_MAX_LINES;
    for (i = 0; i < lineLimit; i++) {
      if (buffer->lines[i].length > maxLineLen)
        maxLineLen = buffer->lines[i].length;
    }
  }

  if (maxLineLen > buffer->pageW) {
    buffer->maxScrollX = maxLineLen - buffer->pageW;
  } else {
    buffer->maxScrollX = 0;
  }

  if (buffer->scrollY > buffer->maxScrollY)
    buffer->scrollY = buffer->maxScrollY;
  if (buffer->scrollX > buffer->maxScrollX)
    buffer->scrollX = buffer->maxScrollX;
}

