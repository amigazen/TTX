/*
 * TTX driver - minimal ARexx host
 *
 * Global port TURBOTEXT accepts TurboText command strings and routes them
 * through TTX_HandleCommand so external ARexx scripts can exercise the
 * editor while it runs. Document port names are TURBOTEXTn (session id).
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_driver.h"
#include "ttx_arexx.h"

#include <rexx/storage.h>
#include <rexx/rxslib.h>

#ifndef PROTO_REXXSYSLIB_H
#include <proto/rexxsyslib.h>
#endif

/****************************************************************************/

#define TTX_AREXX_GLOBAL_PORT "TURBOTEXT"
#define TTX_AREXX_MAX_ARGS    16

struct RxsLib *RexxSysBase = NULL;

/****************************************************************************/

static ULONG
TTX_ArexxStrLen(STRPTR s)
{
	ULONG n = 0;

	if (!s)
		return 0;
	while (s[n] != '\0')
		n++;
	return n;
}

static VOID
TTX_ArexxBuildSessionPortName(struct Session *session, STRPTR buf, ULONG bufLen)
{
	ULONG id = 0;
	STRPTR p = NULL;
	TEXT num[12];
	ULONG n = 0;
	ULONG digits = 0;

	if (!buf || bufLen < 12)
		return;

	buf[0] = '\0';
	if (!session)
		return;

	id = session->sessionID;
	p = buf;
	*p++ = 'T';
	*p++ = 'U';
	*p++ = 'R';
	*p++ = 'B';
	*p++ = 'O';
	*p++ = 'T';
	*p++ = 'E';
	*p++ = 'X';
	*p++ = 'T';

	if (id == 0)
	{
		*p++ = '0';
		*p = '\0';
		return;
	}

	n = id;
	digits = 0;
	while (n > 0 && digits < 11)
	{
		num[digits++] = (TEXT)('0' + (n % 10));
		n /= 10;
	}
	while (digits > 0)
		*p++ = num[--digits];
	*p = '\0';
}

/****************************************************************************/

BOOL
TTX_ParseCommandLine(
	STRPTR line,
	STRPTR *outCommand,
	STRPTR *args,
	ULONG maxArgs,
	ULONG *outArgCount)
{
	STRPTR p = NULL;
	ULONG argc = 0;
	BOOL inQuote = FALSE;
	STRPTR tokenStart = NULL;

	if (!line || !outCommand || !args || !outArgCount || maxArgs < 1)
		return FALSE;

	*outCommand = NULL;
	*outArgCount = 0;
	for (argc = 0; argc < maxArgs; argc++)
		args[argc] = NULL;
	argc = 0;

	p = line;
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p == '\0')
		return FALSE;

	tokenStart = p;
	inQuote = FALSE;

	while (*p != '\0')
	{
		if (*p == '"')
		{
			if (!inQuote)
			{
				inQuote = TRUE;
				tokenStart = p + 1;
			}
			else
			{
				*p = '\0';
				inQuote = FALSE;
				if (*outCommand == NULL)
					*outCommand = tokenStart;
				else if (argc < maxArgs)
					args[argc++] = tokenStart;
				tokenStart = NULL;
			}
		}
		else if (!inQuote && (*p == ' ' || *p == '\t'))
		{
			*p = '\0';
			if (tokenStart && *tokenStart != '\0')
			{
				if (*outCommand == NULL)
					*outCommand = tokenStart;
				else if (argc < maxArgs)
					args[argc++] = tokenStart;
			}
			tokenStart = NULL;
			while (p[1] == ' ' || p[1] == '\t')
				p++;
			tokenStart = p + 1;
		}
		p++;
	}

	if (tokenStart && *tokenStart != '\0')
	{
		if (*outCommand == NULL)
			*outCommand = tokenStart;
		else if (argc < maxArgs)
			args[argc++] = tokenStart;
	}

	*outArgCount = argc;
	if (*outCommand != NULL)
		return TRUE;
	return FALSE;
}

/****************************************************************************/

BOOL
TTX_HandleCommandLine(
	struct TTXApplication *app,
	struct Session *session,
	STRPTR line)
{
	TEXT work[512];
	STRPTR command = NULL;
	STRPTR args[TTX_AREXX_MAX_ARGS];
	ULONG argCount = 0;
	ULONG i = 0;
	BOOL ok = FALSE;

	if (!app || !line)
		return FALSE;

	if (!session)
		session = app->activeSession;
	if (!session)
		session = app->sessions;
	if (!session)
		return FALSE;

	for (i = 0; i < sizeof(work) - 1 && line[i] != '\0'; i++)
		work[i] = line[i];
	work[i] = '\0';

	if (!TTX_ParseCommandLine(work, &command, args, TTX_AREXX_MAX_ARGS, &argCount))
		return FALSE;

	ok = TTX_HandleCommand(app, session, command, args, argCount);
	return ok;
}

