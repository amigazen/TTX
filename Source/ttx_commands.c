/*
 * TTX - Command Handler Functions
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_driver.h"
#include "ttx_commands_prot.h"
#include "ttx_menu_builtin.h"
#include "ttx_reqsui.h"
#include "ttx_prefs.h"
#include "ttx_input.h"
#include "ttx_clipboard.h"
#include "ttx.h"

/* Last Find/Replace strings for repeat Find / FindChange. */
static STRPTR TTX_LastFind = NULL;
static STRPTR TTX_LastReplace = NULL;
/* Defaults match original Find: ignore case, forward, not whole-word. */
static struct TTXFindOptions TTX_LastFindOpts = {
	FALSE, FALSE, TRUE, FALSE, FALSE
};

/*
 * Menu handlers sometimes allocate args (bookmark numbers). Those must be
 * FreeVec'd. Constant string literals ("10000", "Toggle", …) must never be —
 * FreeVec on a non-AllocVec pointer corrupts the memory list.
 */
static STRPTR TTX_MenuHeapArgs[8];
static ULONG TTX_MenuHeapArgCount;

static VOID
TTX_MenuNoteHeapArg(STRPTR p)
{
	if (p && TTX_MenuHeapArgCount < 8) {
		TTX_MenuHeapArgs[TTX_MenuHeapArgCount] = p;
		TTX_MenuHeapArgCount++;
	}
}

static VOID
TTX_MenuFreeHeapArgs(VOID)
{
	ULONG i;

	for (i = 0; i < TTX_MenuHeapArgCount; i++)
		TTX_Free(TTX_MenuHeapArgs[i]);
	TTX_MenuHeapArgCount = 0;
}

static LONG
TTX_ParseLongArg(STRPTR s)
{
	LONG sign = 1;
	LONG val = 0;

	if (!s || *s == '\0')
		return 0;
	if (*s == '-') {
		sign = -1;
		s++;
	} else if (*s == '+') {
		s++;
	}
	while (*s >= '0' && *s <= '9') {
		val = val * 10 + (*s - '0');
		s++;
	}
	return sign * val;
}

static STRPTR
TTX_DupStr(STRPTR src)
{
	ULONG len = 0;
	STRPTR dst = NULL;
	ULONG i = 0;

	if (!src)
		return NULL;
	while (src[len] != '\0')
		len++;
	dst = (STRPTR)TTX_Alloc(len + 1, MEMF_ANY);
	if (!dst)
		return NULL;
	for (i = 0; i < len; i++)
		dst[i] = src[i];
	dst[len] = '\0';
	return dst;
}

static VOID
TTX_SyncMarkingToBuffer(struct Session *session)
{
	struct TTView *view;
	struct TTTextBuffer *buf;

	view = TTX_SessionView(session);
	buf = TT_SessionBuffer(session);
	if (view && buf)
		buf->marking = view->marking;
}


/* Flush menu trace lines before a crash so output.txt shows the last step. */
static VOID
TTX_MenuTrace(STRPTR step)
{
	BPTR out;

	Printf("[MENU] %s\n", step);
	out = Output();
	if (out)
		Flush(out);
}

/* Run an engine command and refresh the session view. */
BOOL
TTX_DoEngineCommand(
	struct TTXApplication *app,
	struct Session *session,
	STRPTR command,
	STRPTR *args,
	ULONG argCount)
{
	if (!app || !session || !session->document || !TurboTextBase || !command)
		return FALSE;

	if (!TT_DoCommand(
		session->document,
		TT_GetActiveView(session->document),
		command, args, argCount))
	{
		Printf("[CMD] TTX_DoEngineCommand: FAIL command='%s' err=%lu\n",
			command, (ULONG)TT_GetLastError());
		return FALSE;
	}

	if (TT_SessionBuffer(session) && session->window)
	{
		CalculateMaxScroll(session, session->window);
		ScrollToCursor(session, session->window);
		UpdateScrollBars(session);
		TTX_RequestRedraw(session);
	}

	if (TT_SessionBuffer(session) && session->document)
		session->document->state.modified = TT_SessionBuffer(session)->modified;
	return TRUE;
}

/* Command dispatcher - routes engine commands to turbotext.library */
BOOL TTX_HandleCommand(struct TTXApplication *app, struct Session *session, STRPTR command, STRPTR *args, ULONG argCount)
{
    BOOL engineResult = FALSE;

    if (!app || !session || !command) {
        return FALSE;
    }
    
    Printf("[CMD] TTX_HandleCommand: command='%s' (argCount=%lu)\n", command, argCount);

    /*
     * Multi-view commands must run in the driver first. A bare engine
     * SplitView success used to return here and skip TTX_Cmd_SplitView,
     * leaving splitRatio at 0 (one pane) even though a second TTView existed.
     */
    if (Stricmp(command, "CenterView") == 0) {
        return TTX_Cmd_CenterView(app, session, args, argCount);
    } else if (Stricmp(command, "GetViewInfo") == 0) {
        return TTX_Cmd_GetViewInfo(app, session, args, argCount);
    } else if (Stricmp(command, "ScrollView") == 0) {
        return TTX_Cmd_ScrollView(app, session, args, argCount);
    } else if (Stricmp(command, "SizeView") == 0) {
        return TTX_Cmd_SizeView(app, session, args, argCount);
    } else if (Stricmp(command, "SplitView") == 0) {
        return TTX_Cmd_SplitView(app, session, args, argCount);
    } else if (Stricmp(command, "SwapViews") == 0) {
        return TTX_Cmd_SwapViews(app, session, args, argCount);
    } else if (Stricmp(command, "SwitchView") == 0) {
        return TTX_Cmd_SwitchView(app, session, args, argCount);
    } else if (Stricmp(command, "UpdateView") == 0) {
        return TTX_Cmd_UpdateView(app, session, args, argCount);
    }

    /* Delegate editing/cursor/file commands to turbotext.library engine */
    if (session->document && TurboTextBase)
    {
        engineResult = TT_DoCommand(
            session->document,
            TT_GetActiveView(session->document),
            command, args, argCount);
        if (engineResult)
        {
            if (TT_SessionBuffer(session) && session->window)
            {
                CalculateMaxScroll(session, session->window);
                UpdateScrollBars(session);
                RenderText(session->window, session);
                UpdateCursor(session->window, session);
            }
            return TRUE;
        }
        /* Unknown engine command - fall through to driver handlers */
    }
    
    /* Document commands */
    if (Stricmp(command, "ActivateLastDoc") == 0) {
        return TTX_Cmd_ActivateLastDoc(app, session, args, argCount);
    } else if (Stricmp(command, "ActivateNextDoc") == 0) {
        return TTX_Cmd_ActivateNextDoc(app, session, args, argCount);
    } else if (Stricmp(command, "ActivatePrevDoc") == 0) {
        return TTX_Cmd_ActivatePrevDoc(app, session, args, argCount);
    } else if (Stricmp(command, "CloseDoc") == 0) {
        return TTX_Cmd_CloseDoc(app, session, args, argCount);
    } else if (Stricmp(command, "OpenDoc") == 0) {
        return TTX_Cmd_OpenDoc(app, session, args, argCount);
    }
    /* Display/Window commands */
    else if (Stricmp(command, "ActivateWindow") == 0) {
        return TTX_Cmd_ActivateWindow(app, session, args, argCount);
    } else if (Stricmp(command, "BeepScreen") == 0) {
        return TTX_Cmd_BeepScreen(app, session, args, argCount);
    } else if (Stricmp(command, "CloseRequester") == 0) {
        return TTX_Cmd_CloseRequester(app, session, args, argCount);
    } else if (Stricmp(command, "ControlWindow") == 0) {
        return TTX_Cmd_ControlWindow(app, session, args, argCount);
    } else if (Stricmp(command, "GetCursor") == 0) {
        return TTX_Cmd_GetCursor(app, session, args, argCount);
    } else if (Stricmp(command, "GetScreenInfo") == 0) {
        return TTX_Cmd_GetScreenInfo(app, session, args, argCount);
    } else if (Stricmp(command, "GetWindowInfo") == 0) {
        return TTX_Cmd_GetWindowInfo(app, session, args, argCount);
    } else if (Stricmp(command, "IconifyWindow") == 0) {
        return TTX_Cmd_IconifyWindow(app, session, args, argCount);
    } else if (Stricmp(command, "MoveSizeWindow") == 0) {
        return TTX_Cmd_MoveSizeWindow(app, session, args, argCount);
    } else if (Stricmp(command, "MoveWindow") == 0) {
        return TTX_Cmd_MoveWindow(app, session, args, argCount);
    } else if (Stricmp(command, "OpenRequester") == 0) {
        return TTX_Cmd_OpenRequester(app, session, args, argCount);
    } else if (Stricmp(command, "RemakeScreen") == 0) {
        return TTX_Cmd_RemakeScreen(app, session, args, argCount);
    } else if (Stricmp(command, "Screen2Back") == 0) {
        return TTX_Cmd_Screen2Back(app, session, args, argCount);
    } else if (Stricmp(command, "Screen2Front") == 0) {
        return TTX_Cmd_Screen2Front(app, session, args, argCount);
    } else if (Stricmp(command, "SetCursor") == 0) {
        return TTX_Cmd_SetCursor(app, session, args, argCount);
    } else if (Stricmp(command, "SetStatusBar") == 0) {
        return TTX_Cmd_SetStatusBar(app, session, args, argCount);
    } else if (Stricmp(command, "SizeWindow") == 0) {
        return TTX_Cmd_SizeWindow(app, session, args, argCount);
    } else if (Stricmp(command, "UsurpWindow") == 0) {
        return TTX_Cmd_UsurpWindow(app, session, args, argCount);
    } else if (Stricmp(command, "Window2Back") == 0) {
        return TTX_Cmd_Window2Back(app, session, args, argCount);
    } else if (Stricmp(command, "Window2Front") == 0) {
        return TTX_Cmd_Window2Front(app, session, args, argCount);
    }
    /* View commands are handled before engine dispatch above. */
    /* Selection block commands */
    else if (Stricmp(command, "CopyBlk") == 0) {
        return TTX_Cmd_CopyBlk(app, session, args, argCount);
    } else if (Stricmp(command, "CutBlk") == 0) {
        return TTX_Cmd_CutBlk(app, session, args, argCount);
    } else if (Stricmp(command, "DeleteBlk") == 0) {
        return TTX_Cmd_DeleteBlk(app, session, args, argCount);
    } else if (Stricmp(command, "EncryptBlk") == 0) {
        return TTX_Cmd_EncryptBlk(app, session, args, argCount);
    } else if (Stricmp(command, "GetBlk") == 0) {
        return TTX_Cmd_GetBlk(app, session, args, argCount);
    } else if (Stricmp(command, "GetBlkInfo") == 0) {
        return TTX_Cmd_GetBlkInfo(app, session, args, argCount);
    } else if (Stricmp(command, "MarkBlk") == 0) {
        return TTX_Cmd_MarkBlk(app, session, args, argCount);
    }
    /* Clipboard commands */
    else if (Stricmp(command, "OpenClip") == 0) {
        return TTX_Cmd_OpenClip(app, session, args, argCount);
    } else if (Stricmp(command, "PasteClip") == 0) {
        return TTX_Cmd_PasteClip(app, session, args, argCount);
    } else if (Stricmp(command, "PrintClip") == 0) {
        return TTX_Cmd_PrintClip(app, session, args, argCount);
    } else if (Stricmp(command, "SaveClip") == 0) {
        return TTX_Cmd_SaveClip(app, session, args, argCount);
    }
    /* File commands */
    else if (Stricmp(command, "ClearFile") == 0) {
        return TTX_Cmd_ClearFile(app, session, args, argCount);
    } else if (Stricmp(command, "GetFileInfo") == 0) {
        return TTX_Cmd_GetFileInfo(app, session, args, argCount);
    } else if (Stricmp(command, "GetFilePath") == 0) {
        return TTX_Cmd_GetFilePath(app, session, args, argCount);
    } else if (Stricmp(command, "InsertFile") == 0) {
        return TTX_Cmd_InsertFile(app, session, args, argCount);
    } else if (Stricmp(command, "OpenFile") == 0) {
        return TTX_Cmd_OpenFile(app, session, args, argCount);
    } else if (Stricmp(command, "PrintFile") == 0) {
        return TTX_Cmd_PrintFile(app, session, args, argCount);
    } else if (Stricmp(command, "SaveFile") == 0) {
        return TTX_Cmd_SaveFile(app, session, args, argCount);
    } else if (Stricmp(command, "SaveFileAs") == 0) {
        return TTX_Cmd_SaveFileAs(app, session, args, argCount);
    } else if (Stricmp(command, "SetFilePath") == 0) {
        return TTX_Cmd_SetFilePath(app, session, args, argCount);
    }
    /* Cursor position commands */
    else if (Stricmp(command, "Find") == 0) {
        return TTX_Cmd_Find(app, session, args, argCount);
    } else if (Stricmp(command, "GetCursorPos") == 0) {
        return TTX_Cmd_GetCursorPos(app, session, args, argCount);
    } else if (Stricmp(command, "Move") == 0) {
        return TTX_Cmd_Move(app, session, args, argCount);
    } else if (Stricmp(command, "MoveChar") == 0) {
        return TTX_Cmd_MoveChar(app, session, args, argCount);
    } else if (Stricmp(command, "MoveDown") == 0) {
        return TTX_Cmd_MoveDown(app, session, args, argCount);
    } else if (Stricmp(command, "MoveDownScr") == 0) {
        return TTX_Cmd_MoveDownScr(app, session, args, argCount);
    } else if (Stricmp(command, "MoveEOF") == 0) {
        return TTX_Cmd_MoveEOF(app, session, args, argCount);
    } else if (Stricmp(command, "MoveEOL") == 0) {
        return TTX_Cmd_MoveEOL(app, session, args, argCount);
    } else if (Stricmp(command, "MoveLastChange") == 0) {
        return TTX_Cmd_MoveLastChange(app, session, args, argCount);
    } else if (Stricmp(command, "MoveLeft") == 0) {
        return TTX_Cmd_MoveLeft(app, session, args, argCount);
    } else if (Stricmp(command, "MoveMatchBkt") == 0) {
        return TTX_Cmd_MoveMatchBkt(app, session, args, argCount);
    } else if (Stricmp(command, "MoveNextTabStop") == 0) {
        return TTX_Cmd_MoveNextTabStop(app, session, args, argCount);
    } else if (Stricmp(command, "MoveNextWord") == 0) {
        return TTX_Cmd_MoveNextWord(app, session, args, argCount);
    } else if (Stricmp(command, "MovePrevTabStop") == 0) {
        return TTX_Cmd_MovePrevTabStop(app, session, args, argCount);
    } else if (Stricmp(command, "MovePrevWord") == 0) {
        return TTX_Cmd_MovePrevWord(app, session, args, argCount);
    } else if (Stricmp(command, "MoveRight") == 0) {
        return TTX_Cmd_MoveRight(app, session, args, argCount);
    } else if (Stricmp(command, "MoveSOF") == 0) {
        return TTX_Cmd_MoveSOF(app, session, args, argCount);
    } else if (Stricmp(command, "MoveSOL") == 0) {
        return TTX_Cmd_MoveSOL(app, session, args, argCount);
    } else if (Stricmp(command, "MoveUp") == 0) {
        return TTX_Cmd_MoveUp(app, session, args, argCount);
    } else if (Stricmp(command, "MoveUpScr") == 0) {
        return TTX_Cmd_MoveUpScr(app, session, args, argCount);
    }
    /* Bookmark commands */
    else if (Stricmp(command, "ClearBookmark") == 0) {
        return TTX_Cmd_ClearBookmark(app, session, args, argCount);
    } else if (Stricmp(command, "MoveAutomark") == 0) {
        return TTX_Cmd_MoveAutomark(app, session, args, argCount);
    } else if (Stricmp(command, "MoveBookmark") == 0) {
        return TTX_Cmd_MoveBookmark(app, session, args, argCount);
    } else if (Stricmp(command, "SetBookmark") == 0) {
        return TTX_Cmd_SetBookmark(app, session, args, argCount);
    }
    /* Editing commands */
    else if (Stricmp(command, "Delete") == 0) {
        return TTX_Cmd_Delete(app, session, args, argCount);
    } else if (Stricmp(command, "DeleteEOL") == 0) {
        return TTX_Cmd_DeleteEOL(app, session, args, argCount);
    } else if (Stricmp(command, "DeleteEOW") == 0) {
        return TTX_Cmd_DeleteEOW(app, session, args, argCount);
    } else if (Stricmp(command, "DeleteLine") == 0) {
        return TTX_Cmd_DeleteLine(app, session, args, argCount);
    } else if (Stricmp(command, "DeleteSOL") == 0) {
        return TTX_Cmd_DeleteSOL(app, session, args, argCount);
    } else if (Stricmp(command, "DeleteSOW") == 0) {
        return TTX_Cmd_DeleteSOW(app, session, args, argCount);
    } else if (Stricmp(command, "FindChange") == 0) {
        return TTX_Cmd_FindChange(app, session, args, argCount);
    } else if (Stricmp(command, "GetChar") == 0) {
        return TTX_Cmd_GetChar(app, session, args, argCount);
    } else if (Stricmp(command, "GetLine") == 0) {
        return TTX_Cmd_GetLine(app, session, args, argCount);
    } else if (Stricmp(command, "Insert") == 0) {
        return TTX_Cmd_Insert(app, session, args, argCount);
    } else if (Stricmp(command, "InsertLine") == 0) {
        return TTX_Cmd_InsertLine(app, session, args, argCount);
    } else if (Stricmp(command, "SetChar") == 0) {
        return TTX_Cmd_SetChar(app, session, args, argCount);
    } else if (Stricmp(command, "SwapChars") == 0) {
        return TTX_Cmd_SwapChars(app, session, args, argCount);
    } else if (Stricmp(command, "Text") == 0) {
        return TTX_Cmd_Text(app, session, args, argCount);
    } else if (Stricmp(command, "ToggleCharCase") == 0) {
        return TTX_Cmd_ToggleCharCase(app, session, args, argCount);
    } else if (Stricmp(command, "UndeleteLine") == 0) {
        return TTX_Cmd_UndeleteLine(app, session, args, argCount);
    } else if (Stricmp(command, "UndoLine") == 0) {
        return TTX_Cmd_UndoLine(app, session, args, argCount);
    }
    /* Word-level editing commands */
    else if (Stricmp(command, "CompleteTemplate") == 0) {
        return TTX_Cmd_CompleteTemplate(app, session, args, argCount);
    } else if (Stricmp(command, "CorrectWord") == 0) {
        return TTX_Cmd_CorrectWord(app, session, args, argCount);
    } else if (Stricmp(command, "CorrectWordCase") == 0) {
        return TTX_Cmd_CorrectWordCase(app, session, args, argCount);
    } else if (Stricmp(command, "GetWord") == 0) {
        return TTX_Cmd_GetWord(app, session, args, argCount);
    } else if (Stricmp(command, "ReplaceWord") == 0) {
        return TTX_Cmd_ReplaceWord(app, session, args, argCount);
    }
    /* Formatting commands */
    else if (Stricmp(command, "Center") == 0) {
        return TTX_Cmd_Center(app, session, args, argCount);
    } else if (Stricmp(command, "Conv2Lower") == 0) {
        return TTX_Cmd_Conv2Lower(app, session, args, argCount);
    } else if (Stricmp(command, "Conv2Spaces") == 0) {
        return TTX_Cmd_Conv2Spaces(app, session, args, argCount);
    } else if (Stricmp(command, "Conv2Tabs") == 0) {
        return TTX_Cmd_Conv2Tabs(app, session, args, argCount);
    } else if (Stricmp(command, "Conv2Upper") == 0) {
        return TTX_Cmd_Conv2Upper(app, session, args, argCount);
    } else if (Stricmp(command, "FormatParagraph") == 0) {
        return TTX_Cmd_FormatParagraph(app, session, args, argCount);
    } else if (Stricmp(command, "Justify") == 0) {
        return TTX_Cmd_Justify(app, session, args, argCount);
    } else if (Stricmp(command, "ShiftLeft") == 0) {
        return TTX_Cmd_ShiftLeft(app, session, args, argCount);
    } else if (Stricmp(command, "ShiftRight") == 0) {
        return TTX_Cmd_ShiftRight(app, session, args, argCount);
    }
    /* Fold commands */
    else if (Stricmp(command, "HideFold") == 0) {
        return TTX_Cmd_HideFold(app, session, args, argCount);
    } else if (Stricmp(command, "MakeFold") == 0) {
        return TTX_Cmd_MakeFold(app, session, args, argCount);
    } else if (Stricmp(command, "ShowFold") == 0) {
        return TTX_Cmd_ShowFold(app, session, args, argCount);
    } else if (Stricmp(command, "ToggleFold") == 0) {
        return TTX_Cmd_ToggleFold(app, session, args, argCount);
    } else if (Stricmp(command, "UnmakeFold") == 0) {
        return TTX_Cmd_UnmakeFold(app, session, args, argCount);
    }
    /* Macro commands */
    else if (Stricmp(command, "EndMacro") == 0) {
        return TTX_Cmd_EndMacro(app, session, args, argCount);
    } else if (Stricmp(command, "ExecARexxMacro") == 0) {
        return TTX_Cmd_ExecARexxMacro(app, session, args, argCount);
    } else if (Stricmp(command, "ExecARexxString") == 0) {
        return TTX_Cmd_ExecARexxString(app, session, args, argCount);
    } else if (Stricmp(command, "FlushARexxCache") == 0) {
        return TTX_Cmd_FlushARexxCache(app, session, args, argCount);
    } else if (Stricmp(command, "GetARexxCache") == 0) {
        return TTX_Cmd_GetARexxCache(app, session, args, argCount);
    } else if (Stricmp(command, "GetMacroInfo") == 0) {
        return TTX_Cmd_GetMacroInfo(app, session, args, argCount);
    } else if (Stricmp(command, "OpenMacro") == 0) {
        return TTX_Cmd_OpenMacro(app, session, args, argCount);
    } else if (Stricmp(command, "PlayMacro") == 0) {
        return TTX_Cmd_PlayMacro(app, session, args, argCount);
    } else if (Stricmp(command, "RecordMacro") == 0) {
        return TTX_Cmd_RecordMacro(app, session, args, argCount);
    } else if (Stricmp(command, "SaveMacro") == 0) {
        return TTX_Cmd_SaveMacro(app, session, args, argCount);
    } else if (Stricmp(command, "SetARexxCache") == 0) {
        return TTX_Cmd_SetARexxCache(app, session, args, argCount);
    }
    /* External tool commands */
    else if (Stricmp(command, "ExecTool") == 0) {
        return TTX_Cmd_ExecTool(app, session, args, argCount);
    }
    /* Configuration commands */
    else if (Stricmp(command, "GetPrefs") == 0) {
        return TTX_Cmd_GetPrefs(app, session, args, argCount);
    } else if (Stricmp(command, "OpenDefinitions") == 0) {
        return TTX_Cmd_OpenDefinitions(app, session, args, argCount);
    } else if (Stricmp(command, "OpenPrefs") == 0) {
        return TTX_Cmd_OpenPrefs(app, session, args, argCount);
    } else if (Stricmp(command, "SaveDefPrefs") == 0) {
        return TTX_Cmd_SaveDefPrefs(app, session, args, argCount);
    } else if (Stricmp(command, "SavePrefs") == 0) {
        return TTX_Cmd_SavePrefs(app, session, args, argCount);
    } else if (Stricmp(command, "SetPrefs") == 0) {
        return TTX_Cmd_SetPrefs(app, session, args, argCount);
    }
    /* ARexx input commands */
    else if (Stricmp(command, "RequestBool") == 0) {
        return TTX_Cmd_RequestBool(app, session, args, argCount);
    } else if (Stricmp(command, "RequestChoice") == 0) {
        return TTX_Cmd_RequestChoice(app, session, args, argCount);
    } else if (Stricmp(command, "RequestFile") == 0) {
        return TTX_Cmd_RequestFile(app, session, args, argCount);
    } else if (Stricmp(command, "RequestNum") == 0) {
        return TTX_Cmd_RequestNum(app, session, args, argCount);
    } else if (Stricmp(command, "RequestStr") == 0) {
        return TTX_Cmd_RequestStr(app, session, args, argCount);
    }
    /* ARexx control commands */
    else if (Stricmp(command, "GetBackground") == 0) {
        return TTX_Cmd_GetBackground(app, session, args, argCount);
    } else if (Stricmp(command, "GetCurrentDir") == 0) {
        return TTX_Cmd_GetCurrentDir(app, session, args, argCount);
    } else if (Stricmp(command, "GetDocuments") == 0) {
        return TTX_Cmd_GetDocuments(app, session, args, argCount);
    } else if (Stricmp(command, "GetErrorInfo") == 0) {
        return TTX_Cmd_GetErrorInfo(app, session, args, argCount);
    } else if (Stricmp(command, "GetLockInfo") == 0) {
        return TTX_Cmd_GetLockInfo(app, session, args, argCount);
    } else if (Stricmp(command, "GetPort") == 0) {
        return TTX_Cmd_GetPort(app, session, args, argCount);
    } else if (Stricmp(command, "GetPriority") == 0) {
        return TTX_Cmd_GetPriority(app, session, args, argCount);
    } else if (Stricmp(command, "GetReadOnly") == 0) {
        return TTX_Cmd_GetReadOnly(app, session, args, argCount);
    } else if (Stricmp(command, "GetVersion") == 0) {
        return TTX_Cmd_GetVersion(app, session, args, argCount);
    } else if (Stricmp(command, "SetBackground") == 0) {
        return TTX_Cmd_SetBackground(app, session, args, argCount);
    } else if (Stricmp(command, "SetCurrentDir") == 0) {
        return TTX_Cmd_SetCurrentDir(app, session, args, argCount);
    } else if (Stricmp(command, "SetDisplayLock") == 0) {
        return TTX_Cmd_SetDisplayLock(app, session, args, argCount);
    } else if (Stricmp(command, "SetInputLock") == 0) {
        return TTX_Cmd_SetInputLock(app, session, args, argCount);
    } else if (Stricmp(command, "SetMeta") == 0) {
        return TTX_Cmd_SetMeta(app, session, args, argCount);
    } else if (Stricmp(command, "SetMeta2") == 0) {
        return TTX_Cmd_SetMeta2(app, session, args, argCount);
    } else if (Stricmp(command, "SetMode") == 0) {
        return TTX_Cmd_SetMode(app, session, args, argCount);
    } else if (Stricmp(command, "SetMode2") == 0) {
        return TTX_Cmd_SetMode2(app, session, args, argCount);
    } else if (Stricmp(command, "SetPriority") == 0) {
        return TTX_Cmd_SetPriority(app, session, args, argCount);
    } else if (Stricmp(command, "SetQuoteMode") == 0) {
        return TTX_Cmd_SetQuoteMode(app, session, args, argCount);
    } else if (Stricmp(command, "SetReadOnly") == 0) {
        return TTX_Cmd_SetReadOnly(app, session, args, argCount);
    }
    /* Helper commands */
    else if (Stricmp(command, "Help") == 0) {
        return TTX_Cmd_Help(app, session, args, argCount);
    } else if (Stricmp(command, "Illegal") == 0) {
        return TTX_Cmd_Illegal(app, session, args, argCount);
    } else if (Stricmp(command, "NOP") == 0) {
        return TTX_Cmd_NOP(app, session, args, argCount);
    } else if (Stricmp(command, "Iconify") == 0) {
        return TTX_Cmd_Iconify(app, session, args, argCount);
    } else if (Stricmp(command, "Quit") == 0) {
        return TTX_Cmd_Quit(app, session, args, argCount);
    } else {
        Printf("[CMD] TTX_HandleCommand: unknown command '%s'\n", command);
        return FALSE;
    }
}

