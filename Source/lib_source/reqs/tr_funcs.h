#ifndef TTXREQS_TR_FUNCS_H
#define TTXREQS_TR_FUNCS_H

#include "compiler.h"
#include "include/libraries/ttxreqs.h"

BOOL TR_LVO TR_RequestBool(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, STRPTR title),
	TR_REG(a2, STRPTR prompt));

LONG TR_LVO TR_RequestChoice(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, STRPTR title),
	TR_REG(a2, STRPTR prompt),
	TR_REG(a3, STRPTR gadgets));

BOOL TR_LVO TR_RequestStr(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, STRPTR title),
	TR_REG(a2, STRPTR defStr),
	TR_REG(a3, STRPTR *outStr));

BOOL TR_LVO TR_RequestNum(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, STRPTR title),
	TR_REG(d0, LONG defVal),
	TR_REG(d1, BOOL positiveOnly),
	TR_REG(a2, LONG *outNum));

STRPTR TR_LVO TR_RequestFile(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, STRPTR title),
	TR_REG(d0, BOOL saveMode),
	TR_REG(a2, STRPTR initialFile),
	TR_REG(a3, STRPTR initialDrawer));

BOOL TR_LVO TR_RequestFind(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, struct TRFindOptions *opts),
	TR_REG(a2, STRPTR findBuf),
	TR_REG(d0, ULONG bufLen),
	TR_REG(a3, LONG *action));

BOOL TR_LVO TR_RequestFindChange(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, struct TRFindOptions *opts),
	TR_REG(a2, STRPTR findBuf),
	TR_REG(a3, STRPTR changeBuf),
	TR_REG(d0, ULONG bufLen),
	TR_REG(d1, LONG *action));

VOID TR_LVO TR_PrefsSetDefaults(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct TRPrefs *p));

struct TRPrefs * TR_LVO TR_PrefsGet(
	TR_REG(a6, struct Library *TTXReqsBase));

VOID TR_LVO TR_PrefsSet(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct TRPrefs *p));

BOOL TR_LVO TR_PrefsLoad(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct TRPrefs *p),
	TR_REG(a1, STRPTR path));

BOOL TR_LVO TR_PrefsSave(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct TRPrefs *p),
	TR_REG(a1, STRPTR path));

BOOL TR_LVO TR_PrefsRequester(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, struct TRPrefs *p));

struct Window * TR_LVO TR_InfoOpen(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct Window *parent),
	TR_REG(a1, struct TRInfoStats *stats));

VOID TR_LVO TR_InfoClose(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct Window *infoWin));

VOID TR_LVO TR_InfoUpdate(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct Window *infoWin),
	TR_REG(a1, struct TRInfoStats *stats));

ULONG TR_LVO TR_InfoProcessMsg(
	TR_REG(a6, struct Library *TTXReqsBase),
	TR_REG(a0, struct Window *infoWin),
	TR_REG(a1, struct IntuiMessage *imsg));

#endif /* TTXREQS_TR_FUNCS_H */
