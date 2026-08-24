/*
 * TTX driver - ARexx host
 *
 * Global port TURBOTEXT and per-document ports TURBOTEXTn accept TurboText
 * command strings and route them through TTX_HandleCommand. ExecARexx*
 * sends scripts to RexxMast and waits while still servicing host ports
 * (nested ADDRESS TURBOTEXT from the script).
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_driver.h"
#include "ttx_arexx.h"
#include "ttx_mem.h"

#include <dos/dosextens.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>

#ifndef PROTO_REXXSYSLIB_H
#include <proto/rexxsyslib.h>
#endif

/****************************************************************************/

#define TTX_AREXX_GLOBAL_PORT "TURBOTEXT"
#define TTX_AREXX_MAX_ARGS    16
#define TTX_AREXX_EXTENSION   "TTX"

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

static STRPTR
TTX_ArexxDupStr(STRPTR src)
{
	ULONG len = 0;
	STRPTR dst = NULL;

	if (!src)
		return NULL;
	len = TTX_ArexxStrLen(src);
	dst = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
	if (!dst)
		return NULL;
	CopyMem(src, dst, len);
	dst[len] = '\0';
	return dst;
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
	struct Session *session = NULL;

	if (!app)
		return;

	/* Drop per-document ports first. */
	session = app->sessions;
	while (session)
	{
		TTX_ArexxUnbindSession(session);
		session = session->next;
	}

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
	struct MsgPort *port = NULL;

	if (!session)
		return;

	TTX_ArexxBuildSessionPortName(session, session->arexxPortName,
		(ULONG)sizeof(session->arexxPortName));

	if (session->arexxPort || session->arexxPortName[0] == '\0')
		return;

	/* Global host can be absent (no rexxsyslib); document ports still useful. */
	(void)app;

	port = CreateMsgPort();
	if (!port)
	{
		Printf("[AREXX] FAIL CreateMsgPort for %s\n", session->arexxPortName);
		return;
	}

	port->mp_Node.ln_Name = session->arexxPortName;
	port->mp_Node.ln_Pri = 0;
	AddPort(port);
	session->arexxPort = port;
	Printf("[AREXX] Document port %s ready\n", session->arexxPortName);
}