/* Handle menu pick - convert menu/item numbers to command */
/* Helper function to get command from menu/item number using UserData */
/* UserData format: (menuNumber << 8) | itemNumber */
static BOOL GetCommandFromMenuPick(ULONG menuNumber, ULONG itemNumber, STRPTR *outCommand, STRPTR *outArgs, ULONG *outArgCount)
{
    ULONG userData = (menuNumber << 8) | itemNumber;
    ULONG extractedMenu = (userData >> 8) & 0xFF;
    ULONG extractedItem = userData & 0xFF;
    
    if (!outCommand) {
        return FALSE;
    }
    
    *outCommand = NULL;
    if (outArgs) {
        outArgs[0] = NULL;
    }
    if (outArgCount) {
        *outArgCount = 0;
    }
    
    /* Map menu/item numbers to commands based on hardcoded menu structure */
    /* Menu 0: Project — matches TTX_BuiltIn.dfn */
    if (extractedMenu == 0) {
        switch (extractedItem) {
            case 0: *outCommand = "OpenFile"; break; /* Open... into current */
            case 1: *outCommand = "OpenDoc"; if (outArgs && outArgCount) { outArgs[0] = "FileReq"; *outArgCount = 1; } break; /* Open New... */
            case 2: *outCommand = "InsertFile"; break;
            case 4: *outCommand = "SaveFile"; break;
            case 5: *outCommand = "SaveFileAs"; break;
            case 7: *outCommand = "ClearFile"; break;
            case 8: *outCommand = "PrintFile"; break;
            case 9: *outCommand = "OpenRequester"; if (outArgs && outArgCount) { outArgs[0] = "Info"; *outArgCount = 1; } break;
            case 11: *outCommand = "SetReadOnly"; if (outArgs && outArgCount) { outArgs[0] = "Toggle"; *outArgCount = 1; } break;
            case 12: *outCommand = "CloseDoc"; break;
            default: return FALSE; /* Bar or unknown item */
        }
        return TRUE;
    }
    /* Menu 1: Windows — "New" is OpenDoc with no args (blank document) */
    else if (extractedMenu == 1) {
        switch (extractedItem) {
            case 0: *outCommand = "OpenDoc"; break;
            case 2: *outCommand = "ActivateNextDoc"; break;
            case 3: *outCommand = "ActivatePrevDoc"; break;
            case 5: *outCommand = "SizeWindow"; if (outArgs && outArgCount) { outArgs[0] = "10000"; outArgs[1] = "10000"; *outArgCount = 2; } break;
            case 6: *outCommand = "SizeWindow"; if (outArgs && outArgCount) { outArgs[0] = "-10000"; outArgs[1] = "-10000"; *outArgCount = 2; } break;
            case 8: *outCommand = "IconifyWindow"; if (outArgs && outArgCount) { outArgs[0] = "Toggle"; *outArgCount = 1; } break;
            case 16: *outCommand = "SplitView"; if (outArgs && outArgCount) { outArgs[0] = "Toggle"; *outArgCount = 1; } break;
            case 17: *outCommand = "SwitchView"; break;
            case 18: *outCommand = "SwapViews"; break;
            case 19: *outCommand = "SizeView"; if (outArgs && outArgCount) { outArgs[0] = "1"; *outArgCount = 1; } break;
            case 20: *outCommand = "SizeView"; if (outArgs && outArgCount) { outArgs[0] = "-1"; *outArgCount = 1; } break;
            case 21: *outCommand = "CenterView"; break;
            default: return FALSE;
        }
        return TRUE;
    }
    /* Menu 2: Edit */
    else if (extractedMenu == 2) {
        switch (extractedItem) {
            case 0: *outCommand = "MarkBlk"; break;
            case 1: *outCommand = "CutBlk"; break;
            case 2: *outCommand = "CopyBlk"; break;
            case 3: *outCommand = "PasteClip"; break;
            case 4: *outCommand = "DeleteBlk"; break;
            case 6: *outCommand = "MarkBlk"; if (outArgs && outArgCount) { outArgs[0] = "Vertical"; *outArgCount = 1; } break;
            case 7: *outCommand = "PasteClip"; if (outArgs && outArgCount) { outArgs[0] = "Vertical"; *outArgCount = 1; } break;
            default: return FALSE;
        }
        return TRUE;
    }
    /* Menu 3: Search */
    else if (extractedMenu == 3) {
        switch (extractedItem) {
            case 0: *outCommand = "OpenRequester"; if (outArgs && outArgCount) { outArgs[0] = "Find"; *outArgCount = 1; } break;
            case 1: *outCommand = "Find"; break;
            case 2: *outCommand = "OpenRequester"; if (outArgs && outArgCount) { outArgs[0] = "FindChange"; *outArgCount = 1; } break;
            case 4: *outCommand = "Move"; break;
            case 5: *outCommand = "MoveChar"; break;
            case 6: *outCommand = "MoveLastChange"; break;
            case 7: *outCommand = "MoveAutomark"; break;
            case 8: *outCommand = "MoveMatchBkt"; break;
            case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20:
                *outCommand = "SetBookmark"; 
                if (outArgs && outArgCount) {
                    ULONG bookmarkNum = extractedItem - 10; /* Convert 11-20 to 1-10 */
                    STRPTR numStr = (STRPTR)TTX_Alloc(16, MEMF_CLEAR);
                    if (numStr) {
                        /* Convert number to string manually (C89 compatible) */
                        ULONG num = bookmarkNum;
                        ULONG pos = 0;
                        UBYTE digits[16];
                        ULONG i;
                        if (num == 0) {
                            digits[0] = '0';
                            pos = 1;
                        } else {
                            while (num > 0 && pos < 15) {
                                digits[pos++] = '0' + (num % 10);
                                num = num / 10;
                            }
                        }
                        digits[pos] = '\0';
                        /* Reverse the string */
                        for (i = 0; i < pos / 2; i++) {
                            UBYTE temp = digits[i];
                            digits[i] = digits[pos - 1 - i];
                            digits[pos - 1 - i] = temp;
                        }
                        CopyMem(digits, numStr, pos + 1);
                        outArgs[0] = numStr;
                        *outArgCount = 1;
                        TTX_MenuNoteHeapArg(numStr);
                    }
                }
                break;
            case 22: case 23: case 24: case 25: case 26: case 27: case 28: case 29: case 30: case 31:
                *outCommand = "MoveBookmark";
                if (outArgs && outArgCount) {
                    ULONG bookmarkNum = extractedItem - 21; /* Convert 22-31 to 1-10 */
                    STRPTR numStr = (STRPTR)TTX_Alloc(16, MEMF_CLEAR);
                    if (numStr) {
                        /* Convert number to string manually (C89 compatible) */
                        ULONG num = bookmarkNum;
                        ULONG pos = 0;
                        UBYTE digits[16];
                        ULONG i;
                        if (num == 0) {
                            digits[0] = '0';
                            pos = 1;
                        } else {
                            while (num > 0 && pos < 15) {
                                digits[pos++] = '0' + (num % 10);
                                num = num / 10;
                            }
                        }
                        digits[pos] = '\0';
                        /* Reverse the string */
                        for (i = 0; i < pos / 2; i++) {
                            UBYTE temp = digits[i];
                            digits[i] = digits[pos - 1 - i];
                            digits[pos - 1 - i] = temp;
                        }
                        CopyMem(digits, numStr, pos + 1);
                        outArgs[0] = numStr;
                        *outArgCount = 1;
                        TTX_MenuNoteHeapArg(numStr);
                    }
                }
                break;
            default: return FALSE;
        }
        return TRUE;
    }
    /* Menu 4: Macros / Menu 5: Folds — still stubs */
    else if (extractedMenu == 4 || extractedMenu == 5) {
        return FALSE;
    }
    /* Menu 6: Extras — matches TTX_BuiltIn.dfn */
    else if (extractedMenu == 6) {
        switch (extractedItem) {
            case 0: *outCommand = "UndeleteLine"; break;
            case 1: *outCommand = "UndoLine"; break;
            case 3: *outCommand = "Center"; break;
            case 4: *outCommand = "Justify"; break;
            case 5: *outCommand = "FormatParagraph"; break;
            case 7: *outCommand = "Conv2Upper"; break;
            case 8: *outCommand = "Conv2Lower"; break;
            case 9: *outCommand = "Conv2Spaces"; break;
            default: return FALSE;
        }
        return TRUE;
    }
    /* Menu 7: Prefs — matches TTX_BuiltIn.dfn */
    else if (extractedMenu == 7) {
        switch (extractedItem) {
            case 0: /* Change... */
                *outCommand = "OpenRequester";
                if (outArgs && outArgCount) {
                    outArgs[0] = "Prefs";
                    *outArgCount = 1;
                }
                break;
            case 2: *outCommand = "OpenPrefs"; break;
            case 3: *outCommand = "SavePrefs"; break;
            case 4: *outCommand = "SaveDefPrefs"; break;
            case 6: *outCommand = "OpenDefinitions"; break;
            default: return FALSE;
        }
        return TRUE;
    }
    else {
        return FALSE;
    }
}

BOOL TTX_HandleMenuPick(struct TTXApplication *app, struct Session *session, ULONG menuNumber, ULONG itemNumber, APTR userData)
{
    STRPTR command = NULL;
    STRPTR args[10];
    STRPTR *dfnArgs = NULL;
    ULONG argCount = 0;
    ULONG i;
    BOOL result = FALSE;
    BOOL fromDFN = FALSE;
    
    if (!app || !session) {
        return FALSE;
    }

    for (i = 0; i < 10; i++)
        args[i] = NULL;
    
    Printf("[MENU] TTX_HandleMenuPick: called with menuNumber=%lu, itemNumber=%lu ud=%lx\n",
        menuNumber, itemNumber, (ULONG)userData);
    
    /* Check for MENUNULL (no selection) - menuNumber and itemNumber are both 0xFFFF */
    if (menuNumber == 0xFFFF && itemNumber == 0xFFFF) {
        return TRUE;  /* MENUNULL - no action needed */
    }

    /* Reset heap-arg tracking for this pick (bookmark numbers only). */
    TTX_MenuHeapArgCount = 0;

    /*
     * DFN menus store a DFNMenuEntry* in nm_UserData. Resolve the command
     * from that entry so item numbering / BAR gaps cannot desync the map.
     */
    if (session->menuDFNBacking && userData) {
        fromDFN = TTX_DFNCommandFromUserData(
            session->menuDFNBacking, userData, &command, &dfnArgs, &argCount);
        if (fromDFN && dfnArgs && argCount > 0) {
            for (i = 0; i < argCount && i < 10; i++)
                args[i] = dfnArgs[i];
        }
    }

    if (!fromDFN) {
        if (!GetCommandFromMenuPick(menuNumber, itemNumber, &command, args, &argCount)) {
            Printf("[MENU] TTX_HandleMenuPick: no command found for menu=%lu, item=%lu\n", menuNumber, itemNumber);
            TTX_MenuFreeHeapArgs();
            return FALSE;
        }
    }
    
    if (command) {
        Printf("[MENU] TTX_HandleMenuPick: command='%s' (dfn=%ld)\n",
            command, (LONG)fromDFN);
        result = TTX_HandleCommand(app, session, command, args, argCount);
        TTX_MenuFreeHeapArgs();
    }
    
    return result;
}

/* Active definitions path for menu/key DFN (OpenDefinitions updates this). */
static TEXT TTX_DefinitionsPath[256] = "PROGDIR:Support/TTX_BuiltIn.dfn";

VOID TTX_SetDefinitionsPath(STRPTR path)
{
	ULONG i;

	if (!path || path[0] == '\0')
		return;
	i = 0;
	while (path[i] != '\0' && i < (sizeof(TTX_DefinitionsPath) - 1UL)) {
		TTX_DefinitionsPath[i] = path[i];
		i++;
	}
	TTX_DefinitionsPath[i] = '\0';
}

STRPTR TTX_GetDefinitionsPath(VOID)
{
	return TTX_DefinitionsPath;
}

