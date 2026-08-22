/*
 * TTX driver - turbotext.library integration and UI hooks
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_driver.h"

struct Library *TurboTextBase;

static BOOL TTX_ProgDirAssigned = FALSE;

/****************************************************************************/

static BOOL
TTX_AssignProgramDir(VOID)
{
	BPTR lock = 0;

	if (TTX_ProgDirAssigned)
		return TRUE;

	lock = GetProgramDir();
	if (!lock)
		return FALSE;

	if (!AssignLock(TTX_PROGASSIGN, lock))
	{
		UnLock(lock);
		return FALSE;
	}

	TTX_ProgDirAssigned = TRUE;
	return TRUE;
}

static VOID
TTX_BuildLibPath(STRPTR buf, ULONG bufLen, STRPTR libName)
{
	STRPTR dest = NULL;
	STRPTR src = NULL;

	if (!buf || bufLen < 2 || !libName)
	{
		if (buf && bufLen > 0)
			buf[0] = '\0';
		return;
	}

	dest = buf;
	src = TTX_LIBS_PREFIX;
	while (*src && (ULONG)(dest - buf) < bufLen - 1)
		*dest++ = *src++;

	src = libName;
	while (*src && (ULONG)(dest - buf) < bufLen - 1)
		*dest++ = *src++;

	*dest = '\0';
}

static struct Library *
TTX_OpenLocalLibrary(STRPTR libName)
{
	TEXT path[128];

	TTX_BuildLibPath(path, sizeof(path), libName);
	if (path[0] == '\0')
		return NULL;

	return OpenLibrary(path, 0);
}

/****************************************************************************/

struct TTTextBuffer *
TT_SessionBuffer(struct Session *session)
{
	if (!session || !session->document)
		return NULL;
	return &session->document->buffer;
}

/****************************************************************************/

static BOOL __saveds
TTX_HookCreateSession(APTR appCtx, struct TTDocument *doc, STRPTR fileName)
{
	struct TTXApplication *app = (struct TTXApplication *)appCtx;

	if (!app || !doc)
		return FALSE;

	return TTX_CreateSessionForDocument(app, doc, fileName);
}

static VOID __saveds
TTX_HookDestroySession(APTR appCtx, struct TTDocument *doc)
{
	struct TTXApplication *app = (struct TTXApplication *)appCtx;
	struct Session *session = NULL;

	if (!app || !doc)
		return;

	session = app->sessions;
	while (session)
	{
		if (session->document == doc)
		{
			TTX_DestroySession(app, session);
			return;
		}
		session = session->next;
	}
}

static VOID __saveds
TTX_HookRefreshView(APTR appCtx, struct TTDocument *doc, struct TTView *view)
{
	struct TTXApplication *app = (struct TTXApplication *)appCtx;
	struct Session *session = NULL;

	(void)view;

	if (!app || !doc)
		return;

	session = app->sessions;
	while (session)
	{
		if (session->document == doc && session->window)
		{
			RenderText(session->window, session);
			UpdateCursor(session->window, session);
			UpdateScrollBars(session);
			return;
		}
		session = session->next;
	}
}

static BOOL __saveds
TTX_HookActivateDocument(APTR appCtx, struct TTDocument *doc)
{
	struct TTXApplication *app = (struct TTXApplication *)appCtx;
	struct Session *session = NULL;

	if (!app || !doc)
		return FALSE;

	session = app->sessions;
	while (session)
	{
		if (session->document == doc)
		{
			app->activeSession = session;
			if (session->window)
				ActivateWindow(session->window);
			return TRUE;
		}
		session = session->next;
	}
	return FALSE;
}

static struct TTUIHooks TTX_UIHooksTable = {
	TTX_HookCreateSession,
	TTX_HookDestroySession,
	TTX_HookRefreshView,
	TTX_HookActivateDocument
};

/****************************************************************************/

