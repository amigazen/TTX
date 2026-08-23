/*
 * turbotext.library - document and view lifecycle
 *
 * Separates document state (engine-owned) from window bindings (driver-owned).
 * Documents persist independently of Intuition windows per TurboText manual.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "private/tt_internal.h"

/****************************************************************************/

struct TTView *
TT_CreateView(struct TTDocument *doc)
{
	struct TurboTextBase *base = NULL;
	struct TTView *view = NULL;
	ULONG i = 0;

	if (!doc)
		return NULL;

	base = TT_GetBase();
	view = (struct TTView *)TT_Alloc(sizeof(struct TTView), MEMF_ANY);
	if (!view)
		return NULL;

	view->next = doc->views;
	view->viewID = base ? base->nextViewID++ : 1;
	view->cursorX = 0;
	view->cursorY = 0;
	view->scrollX = 0;
	view->scrollY = 0;
	view->active = (doc->viewCount == 0);
	view->uiBinding = NULL;
	view->marking.enabled = FALSE;
	view->marking.startY = 0;
	view->marking.startX = 0;
	view->marking.stopY = 0;
	view->marking.stopX = 0;
	for (i = 0; i < TT_MAX_BOOKMARKS; i++)
	{
		view->bookmarkX[i] = 0;
		view->bookmarkY[i] = 0;
		view->bookmarkSet[i] = 0;
	}
	view->lastChangeX = 0;
	view->lastChangeY = 0;
	view->lastChangeValid = 0;
	view->autoMarkX = 0;
	view->autoMarkY = 0;
	view->autoMarkValid = 0;

	doc->views = view;
	doc->viewCount++;
	if (!doc->activeView || view->active)
		doc->activeView = view;

	return view;
}

/*
 * Copy canonical view caret/scroll/mark into buffer working mirrors
 * before engine text ops that still read buffer->cursor*.
 */
VOID
TT_PushViewToBuffer(struct TTView *view, struct TTTextBuffer *buf)
{
	if (!view || !buf)
		return;

	buf->cursorX = view->cursorX;
	buf->cursorY = view->cursorY;
	buf->scrollX = view->scrollX;
	buf->scrollY = view->scrollY;
	buf->marking = view->marking;
}

/* Write buffer working mirrors back onto the view after engine text ops. */
VOID
TT_PullViewFromBuffer(struct TTView *view, struct TTTextBuffer *buf)
{
	if (!view || !buf)
		return;

	view->cursorX = buf->cursorX;
	view->cursorY = buf->cursorY;
	view->scrollX = buf->scrollX;
	view->scrollY = buf->scrollY;
	view->marking = buf->marking;
}

BOOL
TT_ActivateViewI(struct TTDocument *doc, struct TTView *view)
{
	struct TTView *v = NULL;

	if (!doc || !view)
		return FALSE;

	for (v = doc->views; v; v = v->next)
	{
		if (v == view)
			v->active = TRUE;
		else
			v->active = FALSE;
	}

	doc->activeView = view;
	return TRUE;
}

VOID
TT_FreeView(struct TTView *view)
{
	if (view)
		TT_Free(view);
}

/****************************************************************************/

struct TTDocument *
TT_OpenDocumentI(
	struct TurboTextBase *base,
	STRPTR fileName)
{
	struct TTDocument *doc = NULL;
	struct TTView *view = NULL;
	ULONG titleLen = 0;

	if (!base)
	{
		TT_SetLastError(TTERR_NO_DOCUMENT);
		return NULL;
	}

	doc = (struct TTDocument *)TT_Alloc(sizeof(struct TTDocument), MEMF_ANY);
	if (!doc)
	{
		TT_SetLastError(TTERR_NO_DOCUMENT);
		return NULL;
	}

	doc->docID = base->nextDocID++;
	doc->state.fileName = NULL;
	doc->state.modified = FALSE;
	doc->state.readOnly = FALSE;
	doc->state.loadTime = 0;
	doc->state.fileSize = 0;
	doc->state.fileExists = FALSE;
	doc->views = NULL;
	doc->activeView = NULL;
	doc->viewCount = 0;

	if (!TT_InitTextBuffer(&doc->buffer))
	{
		TT_Free(doc);
		TT_SetLastError(TTERR_NO_DOCUMENT);
		return NULL;
	}

	view = TT_CreateView(doc);
	if (!view)
	{
		TT_FreeTextBuffer(&doc->buffer);
		TT_Free(doc);
		TT_SetLastError(TTERR_NO_DOCUMENT);
		return NULL;
	}

	if (fileName)
	{
		titleLen = 0;
		while (fileName[titleLen] != '\0')
			titleLen++;

		if (titleLen > 0)
		{
			doc->state.fileName = TT_DupStr(fileName);
			if (doc->state.fileName)
			{
				BPTR fileLock = 0;
				struct FileInfoBlock *fib = NULL;
				BPTR oldDir = 0;

				fileLock = Lock(fileName, SHARED_LOCK);
				if (fileLock)
				{
					oldDir = CurrentDir(fileLock);
					fib = (struct FileInfoBlock *)TT_Alloc(
						sizeof(struct FileInfoBlock), MEMF_ANY);
					if (fib && Examine(fileLock, fib))
					{
						doc->state.fileExists = TRUE;
						doc->state.fileSize = fib->fib_Size;
						doc->state.loadTime =
							fib->fib_Date.ds_Days * 86400L +
							fib->fib_Date.ds_Minute * 60L +
							fib->fib_Date.ds_Tick / 50L;
					}
					if (fib)
						TT_Free(fib);
					if (oldDir)
						CurrentDir(oldDir);
					UnLock(fileLock);
				}

				TT_LoadFile(fileName, &doc->buffer);
			}
		}
	}

	/* Link into library document list */
	doc->next = base->documents;
	doc->prev = NULL;
	if (base->documents)
		base->documents->prev = doc;
	base->documents = doc;
	base->docCount++;

	TT_SetLastError(TTERR_NONE);
	return doc;
}

BOOL
TT_CloseDocumentI(
	struct TurboTextBase *base,
	struct TTDocument *doc)
{
	struct TTView *view = NULL;
	struct TTView *nextView = NULL;

	if (!base || !doc)
		return FALSE;

	if (doc->prev)
		doc->prev->next = doc->next;
	else
		base->documents = doc->next;
	if (doc->next)
		doc->next->prev = doc->prev;
	if (base->docCount > 0)
		base->docCount--;

	view = doc->views;
	while (view)
	{
		nextView = view->next;
		TT_FreeView(view);
		view = nextView;
	}

	TT_FreeTextBuffer(&doc->buffer);
	if (doc->state.fileName)
		TT_Free(doc->state.fileName);
	TT_Free(doc);

	return TRUE;
}

struct TTView *
TT_GetActiveViewI(struct TTDocument *doc)
{
	if (!doc)
		return NULL;

	return doc->activeView;
}