/* Create menu strip matching DFN file structure */
BOOL TTX_CreateMenuStrip(struct Session *session)
{
    struct Menu *menuStrip = NULL;
    struct VisualInfo *visInfo = NULL;
    struct DFNFile *dfn = NULL;
    struct NewMenu *dfnMenu = NULL;
    ULONG dfnMenuCount = 0;
    BOOL useDFN = FALSE;
    STRPTR dfnPath;
    BPTR dfnLock = NULL;
    
    if (!session || !session->window) {
        return FALSE;
    }

    dfnPath = TTX_DefinitionsPath;
    
    TTX_MenuTrace("TTX_CreateMenuStrip: START");
    
    if (!GadToolsBase) {
        TTX_MenuTrace("TTX_CreateMenuStrip: FAIL (GadToolsBase is NULL)");
        return FALSE;
    }
    
    /*
     * Optional DFN menu (not in the default install yet). Probe with Lock so
     * we never touch dos.library Open/parse when the file is absent.
     */
    SetIoErr(0);
    dfnLock = Lock(dfnPath, ACCESS_READ);
    if (dfnLock) {
        UnLock(dfnLock);
        dfn = ParseDFNFile(dfnPath);
        if (dfn) {
            TTX_MenuTrace("TTX_CreateMenuStrip: loaded DFN");
            useDFN = TRUE;
        }
    }
    
    if (useDFN && dfn) {
        /* Convert DFN to NewMenu array */
        dfnMenu = ConvertDFNToNewMenu(dfn, &dfnMenuCount);
        if (dfnMenu) {
            TTX_MenuTrace("TTX_CreateMenuStrip: CreateMenus(DFN)");
            SetIoErr(0);
            menuStrip = CreateMenus(dfnMenu, TAG_DONE);
            if (!menuStrip) {
                TTX_MenuTrace("TTX_CreateMenuStrip: FAIL (CreateMenus from DFN)");
                TTX_Free(dfnMenu);
                FreeDFNFile(dfn);
                useDFN = FALSE; /* Fall back to hardcoded menu */
            } else {
                session->menuNewMenuBacking = dfnMenu;
                session->menuDFNBacking = dfn;
            }
        } else {
            TTX_MenuTrace("TTX_CreateMenuStrip: WARN (DFN convert failed)");
            FreeDFNFile(dfn);
            useDFN = FALSE; /* Fall back to hardcoded menu */
        }
    }
    
    if (!useDFN) {
        TTX_MenuTrace("TTX_CreateMenuStrip: CreateMenus(builtin)");
        SetIoErr(0);
        menuStrip = CreateMenus(TTX_BuiltinMenu, TAG_DONE);
        if (!menuStrip) {
            TTX_MenuTrace("TTX_CreateMenuStrip: FAIL (CreateMenus builtin)");
            return FALSE;
        }
    }
    
    TTX_MenuTrace("TTX_CreateMenuStrip: GetVisualInfo");
    /* Get visual info for layout */
    visInfo = GetVisualInfo(session->window->WScreen, TAG_END);
    if (!visInfo) {
        TTX_MenuTrace("TTX_CreateMenuStrip: FAIL (GetVisualInfo)");
        FreeMenus(menuStrip);
        return FALSE;
    }
    
    TTX_MenuTrace("TTX_CreateMenuStrip: LayoutMenus");
    /* Layout menus */
    if (!LayoutMenus(menuStrip, visInfo, 
                     GTMN_NewLookMenus, TRUE,
                     TAG_END)) {
        TTX_MenuTrace("TTX_CreateMenuStrip: FAIL (LayoutMenus)");
        FreeVisualInfo(visInfo);
        FreeMenus(menuStrip);
        return FALSE;
    }
    
    TTX_MenuTrace("TTX_CreateMenuStrip: SetMenuStrip");
    /* Set menu strip on window */
    if (!SetMenuStrip(session->window, menuStrip)) {
        TTX_MenuTrace("TTX_CreateMenuStrip: FAIL (SetMenuStrip)");
        FreeVisualInfo(visInfo);
        FreeMenus(menuStrip);
        return FALSE;
    }
    
    /* Store menu strip in session */
    session->menuStrip = menuStrip;
    session->menuVisualInfo = visInfo;
    
    TTX_MenuTrace("TTX_CreateMenuStrip: SUCCESS");
    return TRUE;
}

/* Free menu strip manually (before window cleanup) */
VOID TTX_FreeMenuStrip(struct Session *session)
{
    if (!session) {
        return;
    }
    
    if (session->menuStrip && session->menuStrip != INVALID_RESOURCE) {
        /* Clear menu strip from window if window is still valid */
        if (session->window && session->window != INVALID_RESOURCE) {
            ClearMenuStrip(session->window);
        }

        FreeMenus(session->menuStrip);
        session->menuStrip = NULL;
    }

    if (session->menuVisualInfo) {
        FreeVisualInfo(session->menuVisualInfo);
        session->menuVisualInfo = NULL;
    }

    if (session->menuNewMenuBacking) {
        TTX_Free(session->menuNewMenuBacking);
        session->menuNewMenuBacking = NULL;
    }
    if (session->menuDFNBacking) {
        FreeDFNFile(session->menuDFNBacking);
        session->menuDFNBacking = NULL;
    }
}

/*
 * ResetMenuStrip autodoc sequence after MENUPICK: ClearMenuStrip then
 * ResetMenuStrip (never call ResetMenuStrip while strip is still attached).
 */
VOID TTX_ResetMenuStrip(struct Session *session)
{
    if (!session || !session->window || session->window == INVALID_RESOURCE)
        return;
    if (!session->menuStrip || session->menuStrip == INVALID_RESOURCE)
        return;

    ClearMenuStrip(session->window);
    ResetMenuStrip(session->window, session->menuStrip);
}

#define TTX_ASL_PATH_LEN TTX_PATH_BUF_LEN

static STRPTR
TTX_AllocPathCopy(STRPTR pathBuf)
{
	ULONG pathLen = 0;
	STRPTR fullPath = NULL;

	if (!pathBuf)
		return NULL;

	while (pathBuf[pathLen] != '\0')
		pathLen++;

	fullPath = (STRPTR)TTX_Alloc(pathLen + 1, MEMF_CLEAR);
	if (fullPath) {
		CopyMem(pathBuf, fullPath, pathLen);
		fullPath[pathLen] = '\0';
	}
	return fullPath;
}

static VOID
TTX_CopyStr(STRPTR dst, ULONG dstLen, STRPTR src)
{
	ULONG i = 0;

	if (!dst || dstLen < 1)
		return;
	dst[0] = '\0';
	if (!src)
		return;
	while (src[i] != '\0' && i < (dstLen - 1)) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

static VOID
TTX_SaveAslDrawer(struct TTXApplication *app, STRPTR drawer, STRPTR fullPath)
{
	BPTR fileLock = NULL;
	BPTR parentLock = NULL;
	STRPTR pathBuf = NULL;

	if (!app || !app->lastAslDrawer)
		return;

	if (drawer && drawer[0] != '\0') {
		TTX_CopyStr(app->lastAslDrawer, (ULONG)TTX_PATH_BUF_LEN, drawer);
		return;
	}

	if (!fullPath || fullPath[0] == '\0')
		return;

	pathBuf = TTX_AllocPathBuf();
	if (!pathBuf)
		return;

	fileLock = Lock(fullPath, ACCESS_READ);
	if (!fileLock) {
		TTX_Free(pathBuf);
		return;
	}
	parentLock = ParentDir(fileLock);
	UnLock(fileLock);
	if (!parentLock) {
		TTX_Free(pathBuf);
		return;
	}
	if (NameFromLock(parentLock, pathBuf, (LONG)TTX_PATH_BUF_LEN) > 0)
		TTX_CopyStr(app->lastAslDrawer, (ULONG)TTX_PATH_BUF_LEN, pathBuf);
	UnLock(parentLock);
	TTX_Free(pathBuf);
}

/*
 * Build a full pathname from ASL fr_Drawer + fr_File (GadToolsBox pattern).
 * If fr_Drawer is empty, ASL leaves the process CWD at the browsed drawer.
 */
static STRPTR
TTX_BuildFullPath(STRPTR drawer, STRPTR file, BOOL mustExist)
{
	STRPTR pathBuf = NULL;
	BPTR testLock = NULL;
	BPTR curLock = NULL;
	ULONG i = 0;
	BOOL hasVolume = FALSE;
	STRPTR result = NULL;

	if (!file || file[0] == '\0')
		return NULL;

	while (file[i] != '\0') {
		if (file[i] == ':')
			hasVolume = TRUE;
		i++;
	}

	if (hasVolume)
		return TTX_AllocPathCopy(file);

	pathBuf = TTX_AllocPathBuf();
	if (!pathBuf)
		return NULL;

	pathBuf[0] = '\0';
	if (drawer && drawer[0] != '\0')
		TTX_CopyStr(pathBuf, (ULONG)TTX_PATH_BUF_LEN, drawer);

	if (pathBuf[0] == '\0') {
		curLock = CurrentDir(NULL);
		if (curLock)
			(void)NameFromLock(curLock, pathBuf, (LONG)TTX_PATH_BUF_LEN);
	}

	if (pathBuf[0] == '\0') {
		TTX_Free(pathBuf);
		return NULL;
	}

	if (!AddPart(pathBuf, file, (ULONG)TTX_PATH_BUF_LEN)) {
		TTX_Free(pathBuf);
		return NULL;
	}

	if (mustExist) {
		testLock = Lock(pathBuf, ACCESS_READ);
		if (!testLock) {
			TTX_Free(pathBuf);
			return NULL;
		}
		UnLock(testLock);
	}

	result = TTX_AllocPathCopy(pathBuf);
	TTX_Free(pathBuf);
	return result;
}

static VOID
TTX_GetAslInitialDrawer(
	struct TTXApplication *app,
	struct Session *session,
	STRPTR buf,
	ULONG bufLen,
	STRPTR initialDrawer)
{
	BPTR progLock = NULL;
	ULONG i = 0;
	STRPTR fileName = NULL;
	STRPTR tempPath = NULL;

	if (!buf || bufLen < 2)
		return;

	buf[0] = '\0';

	if (initialDrawer && initialDrawer[0] != '\0') {
		TTX_CopyStr(buf, bufLen, initialDrawer);
		return;
	}

	if (app && app->lastAslDrawer && app->lastAslDrawer[0] != '\0') {
		TTX_CopyStr(buf, bufLen, app->lastAslDrawer);
		return;
	}

	if (session && session->document && session->document->state.fileName) {
		tempPath = TTX_AllocPathBuf();
		if (!tempPath)
			return;
		fileName = session->document->state.fileName;
		TTX_CopyStr(tempPath, (ULONG)TTX_PATH_BUF_LEN, fileName);
		i = 0;
		while (tempPath[i] != '\0')
			i++;
		while (i > 0) {
			i--;
			if (tempPath[i] == '/' || tempPath[i] == '\\') {
				tempPath[i] = '\0';
				break;
			}
			if (tempPath[i] == ':') {
				break;
			}
			tempPath[i] = '\0';
		}
		if (tempPath[0] != '\0')
			TTX_CopyStr(buf, bufLen, tempPath);
		TTX_Free(tempPath);
		if (buf[0] != '\0')
			return;
	}

	progLock = GetProgramDir();
	if (progLock) {
		(void)NameFromLock(progLock, buf, (LONG)bufLen);
		UnLock(progLock);
	}
}

static STRPTR TTX_ShowFileRequester(struct TTXApplication *app, struct Session *session, STRPTR initialFile, STRPTR initialDrawer)
{
    struct FileRequester *fileReq = NULL;
    STRPTR fullPath = NULL;
    struct Window *window = NULL;
    struct TagItem tags[12];
    STRPTR drawerBuf = NULL;
    STRPTR drawerSnap = NULL;
    STRPTR fileSnap = NULL;
    ULONG tagIdx = 0;
    
    if (!app || !AslBase) {
        Printf("[ASL] TTX_ShowFileRequester: FAIL (ASL library not available)\n");
        return NULL;
    }

    drawerBuf = TTX_AllocPathBuf();
    if (!drawerBuf) {
        Printf("[ASL] TTX_ShowFileRequester: FAIL (no memory)\n");
        return NULL;
    }
    
    Printf("[ASL] TTX_ShowFileRequester: START\n");
    
    if (session)
        window = session->window;

    TTX_GetAslInitialDrawer(app, session, drawerBuf, (ULONG)TTX_PATH_BUF_LEN,
        initialDrawer);
    
    tags[tagIdx].ti_Tag = ASLFR_Window;
    tags[tagIdx].ti_Data = (ULONG)window;
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_TitleText;
    tags[tagIdx].ti_Data = (ULONG)"Open File";
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_PositiveText;
    tags[tagIdx].ti_Data = (ULONG)"Open";
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_NegativeText;
    tags[tagIdx].ti_Data = (ULONG)"Cancel";
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_InitialFile;
    tags[tagIdx].ti_Data = (ULONG)initialFile;
    tagIdx++;
    if (drawerBuf[0] != '\0') {
        tags[tagIdx].ti_Tag = ASLFR_InitialDrawer;
        tags[tagIdx].ti_Data = (ULONG)drawerBuf;
        tagIdx++;
    }
    tags[tagIdx].ti_Tag = ASLFR_DoPatterns;
    tags[tagIdx].ti_Data = TRUE;
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_RejectIcons;
    tags[tagIdx].ti_Data = TRUE;
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_DoSaveMode;
    tags[tagIdx].ti_Data = FALSE;
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_DoMultiSelect;
    tags[tagIdx].ti_Data = FALSE;
    tagIdx++;
    tags[tagIdx].ti_Tag = TAG_DONE;
    tags[tagIdx].ti_Data = 0;
    
    fileReq = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, tags);
    
    if (!fileReq) {
        Printf("[ASL] TTX_ShowFileRequester: FAIL (AllocAslRequest failed)\n");
        TTX_Free(drawerBuf);
        return NULL;
    }
    
    if (AslRequest(fileReq, NULL)) {
        drawerSnap = TTX_AllocPathBuf();
        fileSnap = TTX_AllocPathBuf();
        if (drawerSnap && fileSnap) {
            TTX_CopyStr(drawerSnap, (ULONG)TTX_PATH_BUF_LEN,
                fileReq->fr_Drawer ? fileReq->fr_Drawer : (STRPTR)"");
            TTX_CopyStr(fileSnap, (ULONG)TTX_PATH_BUF_LEN,
                fileReq->fr_File ? fileReq->fr_File : (STRPTR)"");
            fullPath = TTX_BuildFullPath(drawerSnap, fileSnap, TRUE);
            if (fullPath) {
                TTX_SaveAslDrawer(app, drawerSnap, fullPath);
                Printf("[ASL] TTX_ShowFileRequester: drawer='%s' file='%s' -> '%s'\n",
                    drawerSnap, fileSnap, fullPath);
            } else {
                Printf("[ASL] TTX_ShowFileRequester: WARN path build failed drawer='%s' file='%s'\n",
                    drawerSnap, fileSnap);
            }
        }
        if (fileSnap)
            TTX_Free(fileSnap);
        if (drawerSnap)
            TTX_Free(drawerSnap);
    } else {
        Printf("[ASL] TTX_ShowFileRequester: user cancelled\n");
    }
    
    if (fileReq) {
        if (AslBase)
            FreeAslRequest(fileReq);
    }

    TTX_Free(drawerBuf);
    
    return fullPath;
}

/* Helper function to show file requester for saving files */
static STRPTR TTX_ShowSaveFileRequester(struct TTXApplication *app, struct Session *session, STRPTR initialFile, STRPTR initialDrawer)
{
    struct FileRequester *fileReq = NULL;
    STRPTR fullPath = NULL;
    struct Window *window = NULL;
    struct TagItem tags[12];
    STRPTR drawerBuf = NULL;
    STRPTR drawerSnap = NULL;
    STRPTR fileSnap = NULL;
    ULONG tagIdx = 0;
    
    if (!app || !AslBase) {
        Printf("[ASL] TTX_ShowSaveFileRequester: FAIL (ASL library not available)\n");
        return NULL;
    }

    drawerBuf = TTX_AllocPathBuf();
    if (!drawerBuf) {
        Printf("[ASL] TTX_ShowSaveFileRequester: FAIL (no memory)\n");
        return NULL;
    }
    
    Printf("[ASL] TTX_ShowSaveFileRequester: START\n");
    
    if (session)
        window = session->window;

    TTX_GetAslInitialDrawer(app, session, drawerBuf, (ULONG)TTX_PATH_BUF_LEN,
        initialDrawer);
    
    tags[tagIdx].ti_Tag = ASLFR_Window;
    tags[tagIdx].ti_Data = (ULONG)window;
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_TitleText;
    tags[tagIdx].ti_Data = (ULONG)"Save File As";
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_PositiveText;
    tags[tagIdx].ti_Data = (ULONG)"Save";
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_NegativeText;
    tags[tagIdx].ti_Data = (ULONG)"Cancel";
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_InitialFile;
    tags[tagIdx].ti_Data = (ULONG)initialFile;
    tagIdx++;
    if (drawerBuf[0] != '\0') {
        tags[tagIdx].ti_Tag = ASLFR_InitialDrawer;
        tags[tagIdx].ti_Data = (ULONG)drawerBuf;
        tagIdx++;
    }
    tags[tagIdx].ti_Tag = ASLFR_DoPatterns;
    tags[tagIdx].ti_Data = FALSE;
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_RejectIcons;
    tags[tagIdx].ti_Data = TRUE;
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_DoSaveMode;
    tags[tagIdx].ti_Data = TRUE;
    tagIdx++;
    tags[tagIdx].ti_Tag = ASLFR_DoMultiSelect;
    tags[tagIdx].ti_Data = FALSE;
    tagIdx++;
    tags[tagIdx].ti_Tag = TAG_DONE;
    tags[tagIdx].ti_Data = 0;
    
    fileReq = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, tags);
    
    if (!fileReq) {
        Printf("[ASL] TTX_ShowSaveFileRequester: FAIL (AllocAslRequest failed)\n");
        TTX_Free(drawerBuf);
        return NULL;
    }
    
    if (AslRequest(fileReq, NULL)) {
        drawerSnap = TTX_AllocPathBuf();
        fileSnap = TTX_AllocPathBuf();
        if (drawerSnap && fileSnap) {
            TTX_CopyStr(drawerSnap, (ULONG)TTX_PATH_BUF_LEN,
                fileReq->fr_Drawer ? fileReq->fr_Drawer : (STRPTR)"");
            TTX_CopyStr(fileSnap, (ULONG)TTX_PATH_BUF_LEN,
                fileReq->fr_File ? fileReq->fr_File : (STRPTR)"");
            fullPath = TTX_BuildFullPath(drawerSnap, fileSnap, FALSE);
            if (fullPath) {
                TTX_SaveAslDrawer(app, drawerSnap, fullPath);
                Printf("[ASL] TTX_ShowSaveFileRequester: drawer='%s' file='%s' -> '%s'\n",
                    drawerSnap, fileSnap, fullPath);
            }
        }
        if (fileSnap)
            TTX_Free(fileSnap);
        if (drawerSnap)
            TTX_Free(drawerSnap);
    } else {
        Printf("[ASL] TTX_ShowSaveFileRequester: user cancelled\n");
    }
    
    if (fileReq) {
        if (AslBase)
            FreeAslRequest(fileReq);
    }

    TTX_Free(drawerBuf);
    
    return fullPath;
}

/* Project menu command handlers */

BOOL TTX_Cmd_OpenFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    STRPTR fileName = NULL;
    STRPTR selectedFile = NULL;
    STRPTR openArgs[1];
    
    Printf("[CMD] TTX_Cmd_OpenFile: START\n");
    
    if (!app || !session || !TT_SessionBuffer(session)) {
        Printf("[CMD] TTX_Cmd_OpenFile: FAIL (app=%lx, session=%lx, buffer=%lx)\n", 
               (ULONG)app, (ULONG)session, (ULONG)(session ? TT_SessionBuffer(session) : NULL));
        return FALSE;
    }

    /* ASL must not run inside an IDCMP handler */
    if (app->intuiHandlerDepth > 0 &&
        !(args && argCount > 0 && args[0])) {
        Printf("[CMD] TTX_Cmd_OpenFile: deferred FileReq (inside IDCMP handler)\n");
        app->deferredAction = TTX_DEFER_OPENFILE_FILEREQ;
        app->deferredOpenSession = session;
        return TRUE;
    }
    
    /* Check if filename provided in args */
    if (args && argCount > 0 && args[0]) {
        fileName = args[0];
    }
    
    if (!fileName) {
        /* No filename provided - show file requester */
        if (!AslBase) {
            Printf("[CMD] TTX_Cmd_OpenFile: FAIL (ASL library not available)\n");
            return FALSE;
        }
        
        selectedFile = TTX_ShowFileRequester(app, session, NULL, NULL);
        if (!selectedFile) {
            /* User cancelled or error */
            Printf("[CMD] TTX_Cmd_OpenFile: cancelled or failed\n");
            return FALSE;
        }
        fileName = selectedFile;
    }
    
    /* Load file via turbotext.library engine (owns doc->state.fileName).
     * Path must reach a3 via correct TT_DoCommand pragma (0BA9805). */
    openArgs[0] = fileName;
    Printf("[CMD] TTX_Cmd_OpenFile: loading '%s'\n", fileName);
    if (!TT_DoCommand(session->document, TT_GetActiveView(session->document),
                      "OpenFile", openArgs, 1))
    {
        Printf("[CMD] TTX_Cmd_OpenFile: FAIL (engine OpenFile '%s' err=%lu)\n",
               fileName, (ULONG)TT_GetLastError());
        if (selectedFile) {
            TTX_Free(selectedFile);
        }
        return FALSE;
    }
    
    if (selectedFile) {
        TTX_Free(selectedFile);
    }
    
    /* Update window title */
    TTX_UpdateSessionWindowTitle(session);
    TTX_SessionInitCurrentDir(session, session->document->state.fileName);
    
    /* Reset cursor to top and redraw the full document */
    if (TT_SessionBuffer(session)) {
        TTX_SessionView(session)->cursorX = 0;
        TTX_SessionView(session)->cursorY = 0;
    }
    session->render.needsFullRedraw = TRUE;
    TTX_InputRefreshSession(session);
    
    if (TT_SessionBuffer(session)) {
        ULONG line0len = 0;
        if (TT_SessionBuffer(session)->lineCount > 0 &&
            TT_SessionBuffer(session)->lines[0].text) {
            while (TT_SessionBuffer(session)->lines[0].text[line0len] != '\0')
                line0len++;
        }
        Printf("[CMD] TTX_Cmd_OpenFile: SUCCESS (lines=%lu line0len=%lu)\n",
               TT_SessionBuffer(session)->lineCount, line0len);
    } else {
        Printf("[CMD] TTX_Cmd_OpenFile: SUCCESS (no buffer)\n");
    }
    return TRUE;
}

