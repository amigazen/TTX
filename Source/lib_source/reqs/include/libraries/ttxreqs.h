#ifndef LIBRARIES_TTXREQS_H
#define LIBRARIES_TTXREQS_H

/*
 * ttxreqs.library - BOOPSI/GadTools requesters + prefs file I/O
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef EXEC_LIBRARIES_H
#include <exec/libraries.h>
#endif

#ifndef INTUITION_INTUITION_H
#include <intuition/intuition.h>
#endif

#define TTXREQSNAME "ttxreqs.library"

struct TTXReqsBase {
	struct Library lib;
};

/****************************************************************************/

struct TRFindOptions {
	BOOL doPatterns;
	BOOL ignoreAccents;
	BOOL ignoreCase;
	BOOL wholeWords;
	BOOL scanBackwards;
};

struct TRPrefs {
	BOOL autoCorrectWordCase;
	BOOL autoEraseSelectedBlocks;
	BOOL autoIndentNewLines;
	BOOL selectWhenDragging;
	BOOL freeForm;
	BOOL lineWrap;
	BOOL overstrike;
	BOOL wordWrap;
	ULONG rightMargin;
	ULONG tabWidth;
	BOOL expandTabs;
};

/****************************************************************************/
/* C prototypes (clients normally use proto/ttxreqs.h + pragmas) */

#ifndef TR_SHARED_LIB

BOOL TR_RequestBool(struct Window *parent, STRPTR title, STRPTR prompt);
LONG TR_RequestChoice(struct Window *parent, STRPTR title, STRPTR prompt,
	STRPTR gadgets);
BOOL TR_RequestStr(struct Window *parent, STRPTR title, STRPTR defStr,
	STRPTR *outStr);
BOOL TR_RequestNum(struct Window *parent, STRPTR title, LONG defVal,
	BOOL positiveOnly, LONG *outNum);
STRPTR TR_RequestFile(struct Window *parent, STRPTR title, BOOL saveMode,
	STRPTR initialFile, STRPTR initialDrawer);
BOOL TR_RequestFind(struct Window *parent, struct TRFindOptions *opts,
	STRPTR findBuf, ULONG bufLen, LONG *action);
BOOL TR_RequestFindChange(struct Window *parent, struct TRFindOptions *opts,
	STRPTR findBuf, STRPTR changeBuf, ULONG bufLen, LONG *action);

VOID TR_PrefsSetDefaults(struct TRPrefs *p);
struct TRPrefs *TR_PrefsGet(VOID);
VOID TR_PrefsSet(struct TRPrefs *p);
BOOL TR_PrefsLoad(struct TRPrefs *p, STRPTR path);
BOOL TR_PrefsSave(struct TRPrefs *p, STRPTR path);
BOOL TR_PrefsRequester(struct Window *parent, struct TRPrefs *p);

#endif /* !TR_SHARED_LIB */

#endif /* LIBRARIES_TTXREQS_H */
