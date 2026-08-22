/*
 * turbotext.library - TurboTextRun command parser (LVO -948 equivalent)
 *
 * Parses driver command strings built by TTX per original ttx.asm LAB_0071:
 * OPENDOC WINDOW INIT DEFINITIONS SETTINGS PUBSCREEN FILE ...
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "private/tt_internal.h"

/****************************************************************************/

static STRPTR
SkipSpace(STRPTR s)
{
	if (!s)
		return NULL;
	while (*s == ' ' || *s == '\t')
		s++;
	return s;
}

static ULONG
TokenLen(STRPTR s)
{
	ULONG n = 0;

	if (!s)
		return 0;
	while (s[n] != '\0' && s[n] != ' ' && s[n] != '\t')
		n++;
	return n;
}

static BOOL
MatchToken(STRPTR *cursor, STRPTR token)
{
	STRPTR s = NULL;
	ULONG len = 0;
	ULONG tlen = 0;

	if (!cursor || !*cursor || !token)
		return FALSE;

	s = SkipSpace(*cursor);
	len = TokenLen(s);
	tlen = 0;
	while (token[tlen] != '\0')
		tlen++;

	if (len != tlen)
		return FALSE;

	if (Strnicmp(s, token, len) != 0)
		return FALSE;

	*cursor = s + len;
	return TRUE;
}

static STRPTR
ExtractValue(STRPTR *cursor)
{
	STRPTR s = NULL;
	STRPTR val = NULL;
	ULONG len = 0;

	if (!cursor || !*cursor)
		return NULL;

	s = SkipSpace(*cursor);
	len = TokenLen(s);
	if (len == 0)
		return NULL;

	val = TT_DupStr(s);
	if (val)
		val[len] = '\0';

	*cursor = s + len;
	return val;
}

/****************************************************************************/

LONG
TT_ParseAndRun(STRPTR cmdLine)
{
	STRPTR cursor = NULL;
	STRPTR fileName = NULL;
	STRPTR windowSpec = NULL;
	STRPTR definitions = NULL;
	STRPTR settings = NULL;
	STRPTR pubscreen = NULL;
	struct TTDocument *doc = NULL;
	LONG result = 0;

	if (!cmdLine)
	{
		TT_SetLastError(TTERR_NO_DOCUMENT);
		return -1;
	}

	cursor = cmdLine;

	if (!MatchToken(&cursor, "OPENDOC"))
	{
		/* Accept bare file path for compatibility */
		fileName = TT_DupStr(SkipSpace(cmdLine));
	}
	else
	{
		while (*cursor != '\0')
		{
			if (MatchToken(&cursor, "FILE"))
				fileName = ExtractValue(&cursor);
			else if (MatchToken(&cursor, "WINDOW"))
				windowSpec = ExtractValue(&cursor);
			else if (MatchToken(&cursor, "DEFINITIONS"))
				definitions = ExtractValue(&cursor);
			else if (MatchToken(&cursor, "SETTINGS"))
				settings = ExtractValue(&cursor);
			else if (MatchToken(&cursor, "PUBSCREEN"))
				pubscreen = ExtractValue(&cursor);
			else if (MatchToken(&cursor, "INIT"))
				(void)ExtractValue(&cursor);
			else
			{
				STRPTR tok = ExtractValue(&cursor);
				if (tok)
					TT_Free(tok);
			}
		}
	}

	(void)windowSpec;
	(void)definitions;
	(void)settings;
	(void)pubscreen;

	doc = TT_OpenDocumentI(TT_GetBase(), fileName);
	if (!doc)
	{
		if (fileName)
			TT_Free(fileName);
		if (windowSpec)
			TT_Free(windowSpec);
		if (definitions)
			TT_Free(definitions);
		if (settings)
			TT_Free(settings);
		if (pubscreen)
			TT_Free(pubscreen);
		return -1;
	}

	/* Ask driver to create UI session for this document */
	if (TT_UIHooks && TT_UIHooks->CreateSession)
	{
		if (!TT_UIHooks->CreateSession(TT_AppCtx, doc, fileName))
		{
			TT_CloseDocumentI(TT_GetBase(), doc);
			result = -1;
		}
		else
		{
			result = (LONG)doc->docID;
		}
	}
	else
	{
		result = (LONG)doc->docID;
	}

	if (fileName)
		TT_Free(fileName);
	if (windowSpec)
		TT_Free(windowSpec);
	if (definitions)
		TT_Free(definitions);
	if (settings)
		TT_Free(settings);
	if (pubscreen)
		TT_Free(pubscreen);

	return result;
}