BOOL TTX_Cmd_OpenDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    BOOL useFileReq = FALSE;
    STRPTR selectedFile = NULL;
    BOOL result = FALSE;
    
    if (args && argCount > 0 && Stricmp(args[0], "FileReq") == 0) {
        useFileReq = TRUE;
    }

    /*
     * Never OpenWindow/AslRequest from inside an IDCMP handler (before ReplyMsg).
     * Defer to the main event loop after the current IntuiMessage is replied to.
     */
    if (app && app->intuiHandlerDepth > 0) {
        Printf("[CMD] TTX_Cmd_OpenDoc: deferred %s (inside IDCMP handler)\n",
               useFileReq ? "OpenDoc+FileReq" : "OpenDoc blank");
        /* FileReq → new window with file; no args → blank new window */
        app->deferredAction = useFileReq ? TTX_DEFER_OPENDOC_FILEREQ
                                         : TTX_DEFER_OPENDOC_NEW;
        app->deferredOpenSession = session;
        return TRUE;
    }
    
    if (useFileReq) {
        /* Open New...: requester then load into a NEW window */
        if (!AslBase) {
            Printf("[CMD] TTX_Cmd_OpenDoc: FAIL (ASL library not available)\n");
            return FALSE;
        }
        
        selectedFile = TTX_ShowFileRequester(app, session, NULL, NULL);
        if (!selectedFile) {
            Printf("[CMD] TTX_Cmd_OpenDoc: cancelled or failed\n");
            return FALSE;
        }
        
        result = TTX_CreateSession(app, selectedFile);
        if (result)
            TTX_RebuildSignalMask(app);
        
        if (selectedFile) {
            TTX_Free(selectedFile);
        }
        
        return result;
    } else {
        /* Windows/New: blank document in a new window (DFN: OpenDoc) */
        result = TTX_CreateSession(app, NULL);
        if (result)
            TTX_RebuildSignalMask(app);
        Printf("[CMD] TTX_Cmd_OpenDoc: blank session ok=%lu\n",
               (ULONG)(result ? 1 : 0));
        return result;
    }
}

BOOL TTX_Cmd_InsertFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    STRPTR fileName = NULL;
    STRPTR selectedFile = NULL;
    STRPTR insArgs[1];
    BOOL result = FALSE;

    if (!app || !session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (args && argCount > 0 && args[0]) {
        fileName = args[0];
    }

    if (!fileName) {
        if (!AslBase) {
            Printf("[CMD] TTX_Cmd_InsertFile: FAIL (ASL library not available)\n");
            return FALSE;
        }

        selectedFile = TTX_ShowFileRequester(app, session, NULL, NULL);
        if (!selectedFile) {
            Printf("[CMD] TTX_Cmd_InsertFile: cancelled or failed\n");
            return FALSE;
        }
        fileName = selectedFile;
    }

    insArgs[0] = fileName;
    result = TTX_DoEngineCommand(app, session, "InsertFile", insArgs, 1);

    if (selectedFile ) {
        TTX_Free(selectedFile);
    }

    if (result) {
        Printf("[CMD] TTX_Cmd_InsertFile: SUCCESS\n");
    } else {
        Printf("[CMD] TTX_Cmd_InsertFile: FAIL\n");
    }

    return result;
}

BOOL TTX_Cmd_SaveFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    if (!session->document->state.fileName) {
        /* No filename - use Save As instead */
        return TTX_Cmd_SaveFileAs(app, session, args, argCount);
    }
    
    if (TT_DoCommand(session->document, TT_GetActiveView(session->document),
                     "SaveFile", NULL, 0))
    {
        session->document->state.modified = FALSE;
        if (TT_SessionBuffer(session))
            TT_SessionBuffer(session)->modified = FALSE;
        Printf("[CMD] TTX_Cmd_SaveFile: SUCCESS\n");
        return TRUE;
    }
    else
    {
        LONG errorCode = IoErr();
        if (errorCode != 0) {
            PrintFault(errorCode, "TTX");
            SetIoErr(0);
        }
        Printf("[CMD] TTX_Cmd_SaveFile: FAIL\n");
        return FALSE;
    }
}

BOOL TTX_Cmd_SaveFileAs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    STRPTR fileName = NULL;
    STRPTR initialFile = NULL;
    STRPTR initialDrawer = NULL;
    STRPTR selectedFile = NULL;
    STRPTR oldPathCopy = NULL;
    STRPTR pathArgs[1];
    BOOL result = FALSE;
    
    Printf("[CMD] TTX_Cmd_SaveFileAs: START\n");
    
    if (!app || !session || !TT_SessionBuffer(session)) {
        Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (app=%lx, session=%lx, buffer=%lx)\n", 
               (ULONG)app, (ULONG)session, session ? (ULONG)TT_SessionBuffer(session) : 0);
        return FALSE;
    }
    
    /* Check if filename provided in args */
    if (args && argCount > 0 && args[0]) {
        fileName = args[0];
    } else if (session->document->state.fileName) {
        /* Use current filename as initial value */
        fileName = session->document->state.fileName;
    }
    
    /* If no filename provided, show file requester */
    if (!fileName) {
        if (!AslBase) {
            Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (ASL library not available)\n");
            return FALSE;
        }
        
        /* Extract initial file and drawer from current filename if available */
        /* Note: We need to copy the strings since we can't modify the original */
        if (session->document->state.fileName) {
            STRPTR currentFileName = session->document->state.fileName;
            ULONG len = 0;
            ULONG lastSlash = 0;
            BOOL foundSlash = FALSE;
            
            /* Find length and last '/' */
            len = 0;
            lastSlash = 0;
            foundSlash = FALSE;
            while (currentFileName[len] != '\0') {
                if (currentFileName[len] == '/') {
                    lastSlash = len;
                    foundSlash = TRUE;
                }
                len++;
            }
            
            if (foundSlash) {
                /* Found '/' - split into drawer and file */
                ULONG drawerLen = lastSlash;
                ULONG fileLen = len - lastSlash - 1;
                
                /* Allocate drawer string */
                if (drawerLen > 0) {
                    initialDrawer = (STRPTR)TTX_Alloc(drawerLen + 1, MEMF_CLEAR);
                    if (initialDrawer) {
                        CopyMem(currentFileName, initialDrawer, drawerLen);
                        initialDrawer[drawerLen] = '\0';
                    }
                }
                
                /* Allocate file string */
                if (fileLen > 0) {
                    initialFile = (STRPTR)TTX_Alloc(fileLen + 1, MEMF_CLEAR);
                    if (initialFile) {
                        CopyMem(&currentFileName[lastSlash + 1], initialFile, fileLen);
                        initialFile[fileLen] = '\0';
                    }
                }
            } else {
                /* No '/' - just file name */
                if (len > 0) {
                    initialFile = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
                    if (initialFile) {
                        CopyMem(currentFileName, initialFile, len);
                        initialFile[len] = '\0';
                    }
                }
                initialDrawer = NULL;
            }
        }
        
        selectedFile = TTX_ShowSaveFileRequester(app, session, initialFile, initialDrawer);
        if (!selectedFile) {
            /* User cancelled or error */
            Printf("[CMD] TTX_Cmd_SaveFileAs: cancelled or failed\n");
            /* Free initial file and drawer strings if we allocated them */
            if (initialFile ) {
                TTX_Free(initialFile);
            }
            if (initialDrawer ) {
                TTX_Free(initialDrawer);
            }
            return FALSE;
        }
        fileName = selectedFile;
    }

    /*
     * Engine owns doc->state.fileName (TT_DupStr / TT_Free). Snapshot the
     * old path so SaveFile failure can restore via SetFilePath.
     */
    if (session->document->state.fileName)
        oldPathCopy = TTX_DupStr(session->document->state.fileName);

    pathArgs[0] = fileName;
    if (!TTX_DoEngineCommand(app, session, "SetFilePath", pathArgs, 1)) {
        Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (SetFilePath)\n");
        if (oldPathCopy)
            TTX_Free(oldPathCopy);
        if (selectedFile)
            TTX_Free(selectedFile);
        if (initialFile)
            TTX_Free(initialFile);
        if (initialDrawer)
            TTX_Free(initialDrawer);
        return FALSE;
    }
    
    /* Save file via engine (uses doc->state.fileName set above) */
    if (TTX_DoEngineCommand(app, session, "SaveFile", NULL, 0)) {
        session->document->state.modified = FALSE;
        if (TT_SessionBuffer(session))
            TT_SessionBuffer(session)->modified = FALSE;
        result = TRUE;
        Printf("[CMD] TTX_Cmd_SaveFileAs: SUCCESS (saved to '%s')\n", session->document->state.fileName);
        TTX_UpdateSessionWindowTitle(session);
        TTX_SessionInitCurrentDir(session, session->document->state.fileName);
    } else {
        LONG errorCode = IoErr();
        if (errorCode != 0) {
            PrintFault(errorCode, "TTX");
            SetIoErr(0);
        }
        Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (SaveFile failed)\n");
        if (oldPathCopy) {
            pathArgs[0] = oldPathCopy;
            TTX_DoEngineCommand(app, session, "SetFilePath", pathArgs, 1);
        }
        result = FALSE;
    }

    if (oldPathCopy)
        TTX_Free(oldPathCopy);
    
    /* Free selected file path if we allocated it */
    if (selectedFile ) {
        TTX_Free(selectedFile);
    }
    
    /* Free initial file and drawer strings if we allocated them */
    if (initialFile && initialFile != fileName) {
        TTX_Free(initialFile);
    }
    if (initialDrawer ) {
        TTX_Free(initialDrawer);
    }
    
    return result;
}

BOOL TTX_Cmd_ClearFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    (void)args;
    (void)argCount;

    if (!app || !session || !TT_SessionBuffer(session)) {
        return FALSE;
    }

    if (!TTX_DoEngineCommand(app, session, "ClearFile", NULL, 0)) {
        Printf("[CMD] TTX_Cmd_ClearFile: FAIL\n");
        return FALSE;
    }

    session->render.needsFullRedraw = TRUE;
    if (session->window) {
        ScrollToCursor(session, session->window);
        RenderText(session->window, session);
        UpdateCursor(session->window, session);
    }

    Printf("[CMD] TTX_Cmd_ClearFile: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_PrintFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement printing */
    Printf("[CMD] TTX_Cmd_PrintFile: not yet implemented\n");
    return FALSE;
}

/* Prompt user before closing if document is modified.
 * Gadgets: Save | Discard | Cancel (EasyRequest return: 1, 2, 0).
 */
BOOL TTX_PromptSaveBeforeClose(struct TTXApplication *app, struct Session *session)
{
    struct EasyStruct es;
    LONG choice = 0;

    if (!session || !session->window || !session->document)
        return FALSE;

    if (!session->document->state.modified)
        return TRUE;

    es.es_StructSize = sizeof(struct EasyStruct);
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)"TTX";
    es.es_TextFormat = (UBYTE *)"Document has been modified.\nSave changes before closing?";
    es.es_GadgetFormat = (UBYTE *)"Save|Discard|Cancel";

    choice = EasyRequestArgs(session->window, &es, NULL, NULL);

    if (choice == 1) {
        /* Save */
        if (session->document->state.fileName)
            TTX_Cmd_SaveFile(app, session, NULL, 0);
        else
            TTX_Cmd_SaveFileAs(app, session, NULL, 0);
        if (session->document->state.modified)
            return FALSE;
        return TRUE;
    }
    if (choice == 2) {
        /* Discard */
        return TRUE;
    }
    /* Cancel (0) or closed requester */
    return FALSE;
}

static BOOL PromptSaveBeforeClose(struct TTXApplication *app, struct Session *session)
{
    return TTX_PromptSaveBeforeClose(app, session);
}

VOID
TTX_UpdateSessionWindowTitle(struct Session *session)
{
    STRPTR titleText = NULL;
    ULONG titleLen = 0;
    ULONG fileNameLen = 0;
    STRPTR endPtr = NULL;
    STRPTR tempPtr = NULL;
    STRPTR name;

    if (!session || !session->window || session->window == INVALID_RESOURCE)
        return;
    if (!session->document)
        return;

    name = session->document->state.fileName;
    if (!name || name[0] == '\0')
        name = (STRPTR)"Untitled";

    tempPtr = name;
    fileNameLen = 0;
    while (tempPtr && *tempPtr != '\0') {
        fileNameLen++;
        tempPtr++;
    }

    titleLen = fileNameLen + 10;
    titleText = TTX_Alloc(titleLen, MEMF_CLEAR);
    if (!titleText)
        return;

    endPtr = Strncpy(titleText, "TTX - ", titleLen);
    if (endPtr)
        Strncpy(endPtr, name, titleLen - (ULONG)(endPtr - titleText));

    if (session->windowState.title)
        TTX_Free(session->windowState.title);
    session->windowState.title = titleText;
    /* Window title + live/custom screen status bar. */
    TTX_RefreshStatusBar(session);
}

/*
 * Screen title used as the status bar: custom SetStatusBar text, or live
 * "TTX 3.0   column <n> line <n>  <filename>  <doc ARexx port>".
 */
VOID
TTX_RefreshStatusBar(struct Session *session)
{
	struct TTView *view = NULL;
	STRPTR name = NULL;
	STRPTR port = NULL;
	TEXT buf[160];
	TEXT portBuf[32];
	ULONG i = 0;
	ULONG v = 0;
	ULONG n = 0;
	ULONG digits = 0;
	TEXT tmp[16];
	STRPTR screenTitle = NULL;
	STRPTR s = NULL;

	if (!session || !session->window || session->window == INVALID_RESOURCE)
		return;

	if (session->statusBarText && session->statusBarText[0] != '\0') {
		screenTitle = session->statusBarText;
	} else {
		view = TTX_SessionView(session);
		name = (session->document && session->document->state.fileName)
			? session->document->state.fileName : (STRPTR)"(untitled)";
		if (!name || name[0] == '\0')
			name = (STRPTR)"(untitled)";

		/* Prefer bound TURBOTEXTn; else build from sessionID. */
		if (session->arexxPortName[0] != '\0') {
			port = session->arexxPortName;
		} else {
			s = "TURBOTEXT";
			i = 0;
			while (*s && i < 24) {
				portBuf[i++] = *s;
				s++;
			}
			v = session->sessionID;
			digits = 0;
			if (v == 0) {
				tmp[digits++] = '0';
			} else {
				n = v;
				while (n && digits < 15) {
					tmp[digits++] = (TEXT)('0' + (n % 10));
					n /= 10;
				}
			}
			while (digits && i < 31)
				portBuf[i++] = tmp[--digits];
			portBuf[i] = '\0';
			port = portBuf;
		}

		/* "TTX 3.0   column <n> line <n>  <filename>  <port>" */
		i = 0;
		s = "TTX 3.0   column ";
		while (*s && i < 150)
			buf[i++] = *s++;
		v = view ? (view->cursorX + 1) : 1;
		digits = 0;
		if (v == 0) tmp[digits++] = '0';
		else { n = v; while (n && digits < 15) { tmp[digits++] = (TEXT)('0'+(n%10)); n/=10; } }
		while (digits && i < 150) buf[i++] = tmp[--digits];
		s = " line ";
		while (*s && i < 150)
			buf[i++] = *s++;
		v = view ? (view->cursorY + 1) : 1;
		digits = 0;
		if (v == 0) tmp[digits++] = '0';
		else { n = v; while (n && digits < 15) { tmp[digits++] = (TEXT)('0'+(n%10)); n/=10; } }
		while (digits && i < 150) buf[i++] = tmp[--digits];
		buf[i++] = ' ';
		buf[i++] = ' ';
		n = 0;
		while (name[n] != '\0' && i < 150) {
			buf[i++] = name[n];
			n++;
		}
		buf[i++] = ' ';
		buf[i++] = ' ';
		n = 0;
		while (port[n] != '\0' && i < 158) {
			buf[i++] = port[n];
			n++;
		}
		buf[i] = '\0';
		screenTitle = buf;
	}

	if (session->windowState.screenTitle)
		TTX_Free(session->windowState.screenTitle);
	session->windowState.screenTitle = NULL;
	if (screenTitle) {
		n = 0;
		while (screenTitle[n] != '\0')
			n++;
		session->windowState.screenTitle = (STRPTR)TTX_Alloc(n + 1, MEMF_CLEAR);
		if (session->windowState.screenTitle)
			CopyMem(screenTitle, session->windowState.screenTitle, n + 1);
	}

	SetWindowTitles(session->window,
		session->windowState.title ? session->windowState.title : (STRPTR)"TTX",
		session->windowState.screenTitle
			? session->windowState.screenTitle : (STRPTR)"TTX");
}

VOID
TTX_NoteSessionActivated(struct TTXApplication *app, struct Session *session)
{
	if (!app || !session)
		return;
	if (app->activeSession && app->activeSession != session)
		app->previousSession = app->activeSession;
	app->activeSession = session;
}

BOOL TTX_Cmd_CloseDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!app || !session) {
        return FALSE;
    }
    
    /* Prompt user if document is modified */
    if (!PromptSaveBeforeClose(app, session)) {
        Printf("[CMD] TTX_Cmd_CloseDoc: user cancelled close\n");
        return FALSE;
    }
    
    TTX_RequestDestroySession(app, session);
    Printf("[CMD] TTX_Cmd_CloseDoc: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_SetReadOnly(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    BOOL toggle = FALSE;
    
    if (!session) {
        return FALSE;
    }
    if (args && argCount > 0 && Stricmp(args[0], "Toggle") == 0) {
        toggle = TRUE;
    }
    
    if (toggle) {
        session->document->state.readOnly = !session->document->state.readOnly;
    } else {
        session->document->state.readOnly = TRUE;
    }
    
    /* Update menu checkmark */
    if (session->menuStrip && session->window) {
        struct MenuItem *item = NULL;
        struct Menu *menu = session->menuStrip;
        ULONG itemNum = 0;
        
        /* Find Project menu (first menu) */
        if (menu) {
            /* Find Read-Only item (item 11 in Project menu) */
            item = menu->FirstItem;
            for (itemNum = 0; itemNum < 11 && item; itemNum++) {
                if (itemNum < 11) {
                    item = item->NextItem;
                }
            }
            
            if (item) {
                if (session->document->state.readOnly) {
                    item->Flags |= CHECKED;
                } else {
                    item->Flags &= ~CHECKED;
                }
                /* Note: Flag changes are automatically reflected when menu is next displayed */
                /* No need to call OnMenu() - that's for enabling/disabling menu items, not refreshing */
            }
        }
    }
    
    Printf("[CMD] TTX_Cmd_SetReadOnly: SUCCESS (readOnly=%s)\n", session->document->state.readOnly ? "TRUE" : "FALSE");
    return TRUE;
}