/****************************************************************************/

BOOL
TTX_ArexxInit(struct TTXApplication *app)
{
	struct MsgPort *port = NULL;

	if (!app)
		return FALSE;

	app->arexxPort = NULL;
	app->arexxSigBit = (UBYTE)-1;
	app->lastArexxResult[0] = '\0';

	if (!RexxSysBase)
	{
		RexxSysBase = (struct RxsLib *)OpenLibrary("rexxsyslib.library", 0);
		if (!RexxSysBase)
		{
			Printf("[AREXX] rexxsyslib.library not available - ARexx host disabled\n");
			return FALSE;
		}
	}

	port = CreateMsgPort();
	if (!port)
		return FALSE;

	port->mp_Node.ln_Name = TTX_AREXX_GLOBAL_PORT;
	port->mp_Node.ln_Pri = 0;
	AddPort(port);

	app->arexxPort = port;
	app->arexxSigBit = port->mp_SigBit;
	Printf("[AREXX] Host port %s ready\n", TTX_AREXX_GLOBAL_PORT);
	return TRUE;
}

VOID
TTX_ArexxShutdown(struct TTXApplication *app)
{
	struct Message *msg = NULL;

	if (!app)
		return;

	if (app->arexxPort)
	{
		RemPort(app->arexxPort);
		while ((msg = GetMsg(app->arexxPort)) != NULL)
			ReplyMsg(msg);
		DeleteMsgPort(app->arexxPort);
		app->arexxPort = NULL;
		app->arexxSigBit = (UBYTE)-1;
	}

	if (RexxSysBase)
	{
		CloseLibrary((struct Library *)RexxSysBase);
		RexxSysBase = NULL;
	}
}

VOID
TTX_ArexxBindSession(struct TTXApplication *app, struct Session *session)
{
	(void)app;
	if (!session)
		return;
	TTX_ArexxBuildSessionPortName(session, session->arexxPortName,
		(ULONG)sizeof(session->arexxPortName));
}

VOID
TTX_ArexxSetResult(struct TTXApplication *app, STRPTR result)
{
	ULONG i = 0;

	if (!app)
		return;
	app->lastArexxResult[0] = '\0';
	if (!result)
		return;
	for (i = 0; i < 255 && result[i] != '\0'; i++)
		app->lastArexxResult[i] = result[i];
	app->lastArexxResult[i] = '\0';
}

/****************************************************************************/

static VOID
TTX_ArexxReply(struct RexxMsg *rmsg, LONG rc, STRPTR result)
{
	ULONG len = 0;

	if (!rmsg)
		return;

	rmsg->rm_Result1 = rc;
	rmsg->rm_Result2 = 0;

	/*
 * Always attach a RESULT argstring on RC=0 (including empty).
 * Skipping empty strings left ARexx RESULT as the literal "RESULT".
 */
if (rc == 0 && result && RexxSysBase)
{
	len = TTX_ArexxStrLen(result);
	rmsg->rm_Result2 = (LONG)CreateArgstring(result, (LONG)len);
}

	ReplyMsg((struct Message *)rmsg);
}

VOID
TTX_ArexxProcess(struct TTXApplication *app)
{
	struct RexxMsg *rmsg = NULL;
	STRPTR cmd = NULL;
	BOOL ok = FALSE;
	struct Session *session = NULL;

	if (!app || !app->arexxPort || !RexxSysBase)
		return;

	while ((rmsg = (struct RexxMsg *)GetMsg(app->arexxPort)) != NULL)
	{
		if (!IsRexxMsg(rmsg))
		{
			ReplyMsg((struct Message *)rmsg);
			continue;
		}

		cmd = (STRPTR)rmsg->rm_Args[0];
		Printf("[AREXX] cmd='%s'\n", cmd ? cmd : (STRPTR)"(null)");

		app->lastArexxResult[0] = '\0';
		session = app->activeSession;
		if (!session)
			session = app->sessions;

		ok = FALSE;
		if (cmd && session)
			ok = TTX_HandleCommandLine(app, session, cmd);

		TTX_ArexxReply(rmsg, ok ? 0 : 10,
			app->lastArexxResult[0] ? app->lastArexxResult : (STRPTR)NULL);
	}
}
