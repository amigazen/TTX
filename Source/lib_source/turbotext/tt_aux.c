/*
 * turbotext.library - dictionary, templates, macros, string RESULT
 *
 * Aux data is pushed from the driver after DFN load (ClearAuxDefs /
 * AddDictWord / AddTemplate). Editing commands that use that data live here.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "private/tt_internal.h"

#define TT_MACRO_MAX_CMDS  256

struct TTAuxString {
	struct TTAuxString *next;
	STRPTR text;
};

static struct TTAuxString *s_dict = NULL;
static struct TTAuxString *s_templates = NULL;
static struct TTAuxString *s_macro = NULL;

/****************************************************************************/

static ULONG
TT_AuxStrLen(STRPTR s)
{
	ULONG n = 0;

	if (!s)
		return 0;
	while (s[n] != '\0')
		n++;
	return n;
}

static VOID
TT_AuxFreeList(struct TTAuxString **head)
{
	struct TTAuxString *n = NULL;
	struct TTAuxString *next = NULL;

	if (!head)
		return;
	n = *head;
	while (n) {
		next = n->next;
		if (n->text)
			TT_Free(n->text);
		TT_Free(n);
		n = next;
	}
	*head = NULL;
}

static BOOL
TT_AuxAdd(struct TTAuxString **head, STRPTR text)
{
	struct TTAuxString *node = NULL;
	struct TTAuxString *tail = NULL;

	if (!head || !text || text[0] == '\0')
		return FALSE;
	node = (struct TTAuxString *)TT_Alloc(sizeof(struct TTAuxString), MEMF_ANY);
	if (!node)
		return FALSE;
	node->text = TT_DupStr(text);
	if (!node->text) {
		TT_Free(node);
		return FALSE;
	}
	node->next = NULL;
	if (!*head) {
		*head = node;
		return TRUE;
	}
	tail = *head;
	while (tail->next)
		tail = tail->next;
	tail->next = node;
	return TRUE;
}

static STRPTR
TT_MacroLineAt(ULONG idx)
{
	struct TTAuxString *n = NULL;
	ULONG i = 0;

	n = s_macro;
	while (n && i < idx) {
		n = n->next;
		i++;
	}
	if (!n)
		return NULL;
	return n->text;
}

static VOID
TT_MacroClearLines(VOID)
{
	TT_AuxFreeList(&s_macro);
	if (TurboTextBase)
		TurboTextBase->macroCount = 0;
}

VOID
TT_ClearStringResult(VOID)
{
	if (!TurboTextBase)
		return;
	if (TurboTextBase->lastStringResult) {
		TT_Free(TurboTextBase->lastStringResult);
		TurboTextBase->lastStringResult = NULL;
	}
}

VOID
TT_SetStringResult(STRPTR s)
{
	if (!TurboTextBase)
		return;
	TT_ClearStringResult();
	if (!s)
		s = (STRPTR)"";
	TurboTextBase->lastStringResult = TT_DupStr(s);
}

VOID
TT_AuxShutdown(VOID)
{
	TT_AuxFreeList(&s_dict);
	TT_AuxFreeList(&s_templates);
	TT_MacroClearLines();
	TT_ClearStringResult();
	if (TurboTextBase) {
		TurboTextBase->macroRecording = FALSE;
		TurboTextBase->macroPlaying = FALSE;
		TurboTextBase->macroCount = 0;
	}
}

static VOID
TT_TemplateLead(STRPTR text, STRPTR out, ULONG outLen)
{
	ULONG i = 0;

	out[0] = '\0';
	if (!text || outLen == 0)
		return;
	while (text[i] != '\0' && i < outLen - 1) {
		if (text[i] == '(' || text[i] == ' ' || text[i] == '\t' ||
		    text[i] == '*' || text[i] == '@' || text[i] == '_')
			break;
		out[i] = text[i];
		i++;
	}
	out[i] = '\0';
}

STRPTR
TT_MatchTemplate(STRPTR prefix)
{
	struct TTAuxString *t = NULL;
	TEXT lead[64];
	STRPTR found = NULL;
	ULONG matches = 0;
	ULONG plen = 0;

	if (!prefix || prefix[0] == '\0')
		return NULL;
	plen = TT_AuxStrLen(prefix);
	for (t = s_templates; t; t = t->next) {
		if (!t->text)
			continue;
		TT_TemplateLead(t->text, lead, sizeof(lead));
		if (lead[0] == '\0')
			continue;
		if (Strnicmp(lead, prefix, plen) != 0)
			continue;
		matches++;
		found = t->text;
		if (matches > 1)
			return NULL;
	}
	return found;
}