BOOL TTX_Cmd_Iconify(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!app) {
        return FALSE;
    }
    
    Printf("[CMD] TTX_Cmd_Iconify: START (iconified=%s)\n", app->iconified ? "TRUE" : "FALSE");
    
    /* Toggle iconification state */
    TTX_Iconify(app, !app->iconified);
    
    Printf("[CMD] TTX_Cmd_Iconify: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_Quit(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    struct Session *currentSession = NULL;
    struct Session *nextSession = NULL;
    BOOL hasModified = FALSE;
    struct IntuiText bodyText;
    struct IntuiText posText;
    struct IntuiText negText;
    STRPTR bodyStr = "One or more documents have been modified.\nSave before quitting?";
    STRPTR posStr = "Save All";
    STRPTR negStr = "Cancel";
    BOOL userChoice = FALSE;
    
    if (!app) {
        return FALSE;
    }
    
    Printf("[CMD] TTX_Cmd_Quit: START (sessionCount=%lu)\n", app->sessionCount);
    
    /* Check if any session has modified documents */
    currentSession = app->sessions;
    while (currentSession) {
        if (currentSession->document &&
            currentSession->document->state.modified) {
            hasModified = TRUE;
            break;
        }
        currentSession = currentSession->next;
    }
    
    /* Prompt user if any document is modified */
    if (hasModified && session && session->window) {
        /* Set up IntuiText structures */
        bodyText.FrontPen = 0;
        bodyText.BackPen = 1;
        bodyText.DrawMode = JAM2;
        bodyText.LeftEdge = 0;
        bodyText.TopEdge = 0;
        bodyText.ITextFont = NULL;
        bodyText.IText = bodyStr;
        bodyText.NextText = NULL;
        
        posText.FrontPen = 0;
        posText.BackPen = 1;
        posText.DrawMode = JAM2;
        posText.LeftEdge = 0;
        posText.TopEdge = 0;
        posText.ITextFont = NULL;
        posText.IText = posStr;
        posText.NextText = NULL;
        
        negText.FrontPen = 0;
        negText.BackPen = 1;
        negText.DrawMode = JAM2;
        negText.LeftEdge = 0;
        negText.TopEdge = 0;
        negText.ITextFont = NULL;
        negText.IText = negStr;
        negText.NextText = NULL;
        
        /* Show AutoRequest dialog */
        userChoice = AutoRequest(session->window, &bodyText, &posText, &negText, 0, 0, 320, 100);
        
        if (!userChoice) {
            /* User chose "Cancel" - don't quit */
            Printf("[CMD] TTX_Cmd_Quit: user cancelled quit\n");
            return FALSE;
        }
        
        /* User chose "Save All" - save all modified documents */
        currentSession = app->sessions;
        while (currentSession) {
            if (currentSession->document &&
                currentSession->document->state.modified) {
                if (currentSession->document->state.fileName) {
                    TTX_Cmd_SaveFile(app, currentSession, NULL, 0);
                } else {
                    TTX_Cmd_SaveFileAs(app, currentSession, NULL, 0);
                }
            }
            currentSession = currentSession->next;
        }
    }
    
    /* Close all sessions (windows) */
    /* Note: We iterate through all sessions and destroy them */
    /* The session list will be modified as we destroy sessions, so we need to be careful */
    currentSession = app->sessions;
    while (currentSession) {
        /* Save next pointer before destroying current session */
        nextSession = currentSession->next;
        
        Printf("[CMD] TTX_Cmd_Quit: closing session (sessionID=%lu)\n", currentSession->sessionID);
        TTX_DestroySession(app, currentSession);
        
        /* Move to next session */
        currentSession = nextSession;
    }
    
    /* Set running flag to FALSE to exit event loop */
    /* This will cause TTX_EventLoop to exit, which will then call TTX_Cleanup */
    app->running = FALSE;
    
    Printf("[CMD] TTX_Cmd_Quit: SUCCESS (all sessions closed, exiting)\n");
    return TRUE;
}

/* ============================================================================
 * Simple Command Implementations (fully implemented)
 * ============================================================================ */

BOOL TTX_Cmd_BeepScreen(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->window) {
        return FALSE;
    }
    
    /* Flash the screen - DisplayBeep uses system preferences (sound/flash) */
    DisplayBeep(session->window->WScreen);
    Printf("[CMD] TTX_Cmd_BeepScreen: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_NOP(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* No operation - do nothing, return success */
    Printf("[CMD] TTX_Cmd_NOP: SUCCESS (no operation)\n");
    return TRUE;
}

BOOL TTX_Cmd_Illegal(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* Illegal command - always returns FALSE to indicate error */
    Printf("[CMD] TTX_Cmd_Illegal: FAIL (illegal command)\n");
    return FALSE;
}

BOOL TTX_Cmd_GetVersion(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    (void)session;
    (void)args;
    (void)argCount;
    Printf("[CMD] TTX_Cmd_GetVersion: version='TTX 3.0'\n");
    TTX_ArexxSetResult(app, "TTX 3.0");
    return TRUE;
}

BOOL TTX_Cmd_GetReadOnly(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	(void)args;
	(void)argCount;
	if (!session || !session->document)
		return FALSE;
	TTX_ArexxSetResult(app, session->document->state.readOnly ? "ON" : "OFF");
	return TRUE;
}


/* ============================================================================
 * Document Commands
 * ============================================================================ */

BOOL TTX_Cmd_ActivateLastDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    struct Session *lastSession = NULL;

    (void)session;
    (void)args;
    (void)argCount;
    if (!app) {
        return FALSE;
    }

    lastSession = app->previousSession;
    if (!lastSession || !lastSession->window ||
        lastSession->window == INVALID_RESOURCE)
        lastSession = app->sessions;

    if (lastSession && lastSession->window &&
        lastSession->window != INVALID_RESOURCE) {
        WindowToFront(lastSession->window);
        ActivateWindow(lastSession->window);
        TTX_NoteSessionActivated(app, lastSession);
        Printf("[CMD] TTX_Cmd_ActivateLastDoc: SUCCESS\n");
        return TRUE;
    }

    Printf("[CMD] TTX_Cmd_ActivateLastDoc: FAIL (no session)\n");
    return FALSE;
}

BOOL TTX_Cmd_ActivateNextDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    struct Session *nextSession = NULL;

    (void)args;
    (void)argCount;
    if (!app || !session) {
        return FALSE;
    }

    nextSession = session->next;
    if (!nextSession) {
        nextSession = app->sessions;
    }

    if (nextSession && nextSession->window &&
        nextSession->window != INVALID_RESOURCE) {
        WindowToFront(nextSession->window);
        ActivateWindow(nextSession->window);
        TTX_NoteSessionActivated(app, nextSession);
        Printf("[CMD] TTX_Cmd_ActivateNextDoc: SUCCESS\n");
        return TRUE;
    }

    Printf("[CMD] TTX_Cmd_ActivateNextDoc: FAIL (no next session)\n");
    return FALSE;
}

BOOL TTX_Cmd_ActivatePrevDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    struct Session *prevSession = NULL;
    struct Session *currentSession = NULL;

    (void)args;
    (void)argCount;
    if (!app || !session) {
        return FALSE;
    }

    prevSession = session->prev;
    if (!prevSession) {
        currentSession = app->sessions;
        while (currentSession && currentSession->next) {
            currentSession = currentSession->next;
        }
        prevSession = currentSession;
    }

    if (prevSession && prevSession->window &&
        prevSession->window != INVALID_RESOURCE) {
        WindowToFront(prevSession->window);
        ActivateWindow(prevSession->window);
        TTX_NoteSessionActivated(app, prevSession);
        Printf("[CMD] TTX_Cmd_ActivatePrevDoc: SUCCESS\n");
        return TRUE;
    }

    Printf("[CMD] TTX_Cmd_ActivatePrevDoc: FAIL (no prev session)\n");
    return FALSE;
}

/* ============================================================================
 * Display/Window Commands (stubs for complex commands)
 * ============================================================================ */

BOOL TTX_Cmd_ActivateWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->window) {
        return FALSE;
    }
    
    ActivateWindow(session->window);
    Printf("[CMD] TTX_Cmd_ActivateWindow: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_CloseRequester(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement requester closing */
    Printf("[CMD] TTX_Cmd_CloseRequester: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_ControlWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    BOOL openWindow = FALSE;

    if (!app || !session) {
        return FALSE;
    }

    /* Parse ON/OFF/TOGGLE */
    if (args && argCount > 0) {
        if (Stricmp(args[0], "ON") == 0) {
            openWindow = TRUE;
        } else if (Stricmp(args[0], "OFF") == 0) {
            openWindow = FALSE;
        } else if (Stricmp(args[0], "TOGGLE") == 0) {
            openWindow = (session->window == NULL);
        }
    } else {
        openWindow = (session->window == NULL);
    }

    if (openWindow && (!session->window ||
        session->window == INVALID_RESOURCE)) {
        if (!TTX_RestoreWindow(app, session)) {
            Printf("[CMD] TTX_Cmd_ControlWindow: restore FAILED\n");
            return FALSE;
        }
        Printf("[CMD] TTX_Cmd_ControlWindow: window restored\n");
        return TRUE;
    } else if (!openWindow && session->window &&
        session->window != INVALID_RESOURCE) {
        TTX_SaveWindowState(session);
        TTX_CloseSessionWindow(app, session, NULL);
        session->windowState.windowOpen = FALSE;
        Printf("[CMD] TTX_Cmd_ControlWindow: window closed\n");
        return TRUE;
    }

    Printf("[CMD] TTX_Cmd_ControlWindow: SUCCESS (no change needed)\n");
    return TRUE;
}

BOOL TTX_Cmd_GetCursor(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_Cmd_GetCursorPos(app, session, args, argCount);
}


BOOL TTX_Cmd_GetScreenInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	TEXT out[64];
	ULONG i = 0;
	ULONG v;
	ULONG n;
	ULONG digits;
	TEXT tmp[16];
	struct Screen *scr;

	(void)args;
	(void)argCount;
	if (!session || !session->window || !session->window->WScreen)
		return FALSE;
	scr = session->window->WScreen;
	v = scr->Width;
	digits = 0;
	if (v == 0) tmp[digits++] = '0';
	else { n = v; while (n && digits < 15) { tmp[digits++] = (TEXT)('0'+(n%10)); n/=10; } }
	while (digits && i < 50) out[i++] = tmp[--digits];
	out[i++] = ' ';
	v = scr->Height;
	digits = 0;
	if (v == 0) tmp[digits++] = '0';
	else { n = v; while (n && digits < 15) { tmp[digits++] = (TEXT)('0'+(n%10)); n/=10; } }
	while (digits && i < 60) out[i++] = tmp[--digits];
	out[i] = '\0';
	TTX_ArexxSetResult(app, out);
	return TRUE;
}


BOOL TTX_Cmd_GetWindowInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	TEXT out[64];
	ULONG i = 0;
	ULONG v;
	ULONG n;
	ULONG digits;
	TEXT tmp[16];

	(void)args;
	(void)argCount;
	if (!session || !session->window)
		return FALSE;
	v = session->window->Width;
	digits = 0;
	if (v == 0) tmp[digits++] = '0';
	else { n = v; while (n && digits < 15) { tmp[digits++] = (TEXT)('0'+(n%10)); n/=10; } }
	while (digits && i < 50) out[i++] = tmp[--digits];
	out[i++] = ' ';
	v = session->window->Height;
	digits = 0;
	if (v == 0) tmp[digits++] = '0';
	else { n = v; while (n && digits < 15) { tmp[digits++] = (TEXT)('0'+(n%10)); n/=10; } }
	while (digits && i < 60) out[i++] = tmp[--digits];
	out[i] = '\0';
	TTX_ArexxSetResult(app, out);
	return TRUE;
}



BOOL TTX_Cmd_IconifyWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	BOOL doIconify = TRUE;

	(void)session;
	if (!app)
		return FALSE;
	if (args && argCount > 0 && args[0]) {
		if (Stricmp(args[0], "Toggle") == 0)
			doIconify = !app->iconified;
		else if (Stricmp(args[0], "Off") == 0 || Stricmp(args[0], "FALSE") == 0)
			doIconify = FALSE;
	}
	TTX_Iconify(app, doIconify);
	return TRUE;
}


BOOL TTX_Cmd_MoveSizeWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	LONG left = 0;
	LONG top = 0;
	LONG width = 0;
	LONG height = 0;

	(void)app;
	if (!session || !session->window || session->window == INVALID_RESOURCE)
		return FALSE;
	if (!args || argCount < 4) {
		Printf("[CMD] TTX_Cmd_MoveSizeWindow: need left top width height\n");
		return FALSE;
	}

	left = TTX_ParseLongArg(args[0]);
	top = TTX_ParseLongArg(args[1]);
	width = TTX_ParseLongArg(args[2]);
	height = TTX_ParseLongArg(args[3]);
	if (width < (LONG)session->windowState.minWidth)
		width = (LONG)session->windowState.minWidth;
	if (height < (LONG)session->windowState.minHeight)
		height = (LONG)session->windowState.minHeight;

	ChangeWindowBox(session->window, (WORD)left, (WORD)top,
		(WORD)width, (WORD)height);
	session->windowState.leftEdge = left;
	session->windowState.topEdge = top;
	session->windowState.innerWidth = (ULONG)width;
	session->windowState.innerHeight = (ULONG)height;
	Printf("[CMD] TTX_Cmd_MoveSizeWindow: SUCCESS\n");
	return TRUE;
}

BOOL TTX_Cmd_MoveWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	LONG left = 0;
	LONG top = 0;

	(void)app;
	if (!session || !session->window || session->window == INVALID_RESOURCE)
		return FALSE;

	if (args && argCount >= 2) {
		left = TTX_ParseLongArg(args[0]);
		top = TTX_ParseLongArg(args[1]);
	} else {
		left = session->windowState.leftEdge;
		top = session->windowState.topEdge;
	}

	ChangeWindowBox(session->window, (WORD)left, (WORD)top,
		session->window->Width, session->window->Height);
	session->windowState.leftEdge = left;
	session->windowState.topEdge = top;
	Printf("[CMD] TTX_Cmd_MoveWindow: SUCCESS\n");
	return TRUE;
}

BOOL TTX_Cmd_OpenRequester(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR which = NULL;
	struct Window *win = NULL;
	struct TTXFindOptions opts;
	TEXT findBuf[256];
	TEXT changeBuf[256];
	LONG action = 0;
	STRPTR fcArgs[2];

	if (!args || argCount < 1 || !args[0]) {
		Printf("[CMD] TTX_Cmd_OpenRequester: FAIL (need type)\n");
		return FALSE;
	}
	which = args[0];
	win = (session && session->window) ? session->window : NULL;

	opts.doPatterns = FALSE;
	opts.ignoreAccents = FALSE;
	opts.ignoreCase = TRUE;
	opts.wholeWords = FALSE;
	opts.scanBackwards = FALSE;
	opts = TTX_LastFindOpts;

	findBuf[0] = '\0';
	changeBuf[0] = '\0';
	if (TTX_LastFind) {
		ULONG i = 0;
		while (TTX_LastFind[i] && i < 255) {
			findBuf[i] = TTX_LastFind[i];
			i++;
		}
		findBuf[i] = '\0';
	}
	if (TTX_LastReplace) {
		ULONG i = 0;
		while (TTX_LastReplace[i] && i < 255) {
			changeBuf[i] = TTX_LastReplace[i];
			i++;
		}
		changeBuf[i] = '\0';
	}

	if (Stricmp(which, "Find") == 0) {
		if (!TTX_RequestFind(win, &opts, findBuf, sizeof(findBuf), &action))
			return FALSE;
		TTX_LastFindOpts = opts;
		if (TTX_LastFind)
			TTX_Free(TTX_LastFind);
		TTX_LastFind = TTX_DupStr(findBuf);
		fcArgs[0] = findBuf;
		return TTX_Cmd_Find(app, session, fcArgs, 1);
	}
	if (Stricmp(which, "FindChange") == 0) {
		if (!TTX_RequestFindChange(win, &opts, findBuf, changeBuf,
			sizeof(findBuf), &action))
			return FALSE;
		TTX_LastFindOpts = opts;
		if (TTX_LastFind)
			TTX_Free(TTX_LastFind);
		if (TTX_LastReplace)
			TTX_Free(TTX_LastReplace);
		TTX_LastFind = TTX_DupStr(findBuf);
		TTX_LastReplace = TTX_DupStr(changeBuf);
		fcArgs[0] = findBuf;
		fcArgs[1] = changeBuf;
		if (action == 0)
			return TTX_Cmd_Find(app, session, fcArgs, 1);
		/* Change / Change All — engine FindChange replaces one match. */
		return TTX_Cmd_FindChange(app, session, fcArgs, 2);
	}
	if (Stricmp(which, "Info") == 0) {
		return TTX_Cmd_GetFileInfo(app, session, NULL, 0);
	}
	if (Stricmp(which, "Prefs") == 0) {
		struct TTXPrefs edit;

		edit = *TTX_PrefsGet();
		if (!TTX_PrefsRequester(win, &edit))
			return FALSE;
		TTX_PrefsApply(app, session, &edit);
		return TRUE;
	}

	Printf("[CMD] TTX_Cmd_OpenRequester: type '%s' not implemented\n", which);
	return FALSE;
}

BOOL TTX_Cmd_RemakeScreen(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement screen remaking */
    Printf("[CMD] TTX_Cmd_RemakeScreen: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_Screen2Back(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->window) {
        return FALSE;
    }
    
    ScreenToBack(session->window->WScreen);
    Printf("[CMD] TTX_Cmd_Screen2Back: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_Screen2Front(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->window) {
        return FALSE;
    }
    
    ScreenToFront(session->window->WScreen);
    Printf("[CMD] TTX_Cmd_Screen2Front: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_SetCursor(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	struct TTView *view;
	struct TTTextBuffer *buf;
	ULONG line = 0;
	ULONG col = 0;

	(void)app;
	view = TTX_SessionView(session);
	buf = TT_SessionBuffer(session);
	if (!view || !buf)
		return FALSE;
	if (args && argCount >= 1)
		line = (ULONG)TTX_ParseLongArg(args[0]);
	if (args && argCount >= 2)
		col = (ULONG)TTX_ParseLongArg(args[1]);
	/* Accept 1-based like Move. */
	if (line > 0)
		line--;
	if (col > 0)
		col--;
	if (line >= buf->lineCount)
		line = buf->lineCount ? buf->lineCount - 1 : 0;
	view->cursorY = line;
	if (col > buf->lines[line].length)
		col = buf->lines[line].length;
	view->cursorX = col;
	if (session->window) {
		ScrollToCursor(session, session->window);
		UpdateScrollBars(session);
		TTX_RequestRedraw(session);
	}
	return TRUE;
}


BOOL TTX_Cmd_SetStatusBar(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	BOOL temporary = FALSE;
	ULONG start = 0;
	ULONG len = 0;
	ULONG a = 0;
	ULONG i = 0;
	ULONG j = 0;
	STRPTR text = NULL;
	TEXT joined[256];

	(void)app;
	if (!session)
		return FALSE;

	if (args && argCount > 0 && args[0] &&
	    (Stricmp(args[0], "TEMPORARY") == 0 ||
	     Stricmp(args[0], "Temporary") == 0 ||
	     Stricmp(args[0], "TEMP") == 0)) {
		temporary = TRUE;
		start = 1;
	}

	joined[0] = '\0';
	i = 0;
	for (a = start; a < argCount && args[a]; a++) {
		if (a > start && i < 255)
			joined[i++] = ' ';
		j = 0;
		while (args[a][j] != '\0' && i < 255) {
			joined[i++] = args[a][j];
			j++;
		}
	}
	joined[i] = '\0';
	len = i;

	if (session->statusBarText) {
		TTX_Free(session->statusBarText);
		session->statusBarText = NULL;
	}
	session->statusBarTemporary = temporary;

	if (len > 0) {
		text = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
		if (!text)
			return FALSE;
		CopyMem(joined, text, len + 1);
		session->statusBarText = text;
	}

	TTX_RefreshStatusBar(session);
	Printf("[CMD] TTX_Cmd_SetStatusBar: SUCCESS\n");
	return TRUE;
}

BOOL TTX_Cmd_SizeWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	LONG dw = 0;
	LONG dh = 0;
	LONG nw;
	LONG nh;

	(void)app;
	if (!session || !session->window)
		return FALSE;

	if (args && argCount >= 2) {
		dw = TTX_ParseLongArg(args[0]);
		dh = TTX_ParseLongArg(args[1]);
	}

	nw = (LONG)session->window->Width + dw;
	nh = (LONG)session->window->Height + dh;
	if (nw < (LONG)session->windowState.minWidth)
		nw = (LONG)session->windowState.minWidth;
	if (nh < (LONG)session->windowState.minHeight)
		nh = (LONG)session->windowState.minHeight;

	SizeWindow(session->window, (WORD)(nw - (LONG)session->window->Width),
		(WORD)(nh - (LONG)session->window->Height));
	Printf("[CMD] TTX_Cmd_SizeWindow: SUCCESS\n");
	return TRUE;
}


BOOL TTX_Cmd_UsurpWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement window usurping */
    Printf("[CMD] TTX_Cmd_UsurpWindow: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_Window2Back(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->window) {
        return FALSE;
    }
    
    WindowToBack(session->window);
    Printf("[CMD] TTX_Cmd_Window2Back: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_Window2Front(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->window) {
        return FALSE;
    }
    
    WindowToFront(session->window);
    ActivateWindow(session->window);
    Printf("[CMD] TTX_Cmd_Window2Front: SUCCESS\n");
    return TRUE;
}

