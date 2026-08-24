/*
 * TTX driver - text rendering (Intuition layer)
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_driver.h"
#include "ttx_boopsi.h"
#include "ttx_prefs.h"

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
  struct TTXPrefs *p;

  (void)buffer;
  p = TTX_PrefsGet();
  if (p && p->tabWidth > 0)
    return p->tabWidth;
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

VOID
TTX_PrepareEngineView(struct Session *session, struct Window *window)
{
  struct TTTextBuffer *buffer;
  struct TTView *view;
  struct RastPort *rp;
  ULONG lineHeight;
  ULONG visibleLines;
  ULONG charWidth;
  ULONG textStartX;
  ULONG textEndX;
  ULONG textWidth;
  ULONG maxChars;
  ULONG maxY;
  ULONG drawTop;

  if (!session || !session->document || !TurboTextBase)
    return;
  buffer = TT_SessionBuffer(session);
  view = TTX_SessionView(session);
  if (!buffer || !view)
    return;

  if (window && window->RPort) {
    rp = window->RPort;
    lineHeight = GetLineHeight(rp);
    if (lineHeight < 1)
      lineHeight = 8;
    charWidth = GetCharWidth(rp, 'M');
    if (charWidth < 1)
      charWidth = 8;

    drawTop = window->BorderTop;
    maxY = window->Height - window->BorderBottom;
    if (session->paneClipActive) {
      drawTop = session->paneClipTop;
      maxY = session->paneClipBottom;
    }
    if (maxY > drawTop)
      visibleLines = (maxY - drawTop) / lineHeight;
    else
      visibleLines = 1;
    if (visibleLines < 1)
      visibleLines = 1;
    buffer->pageH = visibleLines;

    textStartX = window->BorderLeft + buffer->leftMargin + 1;
    textEndX = window->Width - window->BorderRight - 1;
    if (textEndX > textStartX) {
      textWidth = textEndX - textStartX + 1;
      maxChars = textWidth / charWidth;
      if (maxChars > 0)
        maxChars--;
      if (maxChars < 1)
        maxChars = 1;
      buffer->pageW = maxChars;
    }
  }
  if (buffer->pageH < 1)
    buffer->pageH = 1;

  buffer->scrollX = view->scrollX;
  buffer->scrollY = view->scrollY;
  buffer->cursorX = view->cursorX;
  buffer->cursorY = view->cursorY;
  buffer->marking = view->marking;

  TT_DoCommand(session->document, view, (STRPTR)"PrepareView", NULL, 0);
}

VOID ScrollToCursor(struct Session *session, struct Window *window) {
  struct TTTextBuffer *buffer = NULL;
  struct TTView *view = NULL;
  struct RastPort *rp = NULL;
  ULONG charWidth = 0;
  ULONG visibleChars = 0;
  LONG cursorScreenX = 0;

  if (!session || !window) {
    return;
  }

  buffer = TT_SessionBuffer(session);
  view = TTX_SessionView(session);
  if (!buffer || !view) {
    return;
  }

  rp = window->RPort;
  if (!rp) {
    return;
  }

  /* Vertical: engine PrepareView / EnsureCursorVisible. */
  TTX_PrepareEngineView(session, window);

  charWidth = GetCharWidth(rp, 'M');
  {
    ULONG textStartX = window->BorderLeft + buffer->leftMargin + 1;
    ULONG textEndX = window->Width - (window->BorderRight + 1);
    ULONG textWidth = textEndX - textStartX + 1;
    visibleChars = charWidth ? textWidth / charWidth : 1;
  }

  if (buffer->lines && view->cursorY < buffer->lineCount) {
    cursorScreenX = (LONG)TTX_MeasureChars(
        rp, buffer->lines[view->cursorY].text, 0, view->cursorX,
        TTX_TabWidth(buffer));
  }
  if (charWidth > 0) {
    cursorScreenX = cursorScreenX / (LONG)charWidth - (LONG)view->scrollX;
  } else {
    cursorScreenX = 0;
  }

  if (cursorScreenX < 0) {
    view->scrollX = 0;
    if (view->cursorX > 0) {
      view->scrollX = view->cursorX - visibleChars / 2;
      if (view->scrollX < 0) {
        view->scrollX = 0;
      }
    }
  } else if (cursorScreenX >= (LONG)visibleChars) {
    view->scrollX = view->cursorX - visibleChars + 1;
    if (view->scrollX < 0) {
      view->scrollX = 0;
    }
  }

  buffer->scrollX = view->scrollX;
  buffer->scrollY = view->scrollY;
  buffer->cursorX = view->cursorX;
  buffer->cursorY = view->cursorY;
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
  struct TTView *view = NULL;
  struct RastPort *rp = NULL;
  struct DrawInfo *dri = NULL;
  ULONG startY = 0;
  ULONG endY = 0;
  ULONG lineHeight = 0;
  ULONG charWidth = 0;
  ULONG visibleLines = 0;
  ULONG i = 0;
  ULONG row = 0;
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
  UBYTE chrome = 0;
  ULONG gutterX = 0;

  (void)scrollDeltaX;
  (void)scrollDeltaY;
  (void)j;
  (void)useScrollLayer;
  (void)startY;
  (void)endY;

  if (!window || !session || !session->document) {
    return;
  }

  buffer = BUF(session);
  view = TTX_SessionView(session);
  if (view) {
    buffer->scrollX = view->scrollX;
    buffer->scrollY = view->scrollY;
    buffer->cursorX = view->cursorX;
    buffer->cursorY = view->cursorY;
    buffer->marking = view->marking;
  }
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

  /* In-window prop scrollers sit inside BorderRight/BorderBottom. */
  textStartX = window->BorderLeft + buffer->leftMargin + 1;
  textEndX = window->Width - window->BorderRight - 1;
  if (textEndX <= textStartX + 8)
    textEndX = textStartX + 8;

  maxY = window->Height - window->BorderBottom;
  if (maxY <= window->BorderTop + 8)
    maxY = window->BorderTop + 8;

  /* Split-pane clip: confine vertical drawing to one pane. */
  {
    ULONG drawTop;

    drawTop = window->BorderTop;
    if (session->paneClipActive) {
      drawTop = session->paneClipTop;
      maxY = session->paneClipBottom;
      if (maxY <= drawTop + 4)
        maxY = drawTop + lineHeight;
    }

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
  textAreaHeight = maxY - drawTop; /* Actual text area height */
  if (textAreaHeight < 0) {
    textAreaHeight = 0;
  }
  visibleLines = textAreaHeight / lineHeight;
  if (visibleLines == 0 && textAreaHeight > 0) {
    visibleLines = 1; /* At least show one line if there's any space */
  }
  buffer->pageH = visibleLines ? visibleLines : 1;

  /* Engine fills view->paint (visible rows + fold chrome). */
  TTX_PrepareEngineView(session, window);
  textStartX = window->BorderLeft + buffer->leftMargin + 1;
  gutterX = window->BorderLeft + 1;

  /* Clear text area with DrawInfo paper colour. */
  TTX_InvalidateCursor(session);
  SetBPen(rp, penBack);
  SetAPen(rp, penBack);
  SetDrMd(rp, JAM2);
  if (maxY > drawTop) {
    RectFill(rp, textStartX - 1, drawTop, textEndX, maxY - 1);
  }
  SetAPen(rp, penText);
  SetBPen(rp, penBack);
  SetDrMd(rp, JAM2);

  y = drawTop;
  for (row = 0; view && row < view->paint.rowCount && y < maxY; row++) {
    chrome = view->paint.rows[row].chrome;
    i = view->paint.rows[row].docY;
    if (1) {
      ULONG selectStartX = 0;
      ULONG selectStopX = 0;
      BOOL lineHasSelection = FALSE;
      ULONG selectStartPixel = 0;
      ULONG selectStopPixel = 0;

      lineText = view->paint.rows[row].text;
      lineLen = view->paint.rows[row].length;

      if (chrome & TTPAINT_GUTTER) {
        SetAPen(rp, penText);
        RectFill(rp, (LONG)gutterX, (LONG)y, (LONG)(gutterX + 1),
                 (LONG)(y + lineHeight - 1));
      }
      if (chrome & TTPAINT_MARKER) {
        TEXT markCh[2];

        markCh[0] = (TEXT)'>';
        markCh[1] = (TEXT)'\0';
        SetAPen(rp, penText);
        SetBPen(rp, penBack);
        Move(rp, (LONG)gutterX, (LONG)(y + rp->Font->tf_Baseline));
        Text(rp, markCh, 1);
      }

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
    linesRendered = view ? view->paint.rowCount : 0;
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
  } /* end drawTop scope */
}

/*
 * Draw text and cursor to the window RPort.
 * When splitRatio > 0, paint both panes (horizontal split).
 */
VOID
TTX_DrawSession(struct Session *session)
{
  struct Window *window;
  struct TTDocument *doc;
  struct TTView *topView;
  struct TTView *botView;
  struct TTView *savedActive;
  struct RastPort *rp;
  ULONG clientTop;
  ULONG clientBot;
  ULONG clientH;
  ULONG mid;
  ULONG barH;

  if (!session || !session->window || session->window == INVALID_RESOURCE)
    return;
  if (!TT_SessionBuffer(session))
    return;

  window = session->window;
  doc = session->document;

  if (session->splitRatio == 0 || !doc || !doc->views || !doc->views->next) {
    session->paneClipActive = FALSE;
    RenderText(window, session);
    UpdateCursor(window, session);
    return;
  }

  topView = doc->views;
  botView = doc->views->next;
  savedActive = doc->activeView;
  clientTop = window->BorderTop;
  clientBot = window->Height - window->BorderBottom;
  if (clientBot <= clientTop + 16)
    clientBot = clientTop + 16;
  clientH = clientBot - clientTop;
  barH = 3;
  mid = clientTop + (clientH * session->splitRatio) / 100;
  if (mid < clientTop + 8)
    mid = clientTop + 8;
  if (mid + barH > clientBot - 8)
    mid = clientBot - barH - 8;
  session->splitY = mid;

  /* Top pane */
  doc->activeView = topView;
  session->paneClipActive = TRUE;
  session->paneClipTop = clientTop;
  session->paneClipBottom = mid;
  RenderText(window, session);
  if (savedActive == topView)
    UpdateCursor(window, session);

  /* Split bar */
  rp = window->RPort;
  if (rp) {
    SetAPen(rp, 1);
    SetDrMd(rp, JAM1);
    RectFill(rp, window->BorderLeft, mid,
      window->Width - window->BorderRight - 1, mid + barH - 1);
  }

  /* Bottom pane */
  doc->activeView = botView;
  session->paneClipTop = mid + barH;
  session->paneClipBottom = clientBot;
  RenderText(window, session);
  if (savedActive == botView)
    UpdateCursor(window, session);

  doc->activeView = savedActive;
  session->paneClipActive = FALSE;
}

/*
 * Repaint after a buffer change. SIMPLE_REFRESH windows accept direct
 * RPort output; SMART_REFRESH would need BeginRefresh/EndRefresh only
 * inside IDCMP_REFRESHWINDOW (see intuition.doc).
 */
VOID
TTX_RequestRedraw(struct Session *session)
{
  if (!session || session->displayLock)
    return;
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
  struct TTView *view;
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
  view = TTX_SessionView(session);
  if (!buffer || !view || !buffer->lines || lineY >= buffer->lineCount)
    return;

  /* Prefer full paint path when folds/visibility may shift screen rows. */
  if (buffer->folds) {
    TTX_RequestRedraw(session);
    return;
  }

  /* Line not visible — nothing to paint. */
  if (lineY < view->scrollY)
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
  textEndX = window->Width - window->BorderRight - 1;
  if (textEndX <= textStartX + 8)
    textEndX = textStartX + 8;

  maxY = window->Height - window->BorderBottom;
  if (maxY <= window->BorderTop + 8)
    maxY = window->BorderTop + 8;

  y = window->BorderTop + (lineY - view->scrollY) * lineHeight;
  if (y + lineHeight > maxY) {
    if (dri)
      FreeScreenDrawInfo(window->WScreen, dri);
    return;
  }

  /* Clear just this line cell.
   * Must XOR-erase the caret first: vertical moves only redraw the new
   * line, so InvalidateCursor alone would leave a COMPLEMENT stamp on
   * the previous line (trails). Horizontal moves redraw the same line
   * and full redraws wipe the area, which is why those looked fine.
   */
  TTX_EraseCursor(window, session);
  SetBPen(rp, penBack);
  SetAPen(rp, penBack);
  SetDrMd(rp, JAM2);
  RectFill(rp, textStartX - 1, y, textEndX, y + lineHeight - 1);

  lineText = buffer->lines[lineY].text;
  lineLen = buffer->lines[lineY].length;
  scrollXPixels = TTX_MeasureChars(
      rp, lineText, 0, view->scrollX, TTX_TabWidth(buffer));

  if (scrollXPixels < textStartX)
    textX = textStartX - scrollXPixels;
  else
    textX = textStartX;

  renderStart = view->scrollX;
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

VOID TTX_InvalidateCursor(struct Session *session)
{
  if (session)
    session->render.cursorVisible = FALSE;
}

VOID TTX_EraseCursor(struct Window *window, struct Session *session)
{
  struct RastPort *rp;

  if (!session || !session->render.cursorVisible)
    return;

  if (window && window->RPort &&
      session->render.cursorPixelW > 0 &&
      session->render.cursorPixelH > 0) {
    rp = window->RPort;
    SetDrMd(rp, COMPLEMENT);
    RectFill(rp,
             (LONG)session->render.cursorPixelX,
             (LONG)session->render.cursorPixelY,
             (LONG)(session->render.cursorPixelX +
                    session->render.cursorPixelW - 1),
             (LONG)(session->render.cursorPixelY +
                    session->render.cursorPixelH - 1));
    SetDrMd(rp, JAM1);
  }

  session->render.cursorVisible = FALSE;
}

/*
 * Original turbotext drew the caret with SetDrMd(COMPLEMENT) + RectFill over
 * the character cell (see output/turbotext.library.c FUN_00009550 /
 * FUN_000099d0: SetDrMd, RectFill, SetDrMd). XOR toggle avoids JAM2 line
 * artifacts: erase by painting the old cell again, then paint the new cell.
 */
VOID UpdateCursor(struct Window *window, struct Session *session) {
  struct TTTextBuffer *buffer = NULL;
  struct TTView *view = NULL;
  struct RastPort *rp = NULL;
  ULONG lineHeight = 0;
  ULONG charWidth = 0;
  ULONG screenX = 0;
  ULONG screenY = 0;
  ULONG scrollOffset = 0;
  ULONG textStartX = 0;
  ULONG cellW = 0;
  UBYTE ch = 0;

  if (!window || !session || !session->document) {
    return;
  }

  buffer = BUF(session);
  view = TTX_SessionView(session);
  if (!buffer || !view) {
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
  if (charWidth < 1)
    charWidth = 8;

  textStartX = window->BorderLeft + buffer->leftMargin + 1;
  if (view->paint.cursorRow != 0xFFFFFFFFUL)
    screenY = window->BorderTop + view->paint.cursorRow * lineHeight;
  else
    screenY = window->BorderTop;
  if (session->paneClipActive) {
    if (view->paint.cursorRow != 0xFFFFFFFFUL)
      screenY = session->paneClipTop + view->paint.cursorRow * lineHeight;
    else
      screenY = session->paneClipTop;
  }
  screenX = textStartX;

  if (buffer->lines && view->cursorY < buffer->lineCount) {
    screenX += TTX_MeasureChars(
        rp, buffer->lines[view->cursorY].text, 0, view->cursorX,
        TTX_TabWidth(buffer));
    if (view->scrollX > 0) {
      scrollOffset = TTX_MeasureChars(
          rp, buffer->lines[view->cursorY].text, 0, view->scrollX,
          TTX_TabWidth(buffer));
      screenX -= scrollOffset;
    }
    if (view->cursorX < buffer->lines[view->cursorY].length &&
        buffer->lines[view->cursorY].text) {
      ch = (UBYTE)buffer->lines[view->cursorY].text[view->cursorX];
      cellW = GetCharWidth(rp, ch);
    } else {
      cellW = charWidth;
    }
  } else {
    cellW = charWidth;
  }
  if (cellW < 1)
    cellW = charWidth;

  /* Off-screen caret: XOR-erase any previous stamp then forget it. */
  if (view->paint.cursorRow == 0xFFFFFFFFUL) {
    TTX_EraseCursor(window, session);
    return;
  }

  /* Erase previous COMPLEMENT cell if it is still in the bitmap. */
  if (session->render.cursorVisible) {
    if (session->render.cursorPixelX == screenX &&
        session->render.cursorPixelY == screenY &&
        session->render.cursorPixelW == cellW &&
        session->render.cursorPixelH == lineHeight) {
      return; /* Already showing at this cell */
    }
    TTX_EraseCursor(window, session);
  }

  SetDrMd(rp, COMPLEMENT);
  RectFill(rp,
           (LONG)screenX,
           (LONG)screenY,
           (LONG)(screenX + cellW - 1),
           (LONG)(screenY + lineHeight - 1));
  SetDrMd(rp, JAM1);

  session->render.cursorVisible = TRUE;
  session->render.cursorPixelX = screenX;
  session->render.cursorPixelY = screenY;
  session->render.cursorPixelW = cellW;
  session->render.cursorPixelH = lineHeight;

  /* Live L#/C# status on the screen title bar. */
  TTX_RefreshStatusBar(session);
}

VOID MouseToCursor(struct Session *session, struct Window *window,
                   LONG mouseX, LONG mouseY, ULONG *cursorX, ULONG *cursorY) {
  struct TTTextBuffer *buffer = NULL;
  struct TTView *view = NULL;
  struct RastPort *rp = NULL;
  ULONG lineHeight = 0;
  ULONG charWidth = 0;
  ULONG textAreaX = 0;
  ULONG textAreaY = 0;
  ULONG textAreaHeight = 0;
  ULONG lineIndex = 0;
  ULONG charIndex = 0;
  ULONG pixelX = 0;
  ULONG pixelY = 0;
  ULONG i = 0;
  ULONG currentX = 0;

  if (!session || !window || !cursorX || !cursorY) {
    return;
  }

  buffer = TT_SessionBuffer(session);
  view = TTX_SessionView(session);
  if (!buffer || !view) {
    return;
  }

  rp = window->RPort;
  if (!rp) {
    return;
  }

  lineHeight = GetLineHeight(rp);
  charWidth = GetCharWidth(rp, 'M');

  textAreaX = window->BorderLeft + buffer->leftMargin + 1;
  textAreaY = window->BorderTop;
  textAreaHeight = window->Height - window->BorderTop - window->BorderBottom;

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

  {
    ULONG visRow;

    TTX_PrepareEngineView(session, window);
    visRow = lineHeight ? (pixelY / lineHeight) : 0;
    if (view->paint.rowCount == 0) {
      lineIndex = 0;
    } else if (visRow >= view->paint.rowCount) {
      lineIndex = view->paint.rows[view->paint.rowCount - 1].docY;
    } else {
      lineIndex = view->paint.rows[visRow].docY;
    }
  }

  *cursorY = lineIndex;

  if (lineIndex < buffer->lineCount) {
    pixelX += view->scrollX * charWidth;
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

    /* Free-form: click past EOL places caret in the virtual column. */
    if (TR_PrefsGet() && TR_PrefsGet()->freeForm && charWidth > 0) {
      ULONG eolPixel;
      ULONG past;

      eolPixel = currentX;
      if (pixelX > eolPixel) {
        past = (pixelX - eolPixel + charWidth / 2) / charWidth;
        *cursorX = charIndex + past;
      }
    }
  } else {
    *cursorX = 0;
  }
}


/*
 * Empty client rect after title bar and size/scroll borders.
 * Matches the text editor BOOPSI domain (GA_Left/Top + RelWidth/RelHeight).
 */
VOID TTX_GetTextClientBounds(
	struct Window *window,
	LONG *outLeft,
	LONG *outTop,
	LONG *outWidth,
	LONG *outHeight)
{
	LONG left;
	LONG top;
	LONG width;
	LONG height;

	left = 0;
	top = 0;
	width = 8;
	height = 8;
	if (window) {
		left = (LONG)window->BorderLeft;
		top = (LONG)window->BorderTop;
		width = (LONG)window->Width - (LONG)window->BorderLeft -
			(LONG)window->BorderRight;
		height = (LONG)window->Height - (LONG)window->BorderTop -
			(LONG)window->BorderBottom;
		if (width < 8)
			width = 8;
		if (height < 8)
			height = 8;
	}
	if (outLeft)
		*outLeft = left;
	if (outTop)
		*outTop = top;
	if (outWidth)
		*outWidth = width;
	if (outHeight)
		*outHeight = height;
}

VOID CalculateMaxScroll(struct Session *session, struct Window *window) {
  struct TTTextBuffer *buffer = NULL;
  struct TTView *view = NULL;
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

  if (!session) {
    return;
  }

  buffer = TT_SessionBuffer(session);
  view = TTX_SessionView(session);
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
  if (scrollBarW < 1)
    scrollBarW = 1;

  lineHeight = GetLineHeight(window->RPort);
  if (lineHeight > 0) {
    visibleLines = (ULONG)(window->Height - window->BorderTop -
                           window->BorderBottom) / lineHeight;
    if (visibleLines < 1)
      visibleLines = 1;
    buffer->pageH = visibleLines;
  } else {
    buffer->pageH = 1;
  }

  /* Engine maxScrollY from visible fold-aware line count. */
  if (session->document && TurboTextBase && view) {
    TT_DoCommand(session->document, view, (STRPTR)"PrepareView", NULL, 0);
  } else if (buffer->lineCount > buffer->pageH) {
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

  if (view) {
    if (view->scrollY > buffer->maxScrollY)
      view->scrollY = buffer->maxScrollY;
    if (view->scrollX > buffer->maxScrollX)
      view->scrollX = buffer->maxScrollX;
    buffer->scrollX = view->scrollX;
    buffer->scrollY = view->scrollY;
  } else {
    if (buffer->scrollY > buffer->maxScrollY)
      buffer->scrollY = buffer->maxScrollY;
    if (buffer->scrollX > buffer->maxScrollX)
      buffer->scrollX = buffer->maxScrollX;
  }
}