STRPTR
TT_CorrectWordCase(STRPTR word)
{
	struct TTAuxString *w = NULL;

	if (!word || word[0] == '\0')
		return NULL;
	for (w = s_dict; w; w = w->next) {
		if (w->text && Stricmp(w->text, word) == 0)
			return w->text;
	}
	return NULL;
}

static BOOL
TT_OneMismatch(STRPTR a, STRPTR b)
{
	ULONG i = 0;
	ULONG diffs = 0;
	UBYTE ca = 0;
	UBYTE cb = 0;

	if (!a || !b)
		return FALSE;
	while (a[i] != '\0' && b[i] != '\0') {
		ca = (UBYTE)a[i];
		cb = (UBYTE)b[i];
		if (ca >= (UBYTE)'A' && ca <= (UBYTE)'Z')
			ca = (UBYTE)(ca - 'A' + 'a');
		if (cb >= (UBYTE)'A' && cb <= (UBYTE)'Z')
			cb = (UBYTE)(cb - 'A' + 'a');
		if (ca != cb)
			diffs++;
		if (diffs > 1)
			return FALSE;
		i++;
	}
	if (a[i] != '\0' || b[i] != '\0')
		return FALSE;
	return (BOOL)(diffs <= 1);
}

STRPTR
TT_CorrectWord(STRPTR word)
{
	struct TTAuxString *w = NULL;
	STRPTR exact = NULL;
	STRPTR mismatch = NULL;
	STRPTR prefixHit = NULL;
	ULONG prefixHits = 0;
	ULONG wlen = 0;
	UBYTE first = 0;
	UBYTE df = 0;

	if (!word || word[0] == '\0')
		return NULL;

	exact = TT_CorrectWordCase(word);
	if (exact)
		return exact;

	wlen = TT_AuxStrLen(word);
	first = (UBYTE)word[0];
	if (first >= (UBYTE)'A' && first <= (UBYTE)'Z')
		first = (UBYTE)(first - 'A' + 'a');

	for (w = s_dict; w; w = w->next) {
		if (!w->text || w->text[0] == '\0')
			continue;
		df = (UBYTE)w->text[0];
		if (df >= (UBYTE)'A' && df <= (UBYTE)'Z')
			df = (UBYTE)(df - 'A' + 'a');
		if (df != first)
			continue;

		if (TT_AuxStrLen(w->text) == wlen && TT_OneMismatch(w->text, word)) {
			if (!mismatch)
				mismatch = w->text;
		}
		if (Strnicmp(w->text, word, wlen) == 0) {
			prefixHits++;
			prefixHit = w->text;
		}
	}
	if (mismatch)
		return mismatch;
	if (prefixHits == 1)
		return prefixHit;
	return NULL;
}

/****************************************************************************/
/* Command handlers (called from TT_HandleEngineCommand) */

BOOL
TT_Cmd_ClearAuxDefs(VOID)
{
	TT_AuxFreeList(&s_dict);
	TT_AuxFreeList(&s_templates);
	return TRUE;
}

BOOL
TT_Cmd_AddDictWord(STRPTR word)
{
	return TT_AuxAdd(&s_dict, word);
}

BOOL
TT_Cmd_AddTemplate(STRPTR text)
{
	return TT_AuxAdd(&s_templates, text);
}

BOOL
TT_Cmd_GetWord(struct TTTextBuffer *buf)
{
	STRPTR word = NULL;

	word = TT_GetWordAtCursor(buf);
	if (!word) {
		TT_SetStringResult((STRPTR)"");
		return FALSE;
	}
	TT_SetStringResult(word);
	TT_Free(word);
	return TRUE;
}

BOOL
TT_Cmd_CorrectWord(struct TTDocument *doc, struct TTTextBuffer *buf,
	STRPTR *args, ULONG argCount)
{
	STRPTR word = NULL;
	STRPTR owned = NULL;
	STRPTR fixed = NULL;
	BOOL arexxOnly = FALSE;

	if (args && argCount > 0 && args[0] && args[0][0] != '\0') {
		word = args[0];
		arexxOnly = TRUE;
	} else {
		owned = TT_GetWordAtCursor(buf);
		word = owned;
	}
	if (!word || word[0] == '\0') {
		if (owned)
			TT_Free(owned);
		return FALSE;
	}
	fixed = TT_CorrectWord(word);
	if (!fixed) {
		if (owned)
			TT_Free(owned);
		return FALSE;
	}
	if (arexxOnly) {
		TT_SetStringResult(fixed);
		if (owned)
			TT_Free(owned);
		return TRUE;
	}
	if (doc->state.readOnly) {
		if (owned)
			TT_Free(owned);
		return FALSE;
	}
	doc->state.modified = TT_ReplaceWordAtCursor(buf, fixed);
	if (owned)
		TT_Free(owned);
	return doc->state.modified;
}