/* ============================================================================
 * View Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_CenterView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	if (!TTX_DoEngineCommand(app, session, "CenterView", args, argCount))
		return FALSE;
	if (session->window) {
		UpdateScrollBars(session);
		TTX_RequestRedraw(session);
	}
	Printf("[CMD] TTX_Cmd_CenterView: SUCCESS\n");
	return TRUE;
}


BOOL TTX_Cmd_GetViewInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	struct TTView *view;
	TEXT buf[80];
	ULONG i = 0;
	ULONG v;
	ULONG n;
	ULONG digits;
	TEXT tmp[16];

	(void)args;
	(void)argCount;
	view = TTX_SessionView(session);
	if (!view)
		return FALSE;
	/* RESULT: viewID scrollX scrollY cursorX cursorY splitRatio */
	v = view->viewID;
	digits = 0;
	if (v == 0) tmp[digits++] = '0';
	else { n = v; while (n && digits < 15) { tmp[digits++] = (TEXT)('0'+(n%10)); n/=10; } }
	while (digits && i < 70) buf[i++] = tmp[--digits];
	buf[i++] = ' ';
	v = view->scrollY;
	digits = 0;
	if (v == 0) tmp[digits++] = '0';
	else { n = v; while (n && digits < 15) { tmp[digits++] = (TEXT)('0'+(n%10)); n/=10; } }
	while (digits && i < 72) buf[i++] = tmp[--digits];
	buf[i++] = ' ';
	v = session->splitRatio;
	digits = 0;
	if (v == 0) tmp[digits++] = '0';
	else { n = v; while (n && digits < 15) { tmp[digits++] = (TEXT)('0'+(n%10)); n/=10; } }
	while (digits && i < 74) buf[i++] = tmp[--digits];
	buf[i] = '\0';
	TTX_ArexxSetResult(app, buf);
	return TRUE;
}


BOOL TTX_Cmd_ScrollView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	LONG deltaX = 0;
	LONG deltaY = 0;
	struct TTView *view;
	struct TTTextBuffer *buf;

	(void)app;
	if (!session || !TT_SessionBuffer(session))
		return FALSE;
	view = TTX_SessionView(session);
	buf = TT_SessionBuffer(session);
	if (!view || !buf)
		return FALSE;

	if (args && argCount >= 2) {
		deltaX = TTX_ParseLongArg(args[0]);
		deltaY = TTX_ParseLongArg(args[1]);
	} else if (args && argCount >= 1) {
		deltaY = TTX_ParseLongArg(args[0]);
	}

	if (deltaY > 0) {
		view->scrollY += (ULONG)deltaY;
		if (view->scrollY > buf->maxScrollY)
			view->scrollY = buf->maxScrollY;
	} else if (deltaY < 0) {
		if ((ULONG)(-deltaY) > view->scrollY)
			view->scrollY = 0;
		else
			view->scrollY -= (ULONG)(-deltaY);
	}

	if (deltaX > 0) {
		view->scrollX += (ULONG)deltaX;
		if (view->scrollX > buf->maxScrollX)
			view->scrollX = buf->maxScrollX;
	} else if (deltaX < 0) {
		if ((ULONG)(-deltaX) > view->scrollX)
			view->scrollX = 0;
		else
			view->scrollX -= (ULONG)(-deltaX);
	}

	buf->scrollX = view->scrollX;
	buf->scrollY = view->scrollY;
	UpdateScrollBars(session);
	if (session->window) {
		RenderText(session->window, session);
		UpdateCursor(session->window, session);
	}
	Printf("[CMD] TTX_Cmd_ScrollView: SUCCESS dx=%ld dy=%ld\n", deltaX, deltaY);
	return TRUE;
}


BOOL TTX_Cmd_SizeView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	LONG delta = 0;

	(void)app;
	if (!session)
		return FALSE;
	if (session->splitRatio == 0) {
		Printf("[CMD] TTX_Cmd_SizeView: not split\n");
		return FALSE;
	}
	if (args && argCount > 0)
		delta = TTX_ParseLongArg(args[0]);
	/* Each step adjusts split by 5 percent. */
	if (delta > 0) {
		if (session->splitRatio + 5 <= 90)
			session->splitRatio += 5;
	} else if (delta < 0) {
		if (session->splitRatio >= 15)
			session->splitRatio -= 5;
	}
	if (session->window) {
		CalculateMaxScroll(session, session->window);
		TTX_RequestRedraw(session);
	}
	Printf("[CMD] TTX_Cmd_SizeView: ratio=%lu\n", session->splitRatio);
	return TRUE;
}


BOOL TTX_Cmd_SplitView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	BOOL toggle = TRUE;

	if (!session || !session->document)
		return FALSE;

	if (args && argCount > 0 && args[0] && Stricmp(args[0], "Toggle") == 0)
		toggle = TRUE;

	if (toggle && session->splitRatio > 0) {
		/* Unsplit: keep both views but show one pane. */
		session->splitRatio = 0;
		session->splitY = 0;
	} else {
		/*
		 * Engine owns TTView list allocation. Do not inspect doc->views from
		 * the driver (TTDocument layout must stay opaque across the LVO).
		 */
		if (!TTX_DoEngineCommand(app, session, "SplitView", args, argCount))
			return FALSE;
		session->splitRatio = 50;
	}

	if (session->window) {
		CalculateMaxScroll(session, session->window);
		UpdateScrollBars(session);
		TTX_RequestRedraw(session);
	}
	Printf("[CMD] TTX_Cmd_SplitView: ratio=%lu\n", session->splitRatio);
	return TRUE;
}


BOOL TTX_Cmd_SwapViews(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	if (!session)
		return FALSE;
	/* Need a visible split before swapping pane state. */
	if (session->splitRatio == 0) {
		if (!TTX_Cmd_SplitView(app, session, NULL, 0))
			return FALSE;
	}
	if (!TTX_DoEngineCommand(app, session, "SwapViews", args, argCount))
		return FALSE;
	if (session->window) {
		UpdateScrollBars(session);
		TTX_RequestRedraw(session);
	}
	Printf("[CMD] TTX_Cmd_SwapViews: SUCCESS\n");
	return TRUE;
}


BOOL TTX_Cmd_SwitchView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	if (!session)
		return FALSE;
	if (session->splitRatio == 0) {
		if (!TTX_Cmd_SplitView(app, session, NULL, 0))
			return FALSE;
	}
	if (!TTX_DoEngineCommand(app, session, "SwitchView", args, argCount))
		return FALSE;
	if (session->window) {
		CalculateMaxScroll(session, session->window);
		ScrollToCursor(session, session->window);
		UpdateScrollBars(session);
		TTX_RequestRedraw(session);
	}
	Printf("[CMD] TTX_Cmd_SwitchView: SUCCESS\n");
	return TRUE;
}


BOOL TTX_Cmd_UpdateView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->window || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    /* Force full redraw */
    session->render.needsFullRedraw = TRUE;
    RenderText(session->window, session);
    UpdateCursor(session->window, session);
    Printf("[CMD] TTX_Cmd_UpdateView: SUCCESS\n");
    return TRUE;
}

/* ============================================================================
 * Selection Block Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_CopyBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR blockText = NULL;
	struct TTView *view;

	(void)app;
	(void)args;
	(void)argCount;
	view = TTX_SessionView(session);
	if (!session || !TT_SessionBuffer(session) || !view || !view->marking.enabled) {
		Printf("[CMD] TTX_Cmd_CopyBlk: FAIL (no selection)\n");
		return FALSE;
	}

	TTX_SyncMarkingToBuffer(session);
	blockText = GetBlock(TT_SessionBuffer(session));
	if (!blockText) {
		Printf("[CMD] TTX_Cmd_CopyBlk: FAIL (GetBlock failed)\n");
		return FALSE;
	}

	TTX_ClipboardSetText(blockText);
	TTX_Free(blockText);
	/* Drop selection after copy so the next MarkBlk starts clean. */
	view->marking.enabled = FALSE;
	if (TT_SessionBuffer(session))
		TT_SessionBuffer(session)->marking.enabled = FALSE;
	Printf("[CMD] TTX_Cmd_CopyBlk: SUCCESS\n");
	return TRUE;
}


BOOL TTX_Cmd_CutBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR blockText = NULL;
	struct TTView *view;

	view = TTX_SessionView(session);
	if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly)
		return FALSE;
	if (!view || !view->marking.enabled) {
		Printf("[CMD] TTX_Cmd_CutBlk: FAIL (no selection)\n");
		return FALSE;
	}

	TTX_SyncMarkingToBuffer(session);
	blockText = GetBlock(TT_SessionBuffer(session));
	if (!blockText) {
		Printf("[CMD] TTX_Cmd_CutBlk: FAIL (GetBlock failed)\n");
		return FALSE;
	}

	TTX_ClipboardSetText(blockText);
	TTX_Free(blockText);

	if (!TTX_DoEngineCommand(app, session, "DeleteBlk", NULL, 0)) {
		Printf("[CMD] TTX_Cmd_CutBlk: FAIL (DeleteBlk failed)\n");
		return FALSE;
	}
	Printf("[CMD] TTX_Cmd_CutBlk: SUCCESS\n");
	return TRUE;
}


BOOL TTX_Cmd_DeleteBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (!TTX_SessionView(session)->marking.enabled) {
        Printf("[CMD] TTX_Cmd_DeleteBlk: FAIL (no selection)\n");
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "DeleteBlk", args, argCount)) {
        Printf("[CMD] TTX_Cmd_DeleteBlk: SUCCESS\n");
        return TRUE;
    }

    Printf("[CMD] TTX_Cmd_DeleteBlk: FAIL (DeleteBlk failed)\n");
    return FALSE;
}

BOOL TTX_Cmd_EncryptBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement block encryption */
    Printf("[CMD] TTX_Cmd_EncryptBlk: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR blockText = NULL;
	struct TTView *view;

	(void)args;
	(void)argCount;
	view = TTX_SessionView(session);
	if (!session || !TT_SessionBuffer(session) || !view) {
		TTX_ArexxSetResult(app, (STRPTR)"");
		return FALSE;
	}
	if (!view->marking.enabled) {
		TTX_ArexxSetResult(app, (STRPTR)"");
		Printf("[CMD] TTX_Cmd_GetBlk: no selection\n");
		return TRUE;
	}

	TTX_SyncMarkingToBuffer(session);
	blockText = GetBlock(TT_SessionBuffer(session));
	TTX_ArexxSetResult(app, blockText ? blockText : (STRPTR)"");
	if (blockText)
		TTX_Free(blockText);
	Printf("[CMD] TTX_Cmd_GetBlk: SUCCESS\n");
	return TRUE;
}

BOOL TTX_Cmd_GetBlkInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	struct TTView *view;
	TEXT buf[80];
	ULONG i = 0;
	ULONG v = 0;
	ULONG n = 0;
	ULONG digits = 0;
	TEXT tmp[16];
	ULONG startY = 0;
	ULONG startX = 0;
	ULONG stopY = 0;
	ULONG stopX = 0;

	(void)args;
	(void)argCount;
	view = TTX_SessionView(session);
	if (!view) {
		TTX_ArexxSetResult(app, (STRPTR)"OFF OFF 0 0");
		return FALSE;
	}

	/*
	 * RESULT: blockActive verticalBlock blockLine blockColumn
	 * (1-based line/col of mark start; vertical always OFF for now).
	 */
	if (!view->marking.enabled) {
		TTX_ArexxSetResult(app, (STRPTR)"OFF OFF 0 0");
		return TRUE;
	}

	startY = view->marking.startY;
	startX = view->marking.startX;
	stopY = view->marking.stopY;
	stopX = view->marking.stopX;
	if (stopY < startY || (stopY == startY && stopX < startX)) {
		startY = view->marking.stopY;
		startX = view->marking.stopX;
	}

	buf[i++] = 'O';
	buf[i++] = 'N';
	buf[i++] = ' ';
	buf[i++] = 'O';
	buf[i++] = 'F';
	buf[i++] = 'F';
	buf[i++] = ' ';
	v = startY + 1;
	digits = 0;
	if (v == 0) tmp[digits++] = '0';
	else { n = v; while (n && digits < 15) { tmp[digits++] = (TEXT)('0'+(n%10)); n/=10; } }
	while (digits && i < 60) buf[i++] = tmp[--digits];
	buf[i++] = ' ';
	v = startX + 1;
	digits = 0;
	if (v == 0) tmp[digits++] = '0';
	else { n = v; while (n && digits < 15) { tmp[digits++] = (TEXT)('0'+(n%10)); n/=10; } }
	while (digits && i < 70) buf[i++] = tmp[--digits];
	buf[i] = '\0';
	TTX_ArexxSetResult(app, buf);
	Printf("[CMD] TTX_Cmd_GetBlkInfo: %s\n", buf);
	return TRUE;
}

BOOL TTX_Cmd_MarkBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "MarkBlk", args, argCount)) {
        Printf("[CMD] TTX_Cmd_MarkBlk: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

/* ============================================================================
 * Clipboard Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_OpenClip(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	BPTR fh;
	STRPTR path;
	LONG size;
	STRPTR buf;
	LONG got;

	(void)app;
	(void)session;
	if (!args || argCount < 1 || !args[0]) {
		Printf("[CMD] TTX_Cmd_OpenClip: FAIL (need filename)\n");
		return FALSE;
	}
	path = args[0];
	fh = Open(path, MODE_OLDFILE);
	if (!fh)
		return FALSE;
	Seek(fh, 0, OFFSET_END);
	size = Seek(fh, 0, OFFSET_BEGINNING);
	if (size < 0)
		size = 0;
	buf = (STRPTR)TTX_Alloc((ULONG)size + 1, MEMF_CLEAR);
	if (!buf) {
		Close(fh);
		return FALSE;
	}
	got = Read(fh, buf, size);
	Close(fh);
	if (got < 0)
		got = 0;
	buf[got] = '\0';
	TTX_ClipboardSetText(buf);
	TTX_Free(buf);
	Printf("[CMD] TTX_Cmd_OpenClip: SUCCESS\n");
	return TRUE;
}


BOOL TTX_Cmd_PasteClip(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR p;
	STRPTR clip;
	TEXT chBuf[2];
	STRPTR insertArgs[1];

	(void)args;
	(void)argCount;
	if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly)
		return FALSE;

	clip = TTX_ClipboardGetText();
	if (!clip || clip[0] == '\0') {
		Printf("[CMD] TTX_Cmd_PasteClip: FAIL (empty clip)\n");
		return FALSE;
	}

	/* Insert clip char-by-char; newlines become InsertLine. */
	chBuf[1] = '\0';
	insertArgs[0] = chBuf;
	for (p = clip; *p != '\0'; p++) {
		if (*p == '\n' || *p == '\r') {
			if (*p == '\r' && p[1] == '\n')
				p++;
			if (!TTX_DoEngineCommand(app, session, "InsertLine", NULL, 0))
				return FALSE;
		} else {
			chBuf[0] = *p;
			if (!TTX_DoEngineCommand(app, session, "Insert", insertArgs, 1))
				return FALSE;
		}
	}
	Printf("[CMD] TTX_Cmd_PasteClip: SUCCESS\n");
	return TRUE;
}


BOOL TTX_Cmd_PrintClip(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement clipboard printing */
    Printf("[CMD] TTX_Cmd_PrintClip: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SaveClip(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	BPTR fh;
	STRPTR path;
	STRPTR clip;
	ULONG len;

	(void)app;
	(void)session;
	if (!args || argCount < 1 || !args[0])
		return FALSE;
	clip = TTX_ClipboardGetText();
	if (!clip)
		return FALSE;
	path = args[0];
	fh = Open(path, MODE_NEWFILE);
	if (!fh)
		return FALSE;
	len = 0;
	while (clip[len] != '\0')
		len++;
	Write(fh, clip, len);
	Close(fh);
	Printf("[CMD] TTX_Cmd_SaveClip: SUCCESS\n");
	return TRUE;
}


/* ============================================================================
 * File Commands (some already implemented)
 * ============================================================================ */

BOOL TTX_Cmd_GetFileInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	TEXT info[128];
	STRPTR name;
	ULONG i;

	(void)args;
	(void)argCount;
	if (!session || !session->document)
		return FALSE;
	name = session->document->state.fileName;
	if (!name)
		name = "(untitled)";
	/* Compact RESULT: name modified readOnly */
	for (i = 0; i < 120 && name[i]; i++)
		info[i] = name[i];
	info[i++] = ' ';
	info[i++] = session->document->state.modified ? '1' : '0';
	info[i++] = ' ';
	info[i++] = session->document->state.readOnly ? '1' : '0';
	info[i] = '\0';
	TTX_ArexxSetResult(app, info);
	return TRUE;
}


BOOL TTX_Cmd_GetFilePath(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	(void)args;
	(void)argCount;
	if (!session || !session->document)
		return FALSE;
	if (session->document->state.fileName)
		TTX_ArexxSetResult(app, session->document->state.fileName);
	else
		TTX_ArexxSetResult(app, (STRPTR)"(untitled)");
	return TRUE;
}


BOOL TTX_Cmd_SetFilePath(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "SetFilePath", args, argCount);
}


/* ============================================================================
 * Cursor Position Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_Find(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR useArgs[5];
	ULONG useCount = 0;

	if (!session)
		return FALSE;

	if (args && argCount > 0 && args[0] && args[0][0] != '\0') {
		if (TTX_LastFind)
			TTX_Free(TTX_LastFind);
		TTX_LastFind = TTX_DupStr(args[0]);
		useArgs[0] = args[0];
		useCount = 1;
	} else if (TTX_LastFind) {
		useArgs[0] = TTX_LastFind;
		useCount = 1;
	} else {
		Printf("[CMD] TTX_Cmd_Find: FAIL (no search string)\n");
		return FALSE;
	}

	/* Append last requester flags (and any explicit Find args after str). */
	if (!TTX_LastFindOpts.ignoreCase)
		useArgs[useCount++] = (STRPTR)"CaseSensitive";
	if (TTX_LastFindOpts.wholeWords)
		useArgs[useCount++] = (STRPTR)"WholeWord";
	if (TTX_LastFindOpts.scanBackwards)
		useArgs[useCount++] = (STRPTR)"Backward";
	{
		ULONG i;
		for (i = 1; i < argCount && useCount < 5; i++) {
			if (args[i])
				useArgs[useCount++] = args[i];
		}
	}

	if (TTX_DoEngineCommand(app, session, "Find", useArgs, useCount)) {
		Printf("[CMD] TTX_Cmd_Find: SUCCESS\n");
		return TRUE;
	}
	Printf("[CMD] TTX_Cmd_Find: FAIL\n");
	return FALSE;
}


BOOL TTX_Cmd_GetCursorPos(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	struct TTView *view;
	TEXT buf[64];
	ULONG i;
	ULONG n;
	ULONG v;
	TEXT tmp[16];
	ULONG digits;

	(void)args;
	(void)argCount;
	view = TTX_SessionView(session);
	if (!view)
		return FALSE;

	/* RESULT "line col" 1-based like TurboText. */
	i = 0;
	v = view->cursorY + 1;
	digits = 0;
	if (v == 0) { tmp[digits++] = '0'; }
	else { n = v; while (n > 0 && digits < 15) { tmp[digits++] = (TEXT)('0' + (n % 10)); n /= 10; } }
	while (digits > 0 && i < 60) buf[i++] = tmp[--digits];
	buf[i++] = ' ';
	v = view->cursorX + 1;
	digits = 0;
	if (v == 0) { tmp[digits++] = '0'; }
	else { n = v; while (n > 0 && digits < 15) { tmp[digits++] = (TEXT)('0' + (n % 10)); n /= 10; } }
	while (digits > 0 && i < 62) buf[i++] = tmp[--digits];
	buf[i] = '\0';
	TTX_ArexxSetResult(app, buf);
	Printf("[CMD] TTX_Cmd_GetCursorPos: %s\n", buf);
	return TRUE;
}


BOOL TTX_Cmd_Move(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR useArgs[2];
	TEXT numBuf[32];
	LONG lineNum;
	struct Window *win;

	useArgs[0] = NULL;
	useArgs[1] = NULL;
	numBuf[0] = '\0';
	lineNum = 1;
	win = (session && session->window) ? session->window : NULL;

	if (args && argCount > 0 && args[0])
		return TTX_DoEngineCommand(app, session, "Move", args, argCount);

	/* Go To Line... — prompt when no args (menu / bare Move). */
	if (TT_SessionBuffer(session) && TTX_SessionView(session))
		lineNum = (LONG)TTX_SessionView(session)->cursorY + 1;
	if (!TTX_RequestNum(win, (STRPTR)"Go To Line", lineNum, TRUE, &lineNum))
		return FALSE;
	sprintf(numBuf, "%ld", (long)lineNum);
	useArgs[0] = numBuf;
	return TTX_DoEngineCommand(app, session, "Move", useArgs, 1);
}