VOID
TTX_ArexxUnbindSession(struct Session *session)
{
	struct Message *msg = NULL;

	if (!session || !session->arexxPort)
		return;

	RemPort(session->arexxPort);
	while ((msg = GetMsg(session->arexxPort)) != NULL)
		ReplyMsg(msg);
	DeleteMsgPort(session->arexxPort);
	session->arexxPort = NULL;
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
	for (i = 0; i < (TTX_AREXX_RESULT_MAX - 1) && result[i] != '\0'; i++)
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
TTX_ArexxProcessPort(
	struct TTXApplication *app,
	struct MsgPort *port,
	struct Session *session)
{
	struct RexxMsg *rmsg = NULL;
	STRPTR cmd = NULL;
	BOOL ok = FALSE;
	struct Session *ctx = NULL;

	if (!app || !port || !RexxSysBase)
		return;

	while ((rmsg = (struct RexxMsg *)GetMsg(port)) != NULL)
	{
		if (!IsRexxMsg(rmsg))
		{
			ReplyMsg((struct Message *)rmsg);
			continue;
		}

		cmd = (STRPTR)rmsg->rm_Args[0];
		Printf("[AREXX] port='%s' cmd='%s'\n",
			port->mp_Node.ln_Name ? port->mp_Node.ln_Name : (STRPTR)"?",
			cmd ? cmd : (STRPTR)"(null)");

		app->lastArexxResult[0] = '\0';
		ctx = session;
		if (!ctx)
			ctx = app->activeSession;
		if (!ctx)
			ctx = app->sessions;

		ok = FALSE;
		if (cmd && ctx)
		{
			if (ctx != app->activeSession)
				TTX_NoteSessionActivated(app, ctx);
			ok = TTX_HandleCommandLine(app, ctx, cmd);
		}

		TTX_ArexxReply(rmsg, ok ? 0 : 10, app->lastArexxResult);
	}
}

VOID
TTX_ArexxProcess(struct TTXApplication *app)
{
	struct Session *session = NULL;

	if (!app)
		return;

	if (app->arexxPort)
		TTX_ArexxProcessPort(app, app->arexxPort, app->activeSession);

	session = app->sessions;
	while (session)
	{
		if (session->arexxPort)
			TTX_ArexxProcessPort(app, session->arexxPort, session);
		session = session->next;
	}
}

/****************************************************************************/
/* Drain host ports while waiting on a RexxMast reply (nested ADDRESS). */

static VOID
TTX_ArexxServiceHosts(struct TTXApplication *app)
{
	if (!app)
		return;
	TTX_ArexxProcess(app);
}

static ULONG
TTX_ArexxHostSigMask(struct TTXApplication *app)
{
	ULONG mask = 0;
	struct Session *session = NULL;

	if (!app)
		return 0;
	if (app->arexxPort)
		mask |= (1UL << app->arexxPort->mp_SigBit);
	session = app->sessions;
	while (session)
	{
		if (session->arexxPort)
			mask |= (1UL << session->arexxPort->mp_SigBit);
		session = session->next;
	}
	return mask;
}

BOOL
TTX_ArexxExec(
	struct TTXApplication *app,
	STRPTR command,
	BOOL isString,
	BOOL console)
{
	struct MsgPort *reply = NULL;
	struct MsgPort *rexxPort = NULL;
	struct RexxMsg *rmsg = NULL;
	struct RexxMsg *replyMsg = NULL;
	BOOL ok = FALSE;
	BOOL done = FALSE;
	BOOL sent = FALSE;
	BPTR nilFh = 0;
	BPTR conFh = 0;
	ULONG waitMask = 0;
	LONG action = 0;
	STRPTR resultStr = NULL;

	if (!app || !command || command[0] == '\0')
		return FALSE;

	if (!RexxSysBase)
	{
		RexxSysBase = (struct RxsLib *)OpenLibrary("rexxsyslib.library", 0);
		if (!RexxSysBase)
		{
			Printf("[AREXX] Exec: rexxsyslib.library missing\n");
			return FALSE;
		}
	}

	reply = CreateMsgPort();
	if (!reply)
		return FALSE;

	rmsg = CreateRexxMsg(reply, TTX_AREXX_EXTENSION, TTX_AREXX_GLOBAL_PORT);
	if (!rmsg)
	{
		DeleteMsgPort(reply);
		return FALSE;
	}

	action = RXCOMM | RXFF_RESULT;
	if (isString)
		action |= RXFF_STRING;
	rmsg->rm_Action = action;

	if (console)
	{
		conFh = Open("CON:////TTX ARexx/CLOSE", MODE_NEWFILE);
		if (conFh)
		{
			rmsg->rm_Stdin = conFh;
			rmsg->rm_Stdout = conFh;
		}
	}
	else
	{
		nilFh = Open("NIL:", MODE_NEWFILE);
		if (nilFh)
		{
			rmsg->rm_Stdin = nilFh;
			rmsg->rm_Stdout = nilFh;
		}
	}

	rmsg->rm_Args[0] = CreateArgstring(command, (LONG)TTX_ArexxStrLen(command));
	if (!rmsg->rm_Args[0])
	{
		DeleteRexxMsg(rmsg);
		DeleteMsgPort(reply);
		if (conFh)
			Close(conFh);
		if (nilFh)
			Close(nilFh);
		return FALSE;
	}

	Forbid();
	rexxPort = FindPort(RXSDIR);
	if (rexxPort)
	{
		PutMsg(rexxPort, (struct Message *)rmsg);
		sent = TRUE;
	}
	Permit();

	if (!sent)
	{
		Printf("[AREXX] Exec: RexxMast port '%s' not found\n", RXSDIR);
		DeleteArgstring((UBYTE *)rmsg->rm_Args[0]);
		DeleteRexxMsg(rmsg);
		DeleteMsgPort(reply);
		if (conFh)
			Close(conFh);
		if (nilFh)
			Close(nilFh);
		return FALSE;
	}

	waitMask = (1UL << reply->mp_SigBit) | TTX_ArexxHostSigMask(app);
	while (!done)
	{
		Wait(waitMask);
		TTX_ArexxServiceHosts(app);
		while ((replyMsg = (struct RexxMsg *)GetMsg(reply)) != NULL)
		{
			if (replyMsg != rmsg)
			{
				ReplyMsg((struct Message *)replyMsg);
				continue;
			}
			ok = (BOOL)(replyMsg->rm_Result1 == 0);
			if (replyMsg->rm_Result1 == 0 && replyMsg->rm_Result2)
			{
				resultStr = (STRPTR)replyMsg->rm_Result2;
				TTX_ArexxSetResult(app, resultStr);
				DeleteArgstring((UBYTE *)replyMsg->rm_Result2);
				replyMsg->rm_Result2 = 0;
			}
			DeleteArgstring((UBYTE *)replyMsg->rm_Args[0]);
			DeleteRexxMsg(replyMsg);
			rmsg = NULL;
			done = TRUE;
		}
	}

	DeleteMsgPort(reply);
	if (conFh)
		Close(conFh);
	if (nilFh)
		Close(nilFh);

	Printf("[AREXX] Exec %s '%s' -> %s\n",
		isString ? "string" : "macro",
		command,
		ok ? "ok" : "fail");
	return ok;
}

/****************************************************************************/

VOID
TTX_SessionInitCurrentDir(struct Session *session, STRPTR fileName)
{
	BPTR fileLock = 0;
	BPTR parentLock = 0;
	BPTR curLock = 0;
	STRPTR pathBuf = NULL;
	struct Process *proc = NULL;

	if (!session)
		return;

	if (session->currentDir)
	{
		TTX_Free(session->currentDir);
		session->currentDir = NULL;
	}

	pathBuf = TTX_AllocPathBuf();
	if (!pathBuf)
		return;

	if (fileName && fileName[0] != '\0')
	{
		fileLock = Lock(fileName, SHARED_LOCK);
		if (fileLock)
		{
			parentLock = ParentDir(fileLock);
			UnLock(fileLock);
			if (parentLock)
			{
				if (NameFromLock(parentLock, pathBuf, (LONG)TTX_PATH_BUF_LEN) > 0)
					session->currentDir = TTX_ArexxDupStr(pathBuf);
				UnLock(parentLock);
			}
		}
	}

	if (!session->currentDir)
	{
		proc = (struct Process *)FindTask(NULL);
		if (proc && proc->pr_CurrentDir)
		{
			curLock = DupLock(proc->pr_CurrentDir);
			if (curLock)
			{
				if (NameFromLock(curLock, pathBuf, (LONG)TTX_PATH_BUF_LEN) > 0)
					session->currentDir = TTX_ArexxDupStr(pathBuf);
				UnLock(curLock);
			}
		}
	}

	TTX_Free(pathBuf);
}

BOOL
TTX_SessionSetCurrentDir(struct Session *session, STRPTR path)
{
	BPTR lock = 0;
	BPTR oldDir = 0;
	STRPTR pathBuf = NULL;
	BOOL ok = FALSE;

	if (!session || !path || path[0] == '\0')
		return FALSE;

	pathBuf = TTX_AllocPathBuf();
	if (!pathBuf)
		return FALSE;

	lock = Lock(path, SHARED_LOCK);
	if (!lock)
	{
		TTX_Free(pathBuf);
		return FALSE;
	}

	if (NameFromLock(lock, pathBuf, (LONG)TTX_PATH_BUF_LEN) <= 0)
	{
		UnLock(lock);
		TTX_Free(pathBuf);
		return FALSE;
	}

	if (session->currentDir)
		TTX_Free(session->currentDir);
	session->currentDir = TTX_ArexxDupStr(pathBuf);
	ok = (BOOL)(session->currentDir != NULL);

	/* Make this the process CWD while the document is active. */
	oldDir = CurrentDir(lock);
	if (oldDir)
		UnLock(oldDir);
	/* lock is now the process current dir; do not UnLock it. */

	TTX_Free(pathBuf);
	return ok;
}
