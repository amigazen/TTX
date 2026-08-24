/*
 * TTX - clipboard.device IFF FTXT via iffparse.library
 *
 * Mirrors AmigaOS ClipFTXT example: FORM FTXT containing CHRS chunks.
 * Keeps an in-process cache so Copy/Paste still works if the device is
 * busy or iffparse is missing.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_clipboard.h"
#include "ttx_mem.h"

#include <exec/memory.h>
#include <libraries/iffparse.h>
#include <proto/exec.h>
#include <proto/iffparse.h>

#ifndef ID_FTXT
#define ID_FTXT MAKE_ID('F', 'T', 'X', 'T')
#endif
#ifndef ID_CHRS
#define ID_CHRS MAKE_ID('C', 'H', 'R', 'S')
#endif

#ifndef IFFERR_NOMEM
#define IFFERR_NOMEM (-4L)
#endif

#define TTX_CLIP_UNIT 0
#define TTX_CLIP_RBUF 512

struct Library *IFFParseBase = NULL;

static STRPTR TTX_ClipCache = NULL;

static ULONG
TTX_ClipStrLen(STRPTR s)
{
	ULONG n;

	n = 0;
	if (!s)
		return 0;
	while (s[n] != '\0')
		n++;
	return n;
}

static STRPTR
TTX_ClipDup(STRPTR s)
{
	ULONG n;
	STRPTR d;

	if (!s)
		return NULL;
	n = TTX_ClipStrLen(s);
	d = (STRPTR)TTX_Alloc(n + 1, MEMF_CLEAR);
	if (!d)
		return NULL;
	if (n > 0)
		CopyMem(s, d, n);
	d[n] = '\0';
	return d;
}

static BOOL
TTX_ClipEnsureLib(VOID)
{
	if (IFFParseBase)
		return TRUE;
	IFFParseBase = OpenLibrary("iffparse.library", 0L);
	return (IFFParseBase != NULL) ? TRUE : FALSE;
}

VOID
TTX_ClipboardShutdown(VOID)
{
	if (TTX_ClipCache) {
		TTX_Free(TTX_ClipCache);
		TTX_ClipCache = NULL;
	}
	if (IFFParseBase) {
		CloseLibrary(IFFParseBase);
		IFFParseBase = NULL;
	}
}

VOID
TTX_ClipboardSetText(STRPTR text)
{
	if (TTX_ClipCache) {
		TTX_Free(TTX_ClipCache);
		TTX_ClipCache = NULL;
	}
	if (text)
		TTX_ClipCache = TTX_ClipDup(text);
	if (text && text[0] != '\0')
		(void)TTX_ClipboardWriteFTXT(text);
}

STRPTR
TTX_ClipboardGetText(VOID)
{
	STRPTR fromDev;

	fromDev = TTX_ClipboardReadFTXT();
	if (fromDev) {
		if (TTX_ClipCache)
			TTX_Free(TTX_ClipCache);
		TTX_ClipCache = fromDev;
	}
	return TTX_ClipCache;
}

BOOL
TTX_ClipboardWriteFTXT(STRPTR text)
{
	struct IFFHandle *iff;
	struct ClipboardHandle *clip;
	LONG error;
	LONG textlen;
	BOOL ok;

	iff = NULL;
	clip = NULL;
	error = 0;
	ok = FALSE;

	if (!text)
		return FALSE;
	if (!TTX_ClipEnsureLib())
		return FALSE;

	textlen = (LONG)TTX_ClipStrLen(text);

	iff = AllocIFF();
	if (!iff)
		return FALSE;

	clip = OpenClipboard(TTX_CLIP_UNIT);
	if (!clip)
		goto bye;
	iff->iff_Stream = (ULONG)clip;
	InitIFFasClip(iff);

	error = OpenIFF(iff, IFFF_WRITE);
	if (error)
		goto bye;

	error = PushChunk(iff, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN);
	if (!error)
		error = PushChunk(iff, 0, ID_CHRS, IFFSIZE_UNKNOWN);
	if (!error) {
		if (textlen > 0) {
			if (WriteChunkBytes(iff, text, textlen) != textlen)
				error = IFFERR_WRITE;
		}
	}
	if (!error)
		error = PopChunk(iff);
	if (!error)
		error = PopChunk(iff);

	if (!error)
		ok = TRUE;

bye:
	if (iff) {
		CloseIFF(iff);
		if (iff->iff_Stream)
			CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
		FreeIFF(iff);
	}
	return ok;
}

STRPTR
TTX_ClipboardReadFTXT(VOID)
{
	struct IFFHandle *iff;
	struct ClipboardHandle *clip;
	struct ContextNode *cn;
	LONG error;
	LONG rlen;
	STRPTR out;
	ULONG outLen;
	ULONG outAlloc;
	UBYTE readbuf[TTX_CLIP_RBUF];
	STRPTR neu;
	ULONG need;

	iff = NULL;
	clip = NULL;
	error = 0;
	out = NULL;
	outLen = 0;
	outAlloc = 0;

	if (!TTX_ClipEnsureLib())
		return NULL;

	iff = AllocIFF();
	if (!iff)
		return NULL;

	clip = OpenClipboard(TTX_CLIP_UNIT);
	if (!clip)
		goto bye;
	iff->iff_Stream = (ULONG)clip;
	InitIFFasClip(iff);

	error = OpenIFF(iff, IFFF_READ);
	if (error)
		goto bye;

	error = StopChunk(iff, ID_FTXT, ID_CHRS);
	if (error)
		goto bye;

	for (;;) {
		error = ParseIFF(iff, IFFPARSE_SCAN);
		if (error == IFFERR_EOC)
			continue;
		if (error)
			break;

		cn = CurrentChunk(iff);
		if (!cn || cn->cn_Type != ID_FTXT || cn->cn_ID != ID_CHRS)
			continue;

		for (;;) {
			rlen = ReadChunkBytes(iff, readbuf, TTX_CLIP_RBUF);
			if (rlen <= 0) {
				if (rlen < 0)
					error = rlen;
				break;
			}
			need = outLen + (ULONG)rlen + 1;
			if (need > outAlloc) {
				outAlloc = need * 2;
				if (outAlloc < 256)
					outAlloc = 256;
				neu = (STRPTR)TTX_Alloc(outAlloc, MEMF_CLEAR);
				if (!neu) {
					error = IFFERR_NOMEM;
					break;
				}
				if (out && outLen > 0)
					CopyMem(out, neu, outLen);
				if (out)
					TTX_Free(out);
				out = neu;
			}
			CopyMem(readbuf, out + outLen, (ULONG)rlen);
			outLen += (ULONG)rlen;
			out[outLen] = '\0';
		}
		if (error && error != IFFERR_EOF)
			break;
	}

	if (error && error != IFFERR_EOF) {
		if (out) {
			TTX_Free(out);
			out = NULL;
		}
	} else if (out && outLen == 0) {
		TTX_Free(out);
		out = NULL;
	}

bye:
	if (iff) {
		CloseIFF(iff);
		if (iff->iff_Stream)
			CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
		FreeIFF(iff);
	}
	return out;
}