BOOL TTX_Cmd_MoveChar(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	LONG count;
	struct Window *win;
	TEXT numBuf[32];
	STRPTR useArgs[1];
	ULONG i;

	count = 1;
	win = (session && session->window) ? session->window : NULL;

	if (!session || !TT_SessionBuffer(session))
		return FALSE;

	if (args && argCount > 0 && args[0]) {
		count = 0;
		{
			STRPTR s = args[0];
			LONG sign = 1;
			if (*s == '-') {
				sign = -1;
				s++;
			}
			while (*s >= '0' && *s <= '9') {
				count = count * 10 + (*s - '0');
				s++;
			}
			count *= sign;
		}
	} else {
		/* Go To Char... */
		if (TTX_SessionView(session))
			count = (LONG)TTX_SessionView(session)->cursorX + 1;
		if (!TTX_RequestNum(win, (STRPTR)"Go To Char", count, TRUE, &count))
			return FALSE;
		sprintf(numBuf, "%ld", (long)count);
		useArgs[0] = numBuf;
		/* Absolute column on current line via Move line,col */
		{
			STRPTR mvArgs[2];
			TEXT lineBuf[32];
			LONG lineNum;

			lineNum = 1;
			if (TTX_SessionView(session))
				lineNum = (LONG)TTX_SessionView(session)->cursorY + 1;
			sprintf(lineBuf, "%ld", (long)lineNum);
			mvArgs[0] = lineBuf;
			mvArgs[1] = numBuf;
			return TTX_DoEngineCommand(app, session, "Move", mvArgs, 2);
		}
	}

	if (count > 0) {
		for (i = 0; i < (ULONG)count; i++) {
			if (!TTX_Cmd_MoveRight(app, session, NULL, 0))
				break;
		}
		return TRUE;
	}
	if (count < 0) {
		count = -count;
		for (i = 0; i < (ULONG)count; i++) {
			if (!TTX_Cmd_MoveLeft(app, session, NULL, 0))
				break;
		}
		return TRUE;
	}
	return TRUE;
}

