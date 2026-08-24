/*
 * turbotext.library public API
 *
 * Engine library for TurboText/TTX text editor.
 * Document and view model; command dispatch; text buffer operations.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef LIBRARIES_TURBOTEXT_H
#define LIBRARIES_TURBOTEXT_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef EXEC_LIBRARIES_H
#include <exec/libraries.h>
#endif

/****************************************************************************/

#define TURBOTEXTNAME "turbotext.library"

/* TurboText error codes (subset; mirrors manual LastError values) */
#define TTERR_NONE              0
#define TTERR_UNKNOWN_COMMAND  29
#define TTERR_NO_DOCUMENT      30
#define TTERR_FILE_NOT_FOUND   5

/****************************************************************************/
/* Text buffer types (engine-owned) */

#define TT_MAX_LINES       10000
#define TT_MAX_LINE_LENGTH 4096
/* TurboText default "Tab Width For Editing" (Preferences - Tabs). */
#define TT_DEFAULT_TAB_WIDTH 8

struct TTTextLine {
	STRPTR text;
	ULONG length;
	ULONG allocated;
};

struct TTTextMarking {
	BOOL enabled;
	ULONG startY;
	ULONG startX;
	ULONG stopY;
	ULONG stopX;
};

struct TTTextBuffer {
	struct TTTextLine *lines;
	ULONG lineCount;
	ULONG maxLines;
	/*
	 * Working mirrors of the active TTView caret/scroll/mark while engine
	 * text ops run. Canonical state lives on TTView; use TT_Push/PullView.
	 */
	ULONG cursorX;
	ULONG cursorY;
	ULONG scrollX;
	ULONG scrollY;
	ULONG leftMargin;
	ULONG pageW;
	ULONG pageH;
	ULONG maxScrollX;
	ULONG maxScrollY;
	SHORT scrollXShift;
	SHORT scrollYShift;
	BOOL modified;
	struct TTTextMarking marking;
};

/****************************************************************************/
/* Document metadata */

struct TTDocumentState {
	STRPTR fileName;
	BOOL modified;
	BOOL readOnly;
	ULONG loadTime;
	ULONG fileSize;
	BOOL fileExists;
};

/****************************************************************************/
/* View: per-pane cursor/scroll within a document (manual SplitView model) */

#define TT_MAX_BOOKMARKS 10

struct TTView {
	struct TTView *next;
	ULONG viewID;
	ULONG cursorX;
	ULONG cursorY;
	ULONG scrollX;
	ULONG scrollY;
	BOOL active;
	APTR uiBinding;  /* Opaque driver Session / pane binding */
	struct TTTextMarking marking;
	ULONG bookmarkX[TT_MAX_BOOKMARKS];
	ULONG bookmarkY[TT_MAX_BOOKMARKS];
	UBYTE bookmarkSet[TT_MAX_BOOKMARKS];
	ULONG lastChangeX;
	ULONG lastChangeY;
	UBYTE lastChangeValid;
	ULONG autoMarkX;
	ULONG autoMarkY;
	UBYTE autoMarkValid;
};

/****************************************************************************/
/* Document: text storage + views (engine-owned, survives window close) */

struct TTDocument {
	struct TTDocument *next;
	struct TTDocument *prev;
	ULONG docID;
	struct TTDocumentState state;
	struct TTTextBuffer buffer;
	struct TTView *views;
	struct TTView *activeView;
	ULONG viewCount;
};

/****************************************************************************/
/* Extended library base */

struct TurboTextBase {
	struct Library lib;
	struct TTDocument *documents;
	ULONG docCount;
	ULONG nextDocID;
	ULONG nextViewID;
	ULONG lastError;
	/*
	 * String RESULT channel for ARexx (GetWord, CorrectWord*, CompleteTemplate
	 * with args, GetMacroInfo, GetMacroLine). Owned by the library; valid until
	 * the next TT_DoCommand. Driver copies into its ARexx RESULT buffer.
	 */
	STRPTR lastStringResult;
	/* Recorded-macro state (lines live in engine; driver plays via GetMacroLine). */
	BOOL macroRecording;
	BOOL macroPlaying;
	ULONG macroCount;
};

/****************************************************************************/
/* Driver UI hooks - engine calls driver for Intuition operations */

struct TTUIHooks {
	BOOL (*CreateSession)(APTR appCtx, struct TTDocument *doc, STRPTR fileName);
	VOID (*DestroySession)(APTR appCtx, struct TTDocument *doc);
	VOID (*RefreshView)(APTR appCtx, struct TTDocument *doc, struct TTView *view);
	BOOL (*ActivateDocument)(APTR appCtx, struct TTDocument *doc);
};

/****************************************************************************/
/* Public LVO entry points (client code via proto/turbotext.h; not when building the library) */

#if !defined(TT_SHARED_LIB)

#ifdef __cplusplus
extern "C" {
#endif

LONG TurboTextRun(
	STRPTR cmdLine,
	struct TTUIHooks *hooks,
	APTR appCtx);

struct TTDocument *TT_OpenDocument(STRPTR fileName);
BOOL TT_CloseDocument(struct TTDocument *doc);
BOOL TT_DoCommand(
	struct TTDocument *doc,
	struct TTView *view,
	STRPTR command,
	STRPTR *args,
	ULONG argCount);
struct TTView *TT_GetActiveView(struct TTDocument *doc);
ULONG TT_GetLastError(VOID);

#ifdef __cplusplus
}
#endif

#endif /* !TT_SHARED_LIB */

#if defined(__SASC) && defined(TT_SHARED_LIB)
extern long __ttlibversion;
#endif

/****************************************************************************/

#endif /* LIBRARIES_TURBOTEXT_H */