BOOL
TT_Cmd_CorrectWordCase(struct TTDocument *doc, struct TTTextBuffer *buf,
	STRPTR *args, ULONG argCount)
{
	STRPTR word = NULL;
	STRPTR owned = NULL;
	STRPTR fixed = NULL;
	BOOL arexxOnly = FALSE;

	if (args && argCount > 0 && args[0] && args[0][0] != '\0') {
		word = args[0];
		arexxOnly = TRUE;
	} else {
		owned = TT_GetWordAtCursor(buf);
		word = owned;
	}
	if (!word || word[0] == '\0') {
		if (owned)
			TT_Free(owned);
		return FALSE;
	}
	fixed = TT_CorrectWordCase(word);
	if (!fixed) {
		if (owned)
			TT_Free(owned);
		return FALSE;
	}
	if (arexxOnly) {
		TT_SetStringResult(fixed);
		if (owned)
			TT_Free(owned);
		return TRUE;
	}
	if (doc->state.readOnly) {
		if (owned)
			TT_Free(owned);
		return FALSE;
	}
	doc->state.modified = TT_ReplaceWordAtCursor(buf, fixed);
	if (owned)
		TT_Free(owned);
	return doc->state.modified;
}

BOOL
TT_Cmd_CompleteTemplate(struct TTDocument *doc, struct TTView *view,
	struct TTTextBuffer *buf, STRPTR *args, ULONG argCount)
{
	STRPTR prefix = NULL;
	STRPTR ownedPrefix = NULL;
	STRPTR tmpl = NULL;
	STRPTR emptyArgs[1];
	STRPTR insArgs[1];
	TEXT chBuf[2];
	ULONG i = 0;
	BOOL arexxOnly = FALSE;
	ULONG afterCursor = 0;
	BOOL sawCursor = FALSE;

	(void)view;

	if (args && argCount > 0 && args[0] && args[0][0] != '\0') {
		prefix = args[0];
		arexxOnly = TRUE;
	} else {
		ownedPrefix = TT_GetWordAtCursor(buf);
		prefix = ownedPrefix;
	}
	if (!prefix || prefix[0] == '\0') {
		if (ownedPrefix)
			TT_Free(ownedPrefix);
		return FALSE;
	}
	tmpl = TT_MatchTemplate(prefix);
	if (!tmpl) {
		if (ownedPrefix)
			TT_Free(ownedPrefix);
		return FALSE;
	}
	if (arexxOnly) {
		TT_SetStringResult(tmpl);
		if (ownedPrefix)
			TT_Free(ownedPrefix);
		return TRUE;
	}
	if (doc->state.readOnly) {
		if (ownedPrefix)
			TT_Free(ownedPrefix);
		return FALSE;
	}

	emptyArgs[0] = (STRPTR)"";
	if (!TT_HandleEngineCommand(doc, view, (STRPTR)"ReplaceWord", emptyArgs, 1)) {
		if (ownedPrefix)
			TT_Free(ownedPrefix);
		return FALSE;
	}

	i = 0;
	afterCursor = 0;
	sawCursor = FALSE;
	while (tmpl[i] != '\0') {
		if (tmpl[i] == '*' && tmpl[i + 1] == 'n') {
			if (!TT_HandleEngineCommand(doc, view, (STRPTR)"InsertLine", NULL, 0)) {
				if (ownedPrefix)
					TT_Free(ownedPrefix);
				return FALSE;
			}
			i += 2;
			if (sawCursor)
				afterCursor = 0;
			continue;
		}
		if (tmpl[i] == '@' || tmpl[i] == '_') {
			sawCursor = TRUE;
			afterCursor = 0;
			i++;
			continue;
		}
		chBuf[0] = tmpl[i];
		chBuf[1] = '\0';
		insArgs[0] = chBuf;
		if (!TT_HandleEngineCommand(doc, view, (STRPTR)"Insert", insArgs, 1)) {
			if (ownedPrefix)
				TT_Free(ownedPrefix);
			return FALSE;
		}
		if (sawCursor)
			afterCursor++;
		i++;
	}
	for (i = 0; i < afterCursor; i++) {
		if (!TT_HandleEngineCommand(doc, view, (STRPTR)"MoveLeft", NULL, 0))
			break;
	}
	if (ownedPrefix)
		TT_Free(ownedPrefix);
	return TRUE;
}