BOOL TTX_Cmd_MoveDown(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    LONG count = 1;
    ULONG i = 0;
    
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    /* Parse count from args */
    if (args && argCount > 0) {
        /* TODO: Parse numeric - for now use 1 */
        count = 1;
    }
    
    /* Move cursor down by count lines */
    for (i = 0; i < (ULONG)count && TTX_SessionView(session)->cursorY < TT_SessionBuffer(session)->lineCount - 1; i++) {
        TTX_SessionView(session)->cursorY++;
        if (TTX_SessionView(session)->cursorX > TT_SessionBuffer(session)->lines[TTX_SessionView(session)->cursorY].length) {
            TTX_SessionView(session)->cursorX = TT_SessionBuffer(session)->lines[TTX_SessionView(session)->cursorY].length;
        }
    }
    
    ScrollToCursor(session, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session);
    UpdateCursor(session->window, session);
    Printf("[CMD] TTX_Cmd_MoveDown: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveDownScr(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    ULONG pageH = 0;
    
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    pageH = TT_SessionBuffer(session)->pageH;
    if (pageH == 0) {
        pageH = 20;  /* Default if not calculated */
    }
    
    /* Move cursor down by screen height */
    TTX_SessionView(session)->cursorY += pageH;
    if (TTX_SessionView(session)->cursorY >= TT_SessionBuffer(session)->lineCount) {
        TTX_SessionView(session)->cursorY = TT_SessionBuffer(session)->lineCount - 1;
    }
    
    if (TTX_SessionView(session)->cursorX > TT_SessionBuffer(session)->lines[TTX_SessionView(session)->cursorY].length) {
        TTX_SessionView(session)->cursorX = TT_SessionBuffer(session)->lines[TTX_SessionView(session)->cursorY].length;
    }
    
    ScrollToCursor(session, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session);
    UpdateCursor(session->window, session);
    Printf("[CMD] TTX_Cmd_MoveDownScr: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveEOF(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    /* Move to end of file */
    if (TT_SessionBuffer(session)->lineCount > 0) {
        TTX_SessionView(session)->cursorY = TT_SessionBuffer(session)->lineCount - 1;
        TTX_SessionView(session)->cursorX = TT_SessionBuffer(session)->lines[TTX_SessionView(session)->cursorY].length;
        ScrollToCursor(session, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session);
        UpdateCursor(session->window, session);
        Printf("[CMD] TTX_Cmd_MoveEOF: SUCCESS\n");
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_MoveEOL(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "MoveEOL", args, argCount)) {
        Printf("[CMD] TTX_Cmd_MoveEOL: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_MoveLastChange(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "MoveLastChange", args, argCount);
}


BOOL TTX_Cmd_MoveLeft(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    LONG count = 1;
    ULONG i = 0;
    
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    /* Parse count from args */
    if (args && argCount > 0) {
        /* TODO: Parse numeric - for now use 1 */
        count = 1;
    }
    
    /* Move cursor left by count characters */
    for (i = 0; i < (ULONG)count; i++) {
        if (TTX_SessionView(session)->cursorX > 0) {
            TTX_SessionView(session)->cursorX--;
        } else if (TTX_SessionView(session)->cursorY > 0) {
            TTX_SessionView(session)->cursorY--;
            TTX_SessionView(session)->cursorX = TT_SessionBuffer(session)->lines[TTX_SessionView(session)->cursorY].length;
        } else {
            break;
        }
    }
    
    ScrollToCursor(session, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session);
    UpdateCursor(session->window, session);
    Printf("[CMD] TTX_Cmd_MoveLeft: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveMatchBkt(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "MoveMatchBkt", args, argCount);
}


BOOL TTX_Cmd_MoveNextTabStop(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "MoveNextTabStop", args, argCount);
}


BOOL TTX_Cmd_MoveNextWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "MoveNextWord", args, argCount)) {
        Printf("[CMD] TTX_Cmd_MoveNextWord: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_MovePrevTabStop(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "MovePrevTabStop", args, argCount);
}


BOOL TTX_Cmd_MovePrevWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "MovePrevWord", args, argCount)) {
        Printf("[CMD] TTX_Cmd_MovePrevWord: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_MoveRight(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    LONG count = 1;
    ULONG i = 0;
    
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    /* Parse count from args */
    if (args && argCount > 0) {
        /* TODO: Parse numeric - for now use 1 */
        count = 1;
    }
    
    /* Move cursor right by count characters */
    for (i = 0; i < (ULONG)count; i++) {
        if (TTX_SessionView(session)->cursorY < TT_SessionBuffer(session)->lineCount) {
            if (TTX_SessionView(session)->cursorX < TT_SessionBuffer(session)->lines[TTX_SessionView(session)->cursorY].length) {
                TTX_SessionView(session)->cursorX++;
            } else if (TTX_SessionView(session)->cursorY < TT_SessionBuffer(session)->lineCount - 1) {
                TTX_SessionView(session)->cursorY++;
                TTX_SessionView(session)->cursorX = 0;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    
    ScrollToCursor(session, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session);
    UpdateCursor(session->window, session);
    Printf("[CMD] TTX_Cmd_MoveRight: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveSOF(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    /* Move to start of file */
    TTX_SessionView(session)->cursorY = 0;
    TTX_SessionView(session)->cursorX = 0;
    ScrollToCursor(session, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session);
    UpdateCursor(session->window, session);
    Printf("[CMD] TTX_Cmd_MoveSOF: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveSOL(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "MoveSOL", args, argCount)) {
        Printf("[CMD] TTX_Cmd_MoveSOL: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_MoveUp(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    LONG count = 1;
    ULONG i = 0;
    
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    /* Parse count from args */
    if (args && argCount > 0) {
        /* TODO: Parse numeric - for now use 1 */
        count = 1;
    }
    
    /* Move cursor up by count lines */
    for (i = 0; i < (ULONG)count && TTX_SessionView(session)->cursorY > 0; i++) {
        TTX_SessionView(session)->cursorY--;
        if (TTX_SessionView(session)->cursorX > TT_SessionBuffer(session)->lines[TTX_SessionView(session)->cursorY].length) {
            TTX_SessionView(session)->cursorX = TT_SessionBuffer(session)->lines[TTX_SessionView(session)->cursorY].length;
        }
    }
    
    ScrollToCursor(session, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session);
    UpdateCursor(session->window, session);
    Printf("[CMD] TTX_Cmd_MoveUp: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveUpScr(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    ULONG pageH = 0;
    
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    pageH = TT_SessionBuffer(session)->pageH;
    if (pageH == 0) {
        pageH = 20;  /* Default if not calculated */
    }
    
    /* Move cursor up by screen height */
    if (TTX_SessionView(session)->cursorY >= pageH) {
        TTX_SessionView(session)->cursorY -= pageH;
    } else {
        TTX_SessionView(session)->cursorY = 0;
    }
    
    if (TTX_SessionView(session)->cursorX > TT_SessionBuffer(session)->lines[TTX_SessionView(session)->cursorY].length) {
        TTX_SessionView(session)->cursorX = TT_SessionBuffer(session)->lines[TTX_SessionView(session)->cursorY].length;
    }
    
    ScrollToCursor(session, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session);
    UpdateCursor(session->window, session);
    Printf("[CMD] TTX_Cmd_MoveUpScr: SUCCESS\n");
    return TRUE;
}

/* ============================================================================
 * Bookmark Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_ClearBookmark(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "ClearBookmark", args, argCount);
}


BOOL TTX_Cmd_MoveAutomark(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "MoveAutomark", args, argCount);
}


BOOL TTX_Cmd_MoveBookmark(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "MoveBookmark", args, argCount);
}


BOOL TTX_Cmd_SetBookmark(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "SetBookmark", args, argCount);
}


/* ============================================================================
 * Editing Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_Delete(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "Delete", args, argCount)) {
        Printf("[CMD] TTX_Cmd_Delete: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_DeleteEOL(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "DeleteEOL", args, argCount)) {
        Printf("[CMD] TTX_Cmd_DeleteEOL: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_DeleteEOW(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "DeleteEOW", args, argCount)) {
        Printf("[CMD] TTX_Cmd_DeleteEOW: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_DeleteLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "DeleteLine", args, argCount)) {
        Printf("[CMD] TTX_Cmd_DeleteLine: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_DeleteSOL(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "DeleteSOL", args, argCount)) {
        Printf("[CMD] TTX_Cmd_DeleteSOL: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_DeleteSOW(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "DeleteSOW", args, argCount)) {
        Printf("[CMD] TTX_Cmd_DeleteSOW: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_FindChange(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR useArgs[8];
	ULONG useCount;
	ULONG i;

	if (!session || session->document->state.readOnly)
		return FALSE;
	if (!args || argCount < 2 || !args[0] || !args[1])
		return FALSE;

	if (TTX_LastFind)
		TTX_Free(TTX_LastFind);
	TTX_LastFind = TTX_DupStr(args[0]);
	if (TTX_LastReplace)
		TTX_Free(TTX_LastReplace);
	TTX_LastReplace = TTX_DupStr(args[1]);

	useArgs[0] = args[0];
	useArgs[1] = args[1];
	useCount = 2;
	if (!TTX_LastFindOpts.ignoreCase)
		useArgs[useCount++] = (STRPTR)"CaseSensitive";
	if (TTX_LastFindOpts.wholeWords)
		useArgs[useCount++] = (STRPTR)"WholeWord";
	if (TTX_LastFindOpts.scanBackwards)
		useArgs[useCount++] = (STRPTR)"Backward";
	for (i = 2; i < argCount && useCount < 8; i++) {
		if (args[i])
			useArgs[useCount++] = args[i];
	}

	if (TTX_DoEngineCommand(app, session, "FindChange", useArgs, useCount)) {
		Printf("[CMD] TTX_Cmd_FindChange: SUCCESS\n");
		return TRUE;
	}
	return FALSE;
}


BOOL TTX_Cmd_GetChar(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    UBYTE ch = 0;
    
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    ch = GetCharAtCursor(TT_SessionBuffer(session));
    Printf("[CMD] TTX_Cmd_GetChar: character='%c' (0x%02x)\n", (ch >= 32 && ch < 127) ? ch : '?', (unsigned int)ch);
    /* TODO: Return to ARexx via RESULT */
    return TRUE;
}

BOOL TTX_Cmd_GetLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    STRPTR lineText = NULL;
    
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    lineText = GetCurrentLine(TT_SessionBuffer(session));
    if (lineText) {
        Printf("[CMD] TTX_Cmd_GetLine: line='%s'\n", lineText);
        /* TODO: Return to ARexx via RESULT */
        TTX_Free(lineText);
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_Insert(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (args && argCount > 0 && args[0]) {
        if (TTX_DoEngineCommand(app, session, "Insert", args, argCount)) {
            Printf("[CMD] TTX_Cmd_Insert: SUCCESS\n");
            return TRUE;
        }
    }

    return FALSE;
}

BOOL TTX_Cmd_InsertLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR useArgs[4];
	ULONG useCount;
	ULONG i;
	BOOL haveIndent;

	if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
		return FALSE;
	}

	useCount = 0;
	haveIndent = FALSE;
	for (i = 0; i < argCount && useCount < 3; i++) {
		if (args[i]) {
			useArgs[useCount++] = args[i];
			if (Stricmp(args[i], "Indent") == 0)
				haveIndent = TRUE;
		}
	}
	if (!haveIndent && TR_PrefsGet() && TR_PrefsGet()->autoIndentNewLines)
		useArgs[useCount++] = (STRPTR)"Indent";

	if (TTX_DoEngineCommand(app, session, "InsertLine",
		useCount ? useArgs : NULL, useCount)) {
		Printf("[CMD] TTX_Cmd_InsertLine: SUCCESS\n");
		return TRUE;
	}

	return FALSE;
}

BOOL TTX_Cmd_SetChar(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    UBYTE ch = 0;
    
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }
    
    if (args && argCount > 0 && args[0] && args[0][0] != '\0') {
        ch = (UBYTE)args[0][0];
        if (SetCharAtCursor(TT_SessionBuffer(session), ch)) {
            CalculateMaxScroll(session, session->window);
            ScrollToCursor(session, session->window);
            UpdateScrollBars(session);
            RenderText(session->window, session);
            UpdateCursor(session->window, session);
            session->document->state.modified = TT_SessionBuffer(session)->modified;
            Printf("[CMD] TTX_Cmd_SetChar: SUCCESS\n");
            return TRUE;
        }
    }
    
    return FALSE;
}

BOOL TTX_Cmd_SwapChars(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "SwapChars", args, argCount)) {
        Printf("[CMD] TTX_Cmd_SwapChars: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_Text(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* Text is alias for Insert */
    return TTX_Cmd_Insert(app, session, args, argCount);
}

BOOL TTX_Cmd_ToggleCharCase(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "ToggleCharCase", args, argCount)) {
        Printf("[CMD] TTX_Cmd_ToggleCharCase: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_UndeleteLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "UndeleteLine", args, argCount);
}


BOOL TTX_Cmd_UndoLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "UndoLine", args, argCount);
}


/* ============================================================================
 * Word-Level Editing Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_CompleteTemplate(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement template completion */
    Printf("[CMD] TTX_Cmd_CompleteTemplate: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_CorrectWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement word correction */
    Printf("[CMD] TTX_Cmd_CorrectWord: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_CorrectWordCase(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement word case correction */
    Printf("[CMD] TTX_Cmd_CorrectWordCase: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "GetWord", args, argCount);
}


BOOL TTX_Cmd_ReplaceWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "ReplaceWord", args, argCount);
}


/* ============================================================================
 * Formatting Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_Center(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "Center", args, argCount);
}


BOOL TTX_Cmd_Conv2Lower(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "Conv2Lower", args, argCount)) {
        Printf("[CMD] TTX_Cmd_Conv2Lower: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_Conv2Spaces(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	TEXT twBuf[16];
	STRPTR useArgs[1];
	ULONG useCount;
	struct TTXPrefs *p;

	if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
		return FALSE;
	}

	useCount = 0;
	p = TTX_PrefsGet();
	if (p && p->tabWidth > 0) {
		sprintf(twBuf, "%lu", (unsigned long)p->tabWidth);
		useArgs[0] = twBuf;
		useCount = 1;
	} else if (args && argCount > 0) {
		useArgs[0] = args[0];
		useCount = 1;
	}

	if (TTX_DoEngineCommand(app, session, "Conv2Spaces",
				useCount ? useArgs : args, useCount)) {
		Printf("[CMD] TTX_Cmd_Conv2Spaces: SUCCESS\n");
		return TRUE;
	}

	return FALSE;
}

BOOL TTX_Cmd_Conv2Tabs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	TEXT twBuf[16];
	STRPTR useArgs[1];
	ULONG useCount;
	struct TTXPrefs *p;

	if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
		return FALSE;
	}

	useCount = 0;
	p = TTX_PrefsGet();
	if (p && p->tabWidth > 0) {
		sprintf(twBuf, "%lu", (unsigned long)p->tabWidth);
		useArgs[0] = twBuf;
		useCount = 1;
	} else if (args && argCount > 0) {
		useArgs[0] = args[0];
		useCount = 1;
	}

	if (TTX_DoEngineCommand(app, session, "Conv2Tabs",
				useCount ? useArgs : args, useCount)) {
		Printf("[CMD] TTX_Cmd_Conv2Tabs: SUCCESS\n");
		return TRUE;
	}

	return FALSE;
}

BOOL TTX_Cmd_Conv2Upper(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "Conv2Upper", args, argCount)) {
        Printf("[CMD] TTX_Cmd_Conv2Upper: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_FormatParagraph(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "FormatParagraph", args, argCount);
}


BOOL TTX_Cmd_Justify(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	return TTX_DoEngineCommand(app, session, "Justify", args, argCount);
}


BOOL TTX_Cmd_ShiftLeft(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "ShiftLeft", args, argCount)) {
        Printf("[CMD] TTX_Cmd_ShiftLeft: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_ShiftRight(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "ShiftRight", args, argCount)) {
        Printf("[CMD] TTX_Cmd_ShiftRight: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

/* ============================================================================
 * Fold Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_HideFold(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement fold hiding */
    Printf("[CMD] TTX_Cmd_HideFold: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_MakeFold(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement fold creation */
    Printf("[CMD] TTX_Cmd_MakeFold: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_ShowFold(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement fold showing */
    Printf("[CMD] TTX_Cmd_ShowFold: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_ToggleFold(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement fold toggling */
    Printf("[CMD] TTX_Cmd_ToggleFold: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_UnmakeFold(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement fold removal */
    Printf("[CMD] TTX_Cmd_UnmakeFold: not yet implemented\n");
    return FALSE;
}

/* ============================================================================
 * Macro Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_EndMacro(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: End macro recording */
    Printf("[CMD] TTX_Cmd_EndMacro: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_ExecARexxMacro(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	ULONG i = 0;
	BOOL console = FALSE;
	TEXT cmdBuf[512];
	ULONG pos = 0;
	STRPTR selected = NULL;
	BOOL first = TRUE;
	BOOL ok = FALSE;

	(void)session;
	if (!app)
		return FALSE;

	/* Skip CONSOLE / LOCKINPUT / LOCKDISPLAY / NOCACHE switches. */
	while (i < argCount && args[i]) {
		if (Stricmp(args[i], "CONSOLE") == 0)
			console = TRUE;
		else if (Stricmp(args[i], "LOCKINPUT") == 0)
			; /* lock stubs: accepted, not yet enforced */
		else if (Stricmp(args[i], "LOCKDISPLAY") == 0)
			;
		else if (Stricmp(args[i], "NOCACHE") == 0)
			;
		else
			break;
		i++;
	}

	cmdBuf[0] = '\0';
	pos = 0;
	while (i < argCount && args[i] && pos < sizeof(cmdBuf) - 1) {
		ULONG n = 0;
		if (!first && pos < sizeof(cmdBuf) - 1)
			cmdBuf[pos++] = ' ';
		first = FALSE;
		while (args[i][n] != '\0' && pos < sizeof(cmdBuf) - 1)
			cmdBuf[pos++] = args[i][n++];
		i++;
	}
	cmdBuf[pos] = '\0';

	if (cmdBuf[0] == '\0') {
		selected = TTX_ShowFileRequester(app, session, NULL, NULL);
		if (!selected)
			return FALSE;
		ok = TTX_ArexxExec(app, selected, FALSE, console);
		TTX_Free(selected);
		return ok;
	}

	return TTX_ArexxExec(app, cmdBuf, FALSE, console);
}

BOOL TTX_Cmd_ExecARexxString(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	ULONG i = 0;
	BOOL console = FALSE;
	TEXT cmdBuf[512];
	ULONG pos = 0;
	BOOL first = TRUE;

	(void)session;
	if (!app)
		return FALSE;

	while (i < argCount && args[i]) {
		if (Stricmp(args[i], "CONSOLE") == 0)
			console = TRUE;
		else if (Stricmp(args[i], "LOCKINPUT") == 0)
			;
		else if (Stricmp(args[i], "LOCKDISPLAY") == 0)
			;
		else
			break;
		i++;
	}

	cmdBuf[0] = '\0';
	pos = 0;
	while (i < argCount && args[i] && pos < sizeof(cmdBuf) - 1) {
		ULONG n = 0;
		if (!first && pos < sizeof(cmdBuf) - 1)
			cmdBuf[pos++] = ' ';
		first = FALSE;
		while (args[i][n] != '\0' && pos < sizeof(cmdBuf) - 1)
			cmdBuf[pos++] = args[i][n++];
		i++;
	}
	cmdBuf[pos] = '\0';

	if (cmdBuf[0] == '\0')
		return FALSE;

	return TTX_ArexxExec(app, cmdBuf, TRUE, console);
}

BOOL TTX_Cmd_FlushARexxCache(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Flush ARexx macro cache */
    Printf("[CMD] TTX_Cmd_FlushARexxCache: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetARexxCache(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Get ARexx cache state */
    Printf("[CMD] TTX_Cmd_GetARexxCache: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetMacroInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return macro info for ARexx */
    Printf("[CMD] TTX_Cmd_GetMacroInfo: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_OpenMacro(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Open macro file */
    Printf("[CMD] TTX_Cmd_OpenMacro: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_PlayMacro(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Play recorded macro */
    Printf("[CMD] TTX_Cmd_PlayMacro: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_RecordMacro(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Start macro recording */
    Printf("[CMD] TTX_Cmd_RecordMacro: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SaveMacro(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Save recorded macro */
    Printf("[CMD] TTX_Cmd_SaveMacro: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetARexxCache(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set ARexx cache state */
    Printf("[CMD] TTX_Cmd_SetARexxCache: not yet implemented\n");
    return FALSE;
}

/* ============================================================================
 * External Tool Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_ExecTool(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Execute external tool */
    Printf("[CMD] TTX_Cmd_ExecTool: not yet implemented\n");
    return FALSE;
}

/* ============================================================================
 * Configuration Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_GetPrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	struct TTXPrefs *p;
	STRPTR name;
	TEXT buf[32];

	(void)session;
	p = TTX_PrefsGet();
	name = (args && argCount > 0 && args[0]) ? args[0] : NULL;
	if (!name) {
		TTX_ArexxSetResult(app, (STRPTR)"");
		return FALSE;
	}
	buf[0] = '\0';
	if (Stricmp(name, "Overstrike") == 0)
		strcpy(buf, p->overstrike ? "ON" : "OFF");
	else if (Stricmp(name, "FreeForm") == 0)
		strcpy(buf, p->freeForm ? "ON" : "OFF");
	else if (Stricmp(name, "AutoIndent") == 0 ||
		 Stricmp(name, "AutoIndentNewLines") == 0)
		strcpy(buf, p->autoIndentNewLines ? "ON" : "OFF");
	else if (Stricmp(name, "WordWrap") == 0)
		strcpy(buf, p->wordWrap ? "ON" : "OFF");
	else if (Stricmp(name, "LineWrap") == 0)
		strcpy(buf, p->lineWrap ? "ON" : "OFF");
	else if (Stricmp(name, "SelectWhenDragging") == 0)
		strcpy(buf, p->selectWhenDragging ? "ON" : "OFF");
	else if (Stricmp(name, "AutoCorrectWordCase") == 0)
		strcpy(buf, p->autoCorrectWordCase ? "ON" : "OFF");
	else if (Stricmp(name, "AutoEraseSelectedBlocks") == 0)
		strcpy(buf, p->autoEraseSelectedBlocks ? "ON" : "OFF");
	else if (Stricmp(name, "ExpandTabs") == 0)
		strcpy(buf, p->expandTabs ? "ON" : "OFF");
	else if (Stricmp(name, "TabWidth") == 0)
		sprintf(buf, "%lu", (unsigned long)p->tabWidth);
	else if (Stricmp(name, "RightMargin") == 0)
		sprintf(buf, "%lu", (unsigned long)p->rightMargin);
	else {
		TTX_ArexxSetResult(app, (STRPTR)"");
		return FALSE;
	}
	TTX_ArexxSetResult(app, buf);
	return TRUE;
}

BOOL TTX_Cmd_OpenDefinitions(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR path;
	struct Window *win;
	struct Session *s;
	BPTR lock;
	BOOL owned;

	win = (session && session->window) ? session->window : NULL;
	path = (args && argCount > 0 && args[0]) ? args[0] : NULL;
	owned = FALSE;
	if (!path) {
		path = TTX_RequestFile(win, (STRPTR)"Open Definitions", FALSE,
			(STRPTR)"TTX_BuiltIn.dfn", (STRPTR)"PROGDIR:Support");
		if (!path)
			return FALSE;
		owned = TRUE;
	}
	lock = Lock(path, ACCESS_READ);
	if (!lock) {
		if (owned)
			TTX_Free(path);
		return FALSE;
	}
	UnLock(lock);
	TTX_SetDefinitionsPath(path);
	if (owned)
		TTX_Free(path);

	/* Rebuild menus for every open window. */
	for (s = app ? app->sessions : NULL; s; s = s->next) {
		if (!s->window)
			continue;
		TTX_FreeMenuStrip(s);
		TTX_CreateMenuStrip(s);
	}
	return TRUE;
}

BOOL TTX_Cmd_OpenPrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR path;
	struct Window *win;
	struct TTXPrefs loaded;
	BOOL owned;

	win = (session && session->window) ? session->window : NULL;
	path = (args && argCount > 0 && args[0]) ? args[0] : NULL;
	owned = FALSE;
	if (!path) {
		path = TTX_RequestFile(win, (STRPTR)"Open Prefs", FALSE,
			(STRPTR)"TTX.prefs", NULL);
		if (!path)
			return FALSE;
		owned = TRUE;
	}
	if (!TTX_PrefsLoad(&loaded, path)) {
		if (owned)
			TTX_Free(path);
		return FALSE;
	}
	if (owned)
		TTX_Free(path);
	TTX_PrefsApply(app, session, &loaded);
	return TRUE;
}

BOOL TTX_Cmd_SavePrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR path;
	struct Window *win;
	BOOL owned;

	(void)app;
	(void)argCount;
	win = (session && session->window) ? session->window : NULL;
	path = (args && args[0]) ? args[0] : NULL;
	owned = FALSE;
	if (!path) {
		path = TTX_RequestFile(win, (STRPTR)"Save Prefs As", TRUE,
			(STRPTR)"TTX.prefs", NULL);
		if (!path)
			return FALSE;
		owned = TRUE;
	}
	if (!TTX_PrefsSave(TTX_PrefsGet(), path)) {
		if (owned)
			TTX_Free(path);
		return FALSE;
	}
	if (owned)
		TTX_Free(path);
	return TRUE;
}

BOOL TTX_Cmd_SaveDefPrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	(void)app;
	(void)session;
	(void)args;
	(void)argCount;
	return TTX_PrefsSave(TTX_PrefsGet(), (STRPTR)"PROGDIR:TTX.prefs");
}

BOOL TTX_Cmd_SetPrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	struct TTXPrefs *p;
	STRPTR name;
	STRPTR mode;
	BOOL *flag;
	BOOL newVal;
	LONG n;

	p = TTX_PrefsGet();
	name = (args && argCount > 0 && args[0]) ? args[0] : NULL;
	mode = (args && argCount > 1 && args[1]) ? args[1] : (STRPTR)"Toggle";
	if (!name)
		return FALSE;

	flag = NULL;
	if (Stricmp(name, "Overstrike") == 0)
		flag = &p->overstrike;
	else if (Stricmp(name, "FreeForm") == 0)
		flag = &p->freeForm;
	else if (Stricmp(name, "AutoIndent") == 0 ||
		 Stricmp(name, "AutoIndentNewLines") == 0)
		flag = &p->autoIndentNewLines;
	else if (Stricmp(name, "WordWrap") == 0)
		flag = &p->wordWrap;
	else if (Stricmp(name, "LineWrap") == 0)
		flag = &p->lineWrap;
	else if (Stricmp(name, "SelectWhenDragging") == 0)
		flag = &p->selectWhenDragging;
	else if (Stricmp(name, "AutoCorrectWordCase") == 0)
		flag = &p->autoCorrectWordCase;
	else if (Stricmp(name, "AutoEraseSelectedBlocks") == 0)
		flag = &p->autoEraseSelectedBlocks;
	else if (Stricmp(name, "ExpandTabs") == 0)
		flag = &p->expandTabs;
	else if (Stricmp(name, "TabWidth") == 0) {
		n = (LONG)p->tabWidth;
		if (mode && StrToLong(mode, &n) && n > 0)
			p->tabWidth = (ULONG)n;
		TTX_PrefsApply(app, session, p);
		return TRUE;
	} else if (Stricmp(name, "RightMargin") == 0) {
		n = (LONG)p->rightMargin;
		if (mode && StrToLong(mode, &n) && n > 0)
			p->rightMargin = (ULONG)n;
		TTX_PrefsApply(app, session, p);
		return TRUE;
	} else
		return FALSE;

	if (Stricmp(mode, "Toggle") == 0)
		newVal = (BOOL)(!*flag);
	else if (Stricmp(mode, "ON") == 0 || Stricmp(mode, "YES") == 0 ||
		 Stricmp(mode, "TRUE") == 0)
		newVal = TRUE;
	else if (Stricmp(mode, "OFF") == 0 || Stricmp(mode, "NO") == 0 ||
		 Stricmp(mode, "FALSE") == 0)
		newVal = FALSE;
	else
		return FALSE;
	*flag = newVal;
	TTX_PrefsApply(app, session, p);
	return TRUE;
}

/* ============================================================================
 * ARexx Input Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_RequestBool(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR title;
	STRPTR prompt;
	struct Window *win;
	BOOL yes;

	title = (args && argCount > 0 && args[0]) ? args[0] : (STRPTR)"TTX";
	prompt = (args && argCount > 1 && args[1]) ? args[1] : (STRPTR)"OK?";
	win = (session && session->window) ? session->window : NULL;
	yes = TTX_RequestBool(win, title, prompt);
	TTX_ArexxSetResult(app, yes ? (STRPTR)"YES" : (STRPTR)"NO");
	return TRUE;
}

BOOL TTX_Cmd_RequestChoice(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR title;
	STRPTR prompt;
	STRPTR gadgets;
	struct Window *win;
	LONG choice;
	TEXT buf[16];

	title = (args && argCount > 0 && args[0]) ? args[0] : (STRPTR)"TTX";
	prompt = (args && argCount > 1 && args[1]) ? args[1] : (STRPTR)"?";
	gadgets = (args && argCount > 2 && args[2]) ? args[2] : (STRPTR)"OK|Cancel";
	win = (session && session->window) ? session->window : NULL;
	choice = TTX_RequestChoice(win, title, prompt, gadgets);
	sprintf(buf, "%ld", (long)choice);
	TTX_ArexxSetResult(app, buf);
	return (choice != 0) ? TRUE : FALSE;
}

BOOL TTX_Cmd_RequestFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	struct Window *win;
	STRPTR path;
	STRPTR title;

	(void)argCount;
	title = (args && args[0]) ? args[0] : (STRPTR)"Select File";
	win = (session && session->window) ? session->window : NULL;
	path = TTX_RequestFile(win, title, FALSE, NULL, NULL);
	if (!path) {
		TTX_ArexxSetResult(app, (STRPTR)"");
		return FALSE;
	}
	TTX_ArexxSetResult(app, path);
	TTX_Free(path);
	return TRUE;
}

BOOL TTX_Cmd_RequestNum(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	struct Window *win;
	STRPTR title;
	LONG defVal;
	LONG val;
	BOOL positiveOnly;
	TEXT buf[32];
	ULONG i;

	title = (STRPTR)"TTX";
	defVal = 0;
	positiveOnly = FALSE;
	win = (session && session->window) ? session->window : NULL;

	for (i = 0; args && i < argCount; i++) {
		if (!args[i])
			continue;
		if (Stricmp(args[i], "POSITIVE") == 0)
			positiveOnly = TRUE;
		else if (Stricmp(args[i], "DEFAULT") == 0 && (i + 1) < argCount) {
			StrToLong(args[i + 1], &defVal);
			i++;
		} else
			title = args[i];
	}

	if (!TTX_RequestNum(win, title, defVal, positiveOnly, &val))
		return FALSE;
	sprintf(buf, "%ld", (long)val);
	TTX_ArexxSetResult(app, buf);
	return TRUE;
}

BOOL TTX_Cmd_RequestStr(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	struct Window *win;
	STRPTR title;
	STRPTR defStr;
	STRPTR result;
	ULONG i;

	title = (STRPTR)"TTX";
	defStr = (STRPTR)"";
	win = (session && session->window) ? session->window : NULL;

	for (i = 0; args && i < argCount; i++) {
		if (!args[i])
			continue;
		if (Stricmp(args[i], "PROMPT") == 0 && (i + 1) < argCount) {
			title = args[i + 1];
			i++;
		} else
			defStr = args[i];
	}

	if (!TTX_RequestStr(win, title, defStr, &result))
		return FALSE;
	TTX_ArexxSetResult(app, result ? result : (STRPTR)"");
	if (result)
		TTX_Free(result);
	return TRUE;
}

/* ============================================================================
 * ARexx Control Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_GetBackground(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return background state for ARexx */
    Printf("[CMD] TTX_Cmd_GetBackground: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetCurrentDir(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	(void)args;
	(void)argCount;
	if (!app || !session)
		return FALSE;
	/* Untitled / late init: resolve process CWD into the session once. */
	if (!session->currentDir || session->currentDir[0] == '\0')
		TTX_SessionInitCurrentDir(session, NULL);
	if (session->currentDir && session->currentDir[0] != '\0')
		TTX_ArexxSetResult(app, session->currentDir);
	else
		TTX_ArexxSetResult(app, (STRPTR)"");
	return TRUE;
}

BOOL TTX_Cmd_GetDocuments(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	struct Session *s = NULL;
	TEXT buf[TTX_AREXX_RESULT_MAX];
	ULONG pos = 0;
	BOOL first = TRUE;
	STRPTR name = NULL;
	STRPTR base = NULL;

	(void)session;
	(void)args;
	(void)argCount;
	if (!app)
		return FALSE;

	buf[0] = '\0';
	s = app->sessions;
	while (s) {
		ULONG n = 0;

		if (s->arexxPortName[0] == '\0')
			TTX_ArexxBindSession(app, s);

		name = NULL;
		if (s->document && s->document->state.fileName)
			name = s->document->state.fileName;
		base = name ? FilePart(name) : (STRPTR)"Untitled";
		if (!base || base[0] == '\0')
			base = (STRPTR)"Untitled";

		if (!first && pos < sizeof(buf) - 1)
			buf[pos++] = ' ';
		first = FALSE;

		if (pos < sizeof(buf) - 1)
			buf[pos++] = '"';
		n = 0;
		while (base[n] != '\0' && pos < sizeof(buf) - 2)
			buf[pos++] = base[n++];
		if (pos < sizeof(buf) - 1)
			buf[pos++] = '"';
		if (pos < sizeof(buf) - 1)
			buf[pos++] = ' ';
		n = 0;
		while (s->arexxPortName[n] != '\0' && pos < sizeof(buf) - 1)
			buf[pos++] = s->arexxPortName[n++];

		s = s->next;
	}
	buf[pos] = '\0';
	TTX_ArexxSetResult(app, buf);
	return TRUE;
}

BOOL TTX_Cmd_GetErrorInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return error info for ARexx */
    Printf("[CMD] TTX_Cmd_GetErrorInfo: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetLockInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return lock info for ARexx */
    Printf("[CMD] TTX_Cmd_GetLockInfo: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetPort(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	struct Session *s = NULL;
	STRPTR want = NULL;
	STRPTR base = NULL;

	if (!app || !session)
		return FALSE;

	/* Optional NAME/F: look up document by window/file basename. */
	if (args && argCount > 0 && args[0] && args[0][0] != '\0') {
		want = args[0];
		s = app->sessions;
		while (s) {
			base = NULL;
			if (s->document && s->document->state.fileName)
				base = FilePart(s->document->state.fileName);
			if (!base || base[0] == '\0')
				base = (STRPTR)"Untitled";
			if (Stricmp(base, want) == 0 ||
			    (s->document && s->document->state.fileName &&
			     Stricmp(s->document->state.fileName, want) == 0))
			{
				if (s->arexxPortName[0] == '\0')
					TTX_ArexxBindSession(app, s);
				TTX_ArexxSetResult(app, s->arexxPortName);
				return TRUE;
			}
			s = s->next;
		}
		return FALSE;
	}

	if (session->arexxPortName[0] == '\0')
		TTX_ArexxBindSession(app, session);
	Printf("[CMD] TTX_Cmd_GetPort: '%s'\n", session->arexxPortName);
	TTX_ArexxSetResult(app, session->arexxPortName);
	return TRUE;
}

BOOL TTX_Cmd_GetPriority(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return priority for ARexx */
    Printf("[CMD] TTX_Cmd_GetPriority: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetBackground(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set background mode */
    Printf("[CMD] TTX_Cmd_SetBackground: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetCurrentDir(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
	STRPTR path = NULL;

	(void)app;
	if (!session)
		return FALSE;
	if (args && argCount > 0 && args[0] && args[0][0] != '\0')
		path = args[0];
	if (!path)
		return FALSE;
	return TTX_SessionSetCurrentDir(session, path);
}

BOOL TTX_Cmd_SetDisplayLock(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set display lock */
    Printf("[CMD] TTX_Cmd_SetDisplayLock: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetInputLock(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set input lock */
    Printf("[CMD] TTX_Cmd_SetInputLock: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetMeta(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set meta mode */
    Printf("[CMD] TTX_Cmd_SetMeta: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetMeta2(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set meta2 mode */
    Printf("[CMD] TTX_Cmd_SetMeta2: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetMode(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set editing mode */
    Printf("[CMD] TTX_Cmd_SetMode: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetMode2(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set editing mode2 */
    Printf("[CMD] TTX_Cmd_SetMode2: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetPriority(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set process priority */
    Printf("[CMD] TTX_Cmd_SetPriority: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetQuoteMode(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set quote mode */
    Printf("[CMD] TTX_Cmd_SetQuoteMode: not yet implemented\n");
    return FALSE;
}

/* ============================================================================
 * Helper Commands
 * ============================================================================ */

BOOL TTX_Cmd_Help(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Open help window */
    Printf("[CMD] TTX_Cmd_Help: not yet implemented\n");
    return FALSE;
}

/****************************************************************************/

VOID
TTX_ProcessDeferredActions(struct TTXApplication *app)
{
	ULONG action = 0;
	STRPTR deferArgs[1];
	struct Session *closeSession = NULL;
	struct Session *openSession = NULL;

	if (!app)
		return;

	/* Never run nested Intuition while an IntuiMessage is still being handled */
	if (app->intuiHandlerDepth > 0)
		return;

	closeSession = app->deferredCloseSession;
	if (closeSession) {
		app->deferredCloseSession = NULL;
		TTX_DestroySession(app, closeSession);
		TTX_RebuildSignalMask(app);
	}

	action = app->deferredAction;
	if (action == TTX_DEFER_NONE)
		return;

	openSession = app->deferredOpenSession;
	if (!openSession)
		openSession = app->activeSession;

	app->deferredAction = TTX_DEFER_NONE;
	app->deferredOpenSession = NULL;

	if (action == TTX_DEFER_OPENFILE_FILEREQ) {
		TTX_Cmd_OpenFile(app, openSession, NULL, 0);
	} else if (action == TTX_DEFER_OPENDOC_FILEREQ) {
		deferArgs[0] = (STRPTR)"FileReq";
		TTX_Cmd_OpenDoc(app, openSession, deferArgs, 1);
	} else if (action == TTX_DEFER_OPENDOC_NEW) {
		TTX_Cmd_OpenDoc(app, openSession, NULL, 0);
	}
}