BOOL
TTX_OpenTurboText(struct TTXApplication *app)
{
	TEXT path[128];
	LONG err = 0;

	(void)app;

	if (!TTX_AssignProgramDir())
	{
		Printf("TTX: GetProgramDir/AssignLock(%s) failed\n", TTX_PROGASSIGN);
		SetIoErr(ERROR_OBJECT_NOT_FOUND);
		return FALSE;
	}

	TTX_BuildLibPath(path, sizeof(path), TURBOTEXTNAME);
	TurboTextBase = TTX_OpenLocalLibrary(TURBOTEXTNAME);
	if (!TurboTextBase)
	{
		err = IoErr();
		if (err == 0)
			err = ERROR_OBJECT_NOT_FOUND;

		Printf("TTX: could not open %s (IoErr=%ld)\n", path, err);
		Printf("TTX: place %s in Libs/ beside the TTX executable\n",
			TURBOTEXTNAME);
		SetIoErr(err);
		return FALSE;
	}

	return TRUE;
}

VOID
TTX_CloseTurboText(struct TTXApplication *app)
{
	(void)app;

	if (TurboTextBase)
	{
		Printf("[CLEANUP] TTX_CloseTurboText: closing turbotext.library\n");
		CloseLibrary(TurboTextBase);
		TurboTextBase = NULL;
	}

	if (TTX_ProgDirAssigned)
	{
		Printf("[CLEANUP] TTX_CloseTurboText: releasing %s assign\n",
			TTX_PROGASSIGN);
		AssignLock(TTX_PROGASSIGN, (BPTR)NULL);
		TTX_ProgDirAssigned = FALSE;
	}
}

/****************************************************************************/

static VOID
TTX_AppendCmd(STRPTR buf, ULONG bufLen, STRPTR text)
{
	ULONG len = 0;
	ULONG pos = 0;

	if (!buf || !text || bufLen < 2)
		return;

	while (buf[pos] != '\0' && pos < bufLen - 1)
		pos++;

	len = 0;
	while (text[len] != '\0' && pos + len < bufLen - 1)
	{
		buf[pos + len] = text[len];
		len++;
	}
	buf[pos + len] = '\0';
}

static VOID
TTX_BuildOpenDocCmd(
	STRPTR buf,
	ULONG bufLen,
	STRPTR fileName,
	struct TTXArgs *args)
{
	if (!buf || bufLen < 16)
		return;

	buf[0] = '\0';

	if (fileName && fileName[0])
	{
		TTX_AppendCmd(buf, bufLen, "OPENDOC FILE ");
		TTX_AppendCmd(buf, bufLen, fileName);
	}
	else
	{
		TTX_AppendCmd(buf, bufLen, "OPENDOC");
	}

	if (args)
	{
		if (args->window)
		{
			TTX_AppendCmd(buf, bufLen, " WINDOW ");
			TTX_AppendCmd(buf, bufLen, args->window);
		}
		if (args->definitions)
		{
			TTX_AppendCmd(buf, bufLen, " DEFINITIONS ");
			TTX_AppendCmd(buf, bufLen, args->definitions);
		}
		if (args->settings)
		{
			TTX_AppendCmd(buf, bufLen, " SETTINGS ");
			TTX_AppendCmd(buf, bufLen, args->settings);
		}
		if (args->pubscreen)
		{
			TTX_AppendCmd(buf, bufLen, " PUBSCREEN ");
			TTX_AppendCmd(buf, bufLen, args->pubscreen);
		}
	}
}

LONG
TTX_RunWithArgs(struct TTXApplication *app, struct TTXArgs *args)
{
	STRPTR *files = NULL;
	TEXT cmdBuf[512];
	LONG result = 0;

	if (!app || !TurboTextBase)
		return -1;

	if (args && args->files)
	{
		files = args->files;
		while (*files)
		{
			TTX_BuildOpenDocCmd(cmdBuf, sizeof(cmdBuf), *files, args);
			result = TurboTextRun(cmdBuf, &TTX_UIHooksTable, (APTR)app);
			if (result < 0)
				return result;
			files++;
		}
	}
	else if (!args || !args->noWindow)
	{
		TTX_BuildOpenDocCmd(cmdBuf, sizeof(cmdBuf), NULL, args);
		result = TurboTextRun(cmdBuf, &TTX_UIHooksTable, (APTR)app);
	}

	return result;
}
