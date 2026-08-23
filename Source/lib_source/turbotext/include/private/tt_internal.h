/*
 * turbotext.library internal declarations
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TURBOTEXT_PRIVATE_TT_INTERNAL_H
#define TURBOTEXT_PRIVATE_TT_INTERNAL_H

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#include "libraries/turbotext.h"

/****************************************************************************/

extern struct TurboTextBase *TurboTextBase;
extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct Library *UtilityBase;
extern APTR SegList;

/* Registered UI hooks (set by TurboTextRun / driver) */
extern struct TTUIHooks *TT_UIHooks;
extern APTR TT_AppCtx;

/****************************************************************************/
/* Memory helpers - explicit alloc/free (no Seiso in engine) */

APTR TT_Alloc(ULONG size, ULONG flags);
VOID TT_Free(APTR ptr);
STRPTR TT_DupStr(STRPTR src);

/****************************************************************************/
/* Document / view lifecycle (internal; not FuncTab LVO entry points) */

struct TTDocument *TT_OpenDocumentI(
	struct TurboTextBase *base,
	STRPTR fileName);
BOOL TT_CloseDocumentI(
	struct TurboTextBase *base,
	struct TTDocument *doc);
struct TTView *TT_GetActiveViewI(struct TTDocument *doc);
ULONG TT_GetLastErrorI(struct TurboTextBase *base);
BOOL TT_DoCommandI(
	struct TurboTextBase *base,
	struct TTDocument *doc,
	struct TTView *view,
	STRPTR command,
	STRPTR *args,
	ULONG argCount);
struct TurboTextBase *TT_GetBase(VOID);
VOID TT_SetLastError(ULONG code);
struct TTView *TT_CreateView(struct TTDocument *doc);
VOID TT_FreeView(struct TTView *view);
BOOL TT_ActivateViewI(struct TTDocument *doc, struct TTView *view);
VOID TT_PushViewToBuffer(struct TTView *view, struct TTTextBuffer *buf);
VOID TT_PullViewFromBuffer(struct TTView *view, struct TTTextBuffer *buf);
BOOL TT_InitTextBuffer(struct TTTextBuffer *buffer);
VOID TT_FreeTextBuffer(struct TTTextBuffer *buffer);

/****************************************************************************/
/* Text buffer operations (engine) */

BOOL TT_LoadFile(STRPTR fileName, struct TTTextBuffer *buffer);
BOOL TT_SaveFile(STRPTR fileName, struct TTTextBuffer *buffer);
BOOL TT_InsertChar(struct TTTextBuffer *buffer, UBYTE ch);
BOOL TT_DeleteChar(struct TTTextBuffer *buffer);
BOOL TT_DeleteForward(struct TTTextBuffer *buffer);
BOOL TT_InsertNewline(struct TTTextBuffer *buffer);
BOOL TT_InsertText(struct TTTextBuffer *buffer, STRPTR text);
BOOL TT_DeleteEOL(struct TTTextBuffer *buffer);
BOOL TT_DeleteEOW(struct TTTextBuffer *buffer);
BOOL TT_DeleteSOL(struct TTTextBuffer *buffer);
BOOL TT_DeleteSOW(struct TTTextBuffer *buffer);
BOOL TT_DeleteLine(struct TTTextBuffer *buffer);
UBYTE TT_GetCharAtCursor(struct TTTextBuffer *buffer);
STRPTR TT_GetCurrentLine(struct TTTextBuffer *buffer);
BOOL TT_SetCharAtCursor(struct TTTextBuffer *buffer, UBYTE ch);
BOOL TT_SwapChars(struct TTTextBuffer *buffer);
BOOL TT_ToggleCharCase(struct TTTextBuffer *buffer);
BOOL TT_ConvertToUpper(struct TTTextBuffer *buffer);
BOOL TT_ConvertToLower(struct TTTextBuffer *buffer);
BOOL TT_ShiftLeft(struct TTTextBuffer *buffer);
BOOL TT_ShiftRight(struct TTTextBuffer *buffer);
BOOL TT_ConvertTabsToSpaces(struct TTTextBuffer *buffer);
BOOL TT_ConvertSpacesToTabs(struct TTTextBuffer *buffer);
BOOL TT_MoveNextWord(struct TTTextBuffer *buffer);
BOOL TT_MovePrevWord(struct TTTextBuffer *buffer);
BOOL TT_MoveEndOfLine(struct TTTextBuffer *buffer);
BOOL TT_MoveStartOfLine(struct TTTextBuffer *buffer);
BOOL TT_MoveEndOfWord(struct TTTextBuffer *buffer);
BOOL TT_MoveStartOfWord(struct TTTextBuffer *buffer);
STRPTR TT_GetWordAtCursor(struct TTTextBuffer *buffer);
BOOL TT_ReplaceWordAtCursor(struct TTTextBuffer *buffer, STRPTR newWord);

/*
 * Case-insensitive forward search.  If skipIfOnMatch is TRUE (normal Find),
 * scanning starts one column past the cursor when already on a match so
 * repeated Find advances.  If FALSE (FindChange), the match under the
 * cursor is eligible for replacement.
 */
BOOL TT_FindTextAt(struct TTTextBuffer *buffer, STRPTR searchStr,
                   ULONG *outY, ULONG *outX, BOOL skipIfOnMatch);
BOOL TT_FindText(struct TTTextBuffer *buffer, STRPTR searchStr,
                 ULONG *outY, ULONG *outX);

/*
 * In-place reformatting of the current line:
 *   TT_CenterLine     – strip leading/trailing spaces and re-center within
 *                       pageW (or 72 columns when pageW == 0).
 *   TT_JustifyLine    – distribute inter-word spaces so the line reaches
 *                       exactly pageW (or 72) columns.
 *   TT_FormatParagraph – collect all words from the paragraph that
 *                       surrounds the cursor (bounded by blank lines or
 *                       buffer edges) and re-flow them to pageW/72 columns.
 */
BOOL TT_CenterLine(struct TTTextBuffer *buffer);
BOOL TT_JustifyLine(struct TTTextBuffer *buffer);
BOOL TT_FormatParagraph(struct TTTextBuffer *buffer);

/****************************************************************************/
/* Block operations */

STRPTR TT_GetBlock(struct TTTextBuffer *buffer);
BOOL TT_DeleteBlock(struct TTTextBuffer *buffer);
VOID TT_MarkAllBlock(struct TTTextBuffer *buffer);
VOID TT_SetMarking(
	struct TTTextBuffer *buffer,
	ULONG startY, ULONG startX, ULONG stopY, ULONG stopX);
VOID TT_ClearMarking(struct TTTextBuffer *buffer);

/****************************************************************************/
/* Command dispatch (engine commands only) */

BOOL TT_HandleEngineCommand(
	struct TTDocument *doc,
	struct TTView *view,
	STRPTR command,
	STRPTR *args,
	ULONG argCount);

/****************************************************************************/
/* TurboTextRun command parser */

LONG TT_ParseAndRun(STRPTR cmdLine);

/****************************************************************************/

#endif /* TURBOTEXT_PRIVATE_TT_INTERNAL_H */
