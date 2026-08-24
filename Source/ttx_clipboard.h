/*
 * TTX - system clipboard (clipboard.device IFF FTXT) + in-process cache
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TTX_CLIPBOARD_H
#define TTX_CLIPBOARD_H

#include <exec/types.h>

/*
 * Store text in the in-process cache and push FORM FTXT / CHRS to
 * clipboard unit 0 when iffparse.library is available.
 */
VOID TTX_ClipboardSetText(STRPTR text);

/*
 * Prefer fresh FTXT from clipboard.device; fall back to the in-process
 * cache.  Returned pointer is owned by the clipboard module — do not free.
 * May be NULL or empty.
 */
STRPTR TTX_ClipboardGetText(VOID);

/* Write only to clipboard.device (no cache change). TRUE on success. */
BOOL TTX_ClipboardWriteFTXT(STRPTR text);

/*
 * Read FTXT CHRS from clipboard unit 0 into a TTX_Alloc'd string.
 * Caller must TTX_Free.  NULL if none / error.
 */
STRPTR TTX_ClipboardReadFTXT(VOID);

/* Close iffparse.library if we opened it. */
VOID TTX_ClipboardShutdown(VOID);

#endif /* TTX_CLIPBOARD_H */