BOOL
TT_Cmd_RecordMacro(STRPTR *args, ULONG argCount)
{
	/* QUIET is accepted for compatibility; engine has no console. */
	(void)args;
	(void)argCount;

	if (!TurboTextBase)
		return FALSE;
	if (TurboTextBase->macroPlaying)
		return FALSE;
	TT_MacroClearLines();
	TurboTextBase->macroRecording = TRUE;
	return TRUE;
}

BOOL
TT_Cmd_EndMacro(VOID)
{
	if (!TurboTextBase)
		return FALSE;
	if (!TurboTextBase->macroRecording)
		return FALSE;
	TurboTextBase->macroRecording = FALSE;
	return TRUE;
}

BOOL
TT_Cmd_GetMacroInfo(VOID)
{
	TEXT buf[64];
	ULONG pos = 0;
	STRPTR state = NULL;
	ULONG n = 0;
	ULONG digits = 0;
	TEXT tmp[12];
	ULONG v = 0;

	if (!TurboTextBase)
		return FALSE;
	if (TurboTextBase->macroPlaying)
		state = (STRPTR)"PLAYING";
	else if (TurboTextBase->macroRecording)
		state = (STRPTR)"RECORDING";
	else
		state = (STRPTR)"IDLE";
	while (state[n] != '\0' && pos < sizeof(buf) - 1)
		buf[pos++] = state[n++];
	if (pos < sizeof(buf) - 1)
		buf[pos++] = ' ';
	v = TurboTextBase->macroCount;
	digits = 0;
	if (v == 0)
		tmp[digits++] = '0';
	else {
		while (v > 0 && digits < 11) {
			tmp[digits++] = (TEXT)('0' + (v % 10));
			v /= 10;
		}
	}
	while (digits > 0 && pos < sizeof(buf) - 1)
		buf[pos++] = tmp[--digits];
	buf[pos] = '\0';
	TT_SetStringResult(buf);
	return TRUE;
}

BOOL
TT_Cmd_MacroAppend(STRPTR line)
{
	if (!TurboTextBase || !line)
		return TRUE;
	if (!TurboTextBase->macroRecording || TurboTextBase->macroPlaying)
		return TRUE;
	if (TurboTextBase->macroCount >= TT_MACRO_MAX_CMDS)
		return TRUE;
	if (!TT_AuxAdd(&s_macro, line))
		return TRUE;
	TurboTextBase->macroCount++;
	return TRUE;
}

BOOL
TT_Cmd_MacroClear(VOID)
{
	if (!TurboTextBase)
		return FALSE;
	TT_MacroClearLines();
	TurboTextBase->macroRecording = FALSE;
	return TRUE;
}

BOOL
TT_Cmd_MacroLoadLine(STRPTR line)
{
	if (!TurboTextBase || !line || line[0] == '\0')
		return FALSE;
	if (TurboTextBase->macroCount >= TT_MACRO_MAX_CMDS)
		return FALSE;
	if (!TT_AuxAdd(&s_macro, line))
		return FALSE;
	TurboTextBase->macroCount++;
	return TRUE;
}

BOOL
TT_Cmd_GetMacroLine(STRPTR *args, ULONG argCount)
{
	ULONG idx = 0;
	STRPTR line = NULL;

	if (!TurboTextBase)
		return FALSE;
	if (!args || argCount == 0 || !args[0])
		return FALSE;
	idx = 0;
	{
		ULONG i = 0;
		STRPTR s = args[0];
		while (s[i] >= '0' && s[i] <= '9') {
			idx = idx * 10 + (ULONG)(s[i] - '0');
			i++;
		}
	}
	if (idx >= TurboTextBase->macroCount)
		return FALSE;
	line = TT_MacroLineAt(idx);
	if (!line)
		return FALSE;
	TT_SetStringResult(line);
	return TRUE;
}

BOOL
TT_Cmd_MacroPlayBegin(VOID)
{
	if (!TurboTextBase)
		return FALSE;
	if (TurboTextBase->macroRecording)
		return FALSE;
	if (TurboTextBase->macroCount == 0)
		return FALSE;
	TurboTextBase->macroPlaying = TRUE;
	return TRUE;
}

BOOL
TT_Cmd_MacroPlayEnd(VOID)
{
	if (!TurboTextBase)
		return FALSE;
	TurboTextBase->macroPlaying = FALSE;
	return TRUE;
}
