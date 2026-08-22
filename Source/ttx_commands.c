/*
 * TTX - Command Handler Functions
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_driver.h"
#include "ttx_commands_prot.h"
#include "ttx_menu_builtin.h"
#include "ttx.h"

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
		CalculateMaxScroll(TT_SessionBuffer(session), session->window);
		ScrollToCursor(TT_SessionBuffer(session), session->window);
		UpdateScrollBars(session);
		RenderText(session->window, session);
		UpdateCursor(session->window, session);
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
                CalculateMaxScroll(TT_SessionBuffer(session), session->window);
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
    /* View commands */
    else if (Stricmp(command, "CenterView") == 0) {
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
    /* Menu 0: Project */
    if (extractedMenu == 0) {
        switch (extractedItem) {
            case 0: *outCommand = "OpenDoc"; if (outArgs && outArgCount) { outArgs[0] = "FileReq"; *outArgCount = 1; } break;
            case 1: *outCommand = "OpenDoc"; break;
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
    /* Menu 1: Windows */
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
                    }
                }
                break;
            default: return FALSE;
        }
        return TRUE;
    }
    /* Menu 4: Macros, Menu 5: Folds, Menu 6: Extras, Menu 7: Prefs - TODO */
    else {
        return FALSE;
    }
}

BOOL TTX_HandleMenuPick(struct TTXApplication *app, struct Session *session, ULONG menuNumber, ULONG itemNumber)
{
    STRPTR command = NULL;
    STRPTR args[10] = {NULL};
    ULONG argCount = 0;
    BOOL result = FALSE;
    
    if (!app || !session) {
        return FALSE;
    }
    
    Printf("[MENU] TTX_HandleMenuPick: called with menuNumber=%lu, itemNumber=%lu\n", menuNumber, itemNumber);
    
    /* Check for MENUNULL (no selection) - menuNumber and itemNumber are both 0xFFFF */
    if (menuNumber == 0xFFFF && itemNumber == 0xFFFF) {
        return TRUE;  /* MENUNULL - no action needed */
    }
    
    /* Get command from menu/item number */
    /* Note: The UserData is stored in the NewMenu structure and accessed via GTMENUITEM_USERDATA */
    /* The menuCode from IDCMP_MENUPICK is used in ttx.c to extract UserData */
    /* Here we use menuNumber/itemNumber directly to map to commands */
    if (!GetCommandFromMenuPick(menuNumber, itemNumber, &command, args, &argCount)) {
        Printf("[MENU] TTX_HandleMenuPick: no command found for menu=%lu, item=%lu\n", menuNumber, itemNumber);
        return FALSE;
    }
    
    if (command) {
        result = TTX_HandleCommand(app, session, command, args, argCount);
        /* Free allocated argument strings */
        if (args[0] && argCount > 0) {
            ULONG i;
            for (i = 0; i < argCount; i++) {
                if (args[i]) {
                    /* Only free if it's not a constant string literal */
                    /* Check if it's an allocated string by comparing pointers */
                    BOOL isConstant = FALSE;
                    if (Stricmp(args[i], "Toggle") == 0 || 
                        Stricmp(args[i], "FileReq") == 0 ||
                        Stricmp(args[i], "Info") == 0 ||
                        Stricmp(args[i], "Find") == 0 ||
                        Stricmp(args[i], "FindChange") == 0 ||
                        Stricmp(args[i], "Vertical") == 0) {
                        isConstant = TRUE;
                    }
                    if (!isConstant) {
                        TTX_Free(args[i]);
                    }
                }
            }
        }
    }
    
    return result;
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
    static const STRPTR dfnPath = "PROGDIR:Support/TTX_BuiltIn.dfn";
    BPTR dfnLock = NULL;
    
    if (!session || !session->window) {
        return FALSE;
    }
    
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
    dfnLock = Lock((STRPTR)dfnPath, ACCESS_READ);
    if (dfnLock) {
        UnLock(dfnLock);
        dfn = ParseDFNFile((STRPTR)dfnPath);
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
    STRPTR oldFileName = NULL;
    
    Printf("[CMD] TTX_Cmd_OpenFile: START\n");
    
    if (!app || !session || !TT_SessionBuffer(session)) {
        Printf("[CMD] TTX_Cmd_OpenFile: FAIL (app=%lx, session=%lx, buffer=%lx)\n", 
               (ULONG)app, (ULONG)session, (ULONG)(session ? TT_SessionBuffer(session) : NULL));
        return FALSE;
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
    
    /* Save old filename for cleanup */
    oldFileName = session->document->state.fileName;
    
    /* Allocate new filename and copy it */
    if (fileName ) {
        ULONG fileNameLen = 0;
        STRPTR newFileName = NULL;
        STRPTR endPtr = NULL;
        STRPTR tempPtr = NULL;
        
        /* Calculate string length (utility.library V39 doesn't have Strlen) */
        tempPtr = fileName;
        while (tempPtr && *tempPtr != '\0') {
            fileNameLen++;
            tempPtr++;
        }
        fileNameLen++; /* Add 1 for NUL terminator */
        
        newFileName = TTX_Alloc(fileNameLen, MEMF_CLEAR);
        if (newFileName) {
            endPtr = Strncpy(newFileName, fileName, fileNameLen);
            if (!endPtr) {
                /* String was truncated - this shouldn't happen since we allocated the right size */
                Printf("[CMD] TTX_Cmd_OpenFile: WARN (filename truncated)\n");
            }
            session->document->state.fileName = newFileName;
        } else {
            Printf("[CMD] TTX_Cmd_OpenFile: FAIL (could not allocate filename)\n");
            if (selectedFile ) {
                TTX_Free(selectedFile);
            }
            return FALSE;
        }
    }
    
    /* Load file via turbotext.library engine */
    {
        STRPTR openArgs[1];
        openArgs[0] = fileName;
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
    }
    
    /* Free old filename if we had one */
        if (oldFileName ) {
            TTX_Free(oldFileName);
    }
    
    /* Free selected file path if we allocated it (the filename is now in session->fileName) */
    if (selectedFile ) {
        TTX_Free(selectedFile);
    }
    
    /* Update window title */
    if (session->window && session->document->state.fileName) {
        STRPTR titleText = NULL;
        ULONG titleLen = 0;
        ULONG fileNameLen = 0;
        STRPTR endPtr = NULL;
        STRPTR tempPtr = NULL;
        
        /* Calculate filename length (utility.library V39 doesn't have Strlen) */
        tempPtr = session->document->state.fileName;
        while (tempPtr && *tempPtr != '\0') {
            fileNameLen++;
            tempPtr++;
        }
        
        titleLen = fileNameLen + 10; /* "TTX - " + filename + null */
        titleText = TTX_Alloc(titleLen, MEMF_CLEAR);
        if (titleText) {
            /* Use Strncpy chaining to concatenate strings */
            endPtr = Strncpy(titleText, "TTX - ", titleLen);
            if (endPtr) {
                Strncpy(endPtr, session->document->state.fileName, titleLen - (ULONG)(endPtr - titleText));
            }
            SetWindowTitles(session->window, titleText, (STRPTR)-1);
        }
    }
    
    /* Calculate max scroll values and update scroll bars */
    if (TT_SessionBuffer(session)) {
        CalculateMaxScroll(TT_SessionBuffer(session), session->window);
        UpdateScrollBars(session);
    }
    
    /* Reset cursor to top */
    if (TT_SessionBuffer(session)) {
        TT_SessionBuffer(session)->cursorX = 0;
        TT_SessionBuffer(session)->cursorY = 0;
    }
    
    /* Refresh display */
    if (TT_SessionBuffer(session) && session->window) {
        RenderText(session->window, session);
        UpdateCursor(session->window, session);
    }
    
    Printf("[CMD] TTX_Cmd_OpenFile: SUCCESS (lines=%lu)\n",
           TT_SessionBuffer(session) ? TT_SessionBuffer(session)->lineCount : 0);
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
               useFileReq ? "OpenFile" : "OpenNew");
        app->deferredAction = useFileReq ? TTX_DEFER_OPENFILE_FILEREQ
                                         : TTX_DEFER_OPENDOC_NEW;
        app->deferredOpenSession = session;
        return TRUE;
    }
    
    if (useFileReq) {
        /* Open with file requester: load into the CURRENT window */
        STRPTR openArgs[1];

        if (!AslBase) {
            Printf("[CMD] TTX_Cmd_OpenDoc: FAIL (ASL library not available)\n");
            return FALSE;
        }
        
        selectedFile = TTX_ShowFileRequester(app, session, NULL, NULL);
        if (!selectedFile) {
            /* User cancelled or error */
            Printf("[CMD] TTX_Cmd_OpenDoc: cancelled or failed\n");
            return FALSE;
        }
        
        openArgs[0] = selectedFile;
        result = TTX_Cmd_OpenFile(app, session, openArgs, 1);
        
        if (selectedFile) {
            TTX_Free(selectedFile);
        }
        
        return result;
    } else {
        /* Open New: file requester, then load into a NEW window */
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
    STRPTR oldFileName = NULL;
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
    
    /* Save old filename to free later if we allocated a new one */
    oldFileName = session->document->state.fileName;
    
    /* Allocate and copy new filename */
    if (fileName) {
        ULONG len = 0;
        while (fileName[len] != '\0') {
            len++;
        }
        if (len > 0) {
            session->document->state.fileName = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
            if (session->document->state.fileName) {
                CopyMem(fileName, session->document->state.fileName, len);
                session->document->state.fileName[len] = '\0';
            } else {
                Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (TTX_Alloc fileName failed)\n");
                /* Free selected file if we allocated it */
                if (selectedFile ) {
                    TTX_Free(selectedFile);
                }
                return FALSE;
            }
        }
    }
    
    /* Save file via engine */
    if (TTX_DoEngineCommand(app, session, "SaveFile", NULL, 0)) {
        session->document->state.modified = FALSE;
        if (TT_SessionBuffer(session))
            TT_SessionBuffer(session)->modified = FALSE;
        result = TRUE;
        Printf("[CMD] TTX_Cmd_SaveFileAs: SUCCESS (saved to '%s')\n", session->document->state.fileName);
        
        /* Free old filename if we replaced it */
        if (oldFileName && oldFileName != session->document->state.fileName ) {
            TTX_Free(oldFileName);
        }
    } else {
        LONG errorCode = IoErr();
        if (errorCode != 0) {
            PrintFault(errorCode, "TTX");
            SetIoErr(0);
        }
        Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (SaveFile failed)\n");
        
        /* Restore old filename on failure */
        if (session->document->state.fileName && session->document->state.fileName != oldFileName ) {
            TTX_Free(session->document->state.fileName);
        }
        session->document->state.fileName = oldFileName;
        result = FALSE;
    }
    
    /* Free selected file path if we allocated it */
    if (selectedFile ) {
        TTX_Free(selectedFile);
    }
    
    /* Free initial file and drawer strings if we allocated them */
    if (initialFile && 1 && initialFile != fileName) {
        TTX_Free(initialFile);
    }
    if (initialDrawer ) {
        TTX_Free(initialDrawer);
    }
    
    return result;
}

BOOL TTX_Cmd_ClearFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    ULONG i = 0;
    
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    /* Clear all lines except first empty line */
    for (i = 1; i < TT_SessionBuffer(session)->lineCount; i++) {
        if (TT_SessionBuffer(session)->lines[i].text) {
            TTX_Free(TT_SessionBuffer(session)->lines[i].text);
            TT_SessionBuffer(session)->lines[i].text = NULL;
        }
    }
    TT_SessionBuffer(session)->lineCount = 1;
    TT_SessionBuffer(session)->lines[0].length = 0;
    if (TT_SessionBuffer(session)->lines[0].text) {
        TT_SessionBuffer(session)->lines[0].text[0] = '\0';
    }
    TT_SessionBuffer(session)->cursorX = 0;
    TT_SessionBuffer(session)->cursorY = 0;
    TT_SessionBuffer(session)->scrollX = 0;
    TT_SessionBuffer(session)->scrollY = 0;
        TT_SessionBuffer(session)->modified = TRUE;
    session->document->state.modified = TRUE;
    
    /* Force full redraw */
    session->render.needsFullRedraw = TRUE;
    
    ScrollToCursor(TT_SessionBuffer(session), session->window);
    RenderText(session->window, session);
    UpdateCursor(session->window, session);
    
    Printf("[CMD] TTX_Cmd_ClearFile: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_PrintFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement printing */
    Printf("[CMD] TTX_Cmd_PrintFile: not yet implemented\n");
    return FALSE;
}

/* Prompt user before closing if document is modified */
static BOOL PromptSaveBeforeClose(struct TTXApplication *app, struct Session *session)
{
    struct IntuiText bodyText;
    struct IntuiText posText;
    struct IntuiText negText;
    BOOL result = FALSE;
    STRPTR bodyStr = "Document has been modified.\nSave before closing?";
    STRPTR posStr = "Save";
    STRPTR negStr = "Cancel";
    
    if (!session || !session->window || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    /* Only prompt if document is modified */
    if (!session->document->state.modified) {
        return TRUE; /* Not modified - OK to close */
    }
    
    /* Set up IntuiText structures */
    bodyText.FrontPen = 0;
    bodyText.BackPen = 1;
    bodyText.DrawMode = JAM2;
    bodyText.LeftEdge = 0;
    bodyText.TopEdge = 0;
    bodyText.ITextFont = NULL; /* Use default font */
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
    result = AutoRequest(session->window, &bodyText, &posText, &negText, 0, 0, 320, 100);
    
    if (result) {
        /* User chose "Save" - save the file */
        if (session->document->state.fileName) {
            /* Save to existing filename */
            TTX_Cmd_SaveFile(app, session, NULL, 0);
        } else {
            /* No filename - use Save As */
            TTX_Cmd_SaveFileAs(app, session, NULL, 0);
        }
        /* If save was cancelled, don't close */
        if (session->document->state.modified) {
            return FALSE; /* Save was cancelled */
        }
    } else {
        /* User chose "Cancel" - don't close */
        return FALSE;
    }
    
    return TRUE; /* OK to close */
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
    /* Return version string - for ARexx compatibility, would return in RESULT */
    /* For now, just print it */
    Printf("[CMD] TTX_Cmd_GetVersion: version='TTX 3.0'\n");
    return TRUE;
}

BOOL TTX_Cmd_GetReadOnly(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session) {
        return FALSE;
    }
    
    /* Return read-only state - for ARexx compatibility, would return in RESULT */
    Printf("[CMD] TTX_Cmd_GetReadOnly: readOnly=%s\n", session->document->state.readOnly ? "TRUE" : "FALSE");
    return TRUE;
}

/* ============================================================================
 * Document Commands
 * ============================================================================ */

BOOL TTX_Cmd_ActivateLastDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    struct Session *lastSession = NULL;
    
    if (!app) {
        return FALSE;
    }
    
    /* Find last activated session (for now, just use first session) */
    /* TODO: Track activation order */
    lastSession = app->sessions;
    if (lastSession && lastSession->window) {
        WindowToFront(lastSession->window);
        ActivateWindow(lastSession->window);
        app->activeSession = lastSession;
        Printf("[CMD] TTX_Cmd_ActivateLastDoc: SUCCESS\n");
        return TRUE;
    }
    
    Printf("[CMD] TTX_Cmd_ActivateLastDoc: FAIL (no session)\n");
    return FALSE;
}

BOOL TTX_Cmd_ActivateNextDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    struct Session *nextSession = NULL;
    
    if (!app || !session) {
        return FALSE;
    }
    
    /* Find next session in list */
    nextSession = session->next;
    if (!nextSession) {
        /* Wrap to first session */
        nextSession = app->sessions;
    }
    
    if (nextSession && nextSession->window) {
        WindowToFront(nextSession->window);
        ActivateWindow(nextSession->window);
        app->activeSession = nextSession;
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
    
    if (!app || !session) {
        return FALSE;
    }
    
    /* Find previous session in list */
    prevSession = session->prev;
    if (!prevSession) {
        /* Wrap to last session */
        currentSession = app->sessions;
        while (currentSession && currentSession->next) {
            currentSession = currentSession->next;
        }
        prevSession = currentSession;
    }
    
    if (prevSession && prevSession->window) {
        WindowToFront(prevSession->window);
        ActivateWindow(prevSession->window);
        app->activeSession = prevSession;
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
    
    if (!session) {
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
    
    if (openWindow && !session->window) {
        /* TODO: Restore window */
        Printf("[CMD] TTX_Cmd_ControlWindow: window restore not yet implemented\n");
        return FALSE;
    } else if (!openWindow && session->window) {
        /* Close window but keep session */
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
    /* TODO: Return cursor state for ARexx */
    Printf("[CMD] TTX_Cmd_GetCursor: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetScreenInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return screen info for ARexx */
    Printf("[CMD] TTX_Cmd_GetScreenInfo: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetWindowInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return window info for ARexx */
    Printf("[CMD] TTX_Cmd_GetWindowInfo: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_IconifyWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!app || !session) {
        return FALSE;
    }
    
    /* Iconify just this window (not the whole app) */
    if (session->window) {
        /* TODO: Implement window-level iconification */
        Printf("[CMD] TTX_Cmd_IconifyWindow: not yet implemented\n");
        return FALSE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_MoveSizeWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement window move and size */
    Printf("[CMD] TTX_Cmd_MoveSizeWindow: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_MoveWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->window) {
        return FALSE;
    }
    
    /* Parse position from args */
    if (args && argCount >= 2) {
        /* TODO: Parse numeric args */
        Printf("[CMD] TTX_Cmd_MoveWindow: numeric parsing not yet implemented\n");
        return FALSE;
    }
    
    /* MoveWindow() is relative; ChangeWindowBox() sets absolute position */
    ChangeWindowBox(session->window, session->windowState.leftEdge,
                    session->windowState.topEdge, session->window->Width,
                    session->window->Height);
    Printf("[CMD] TTX_Cmd_MoveWindow: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_OpenRequester(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement requester opening */
    Printf("[CMD] TTX_Cmd_OpenRequester: not yet implemented\n");
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
    /* TODO: Implement cursor style setting */
    Printf("[CMD] TTX_Cmd_SetCursor: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetStatusBar(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement status bar setting */
    Printf("[CMD] TTX_Cmd_SetStatusBar: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SizeWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement window sizing */
    Printf("[CMD] TTX_Cmd_SizeWindow: not yet implemented\n");
    return FALSE;
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
    /* TODO: Implement view centering */
    Printf("[CMD] TTX_Cmd_CenterView: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetViewInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return view info for ARexx */
    Printf("[CMD] TTX_Cmd_GetViewInfo: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_ScrollView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    LONG deltaX = 0;
    LONG deltaY = 0;
    
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    /* Parse scroll deltas from args */
    if (args && argCount >= 2) {
        /* TODO: Parse numeric args - for now use 0 */
        deltaX = 0;
        deltaY = 0;
    } else if (args && argCount >= 1) {
        /* Single arg = vertical scroll */
        deltaY = 0;  /* TODO: Parse numeric */
    }
    
    /* Apply scroll */
    if (deltaY > 0) {
        TT_SessionBuffer(session)->scrollY += (ULONG)deltaY;
        if (TT_SessionBuffer(session)->scrollY > TT_SessionBuffer(session)->maxScrollY) {
            TT_SessionBuffer(session)->scrollY = TT_SessionBuffer(session)->maxScrollY;
        }
    } else if (deltaY < 0) {
        if ((ULONG)(-deltaY) > TT_SessionBuffer(session)->scrollY) {
            TT_SessionBuffer(session)->scrollY = 0;
        } else {
            TT_SessionBuffer(session)->scrollY -= (ULONG)(-deltaY);
        }
    }
    
    if (deltaX > 0) {
        TT_SessionBuffer(session)->scrollX += (ULONG)deltaX;
        if (TT_SessionBuffer(session)->scrollX > TT_SessionBuffer(session)->maxScrollX) {
            TT_SessionBuffer(session)->scrollX = TT_SessionBuffer(session)->maxScrollX;
        }
    } else if (deltaX < 0) {
        if ((ULONG)(-deltaX) > TT_SessionBuffer(session)->scrollX) {
            TT_SessionBuffer(session)->scrollX = 0;
        } else {
            TT_SessionBuffer(session)->scrollX -= (ULONG)(-deltaX);
        }
    }
    
    UpdateScrollBars(session);
    RenderText(session->window, session);
    UpdateCursor(session->window, session);
    Printf("[CMD] TTX_Cmd_ScrollView: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_SizeView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    ULONG width = 0;
    ULONG height = 0;
    
    if (!session || !session->window) {
        return FALSE;
    }
    
    /* Parse size from args */
    if (args && argCount >= 2) {
        /* TODO: Parse numeric args */
        width = 0;
        height = 0;
    }
    
    /* Resize window if sizes provided */
    if (width > 0 && height > 0) {
        /* TODO: Implement window resizing */
        Printf("[CMD] TTX_Cmd_SizeView: window resize not yet implemented\n");
        return FALSE;
    }
    
    /* Recalculate max scroll for current window size */
    if (TT_SessionBuffer(session)) {
        CalculateMaxScroll(TT_SessionBuffer(session), session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session);
        UpdateCursor(session->window, session);
    }
    
    Printf("[CMD] TTX_Cmd_SizeView: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_SplitView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement view splitting */
    Printf("[CMD] TTX_Cmd_SplitView: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SwapViews(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement view swapping */
    Printf("[CMD] TTX_Cmd_SwapViews: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SwitchView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement view switching */
    Printf("[CMD] TTX_Cmd_SwitchView: not yet implemented\n");
    return FALSE;
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
    
    if (!session || !TT_SessionBuffer(session) || !TT_SessionBuffer(session)->marking.enabled) {
        Printf("[CMD] TTX_Cmd_CopyBlk: FAIL (no selection)\n");
        return FALSE;
    }
    
    /* Get selected text */
    blockText = GetBlock(TT_SessionBuffer(session));
    if (!blockText) {
        Printf("[CMD] TTX_Cmd_CopyBlk: FAIL (GetBlock failed)\n");
        return FALSE;
    }
    
    /* TODO: Copy to clipboard - for now just print */
    Printf("[CMD] TTX_Cmd_CopyBlk: SUCCESS (text='%s')\n", blockText);
    
    /* Free block text */
    TTX_Free(blockText);
    
    return TRUE;
}

BOOL TTX_Cmd_CutBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    STRPTR blockText = NULL;
    
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }
    
    if (!TT_SessionBuffer(session)->marking.enabled) {
        Printf("[CMD] TTX_Cmd_CutBlk: FAIL (no selection)\n");
        return FALSE;
    }
    
    /* Get selected text */
    blockText = GetBlock(TT_SessionBuffer(session));
    if (!blockText) {
        Printf("[CMD] TTX_Cmd_CutBlk: FAIL (GetBlock failed)\n");
        return FALSE;
    }
    
    /* TODO: Copy to clipboard - for now just print */
    Printf("[CMD] TTX_Cmd_CutBlk: SUCCESS (text='%s')\n", blockText);
    
    /* Delete the block via engine */
    if (!TTX_DoEngineCommand(app, session, "DeleteBlk", NULL, 0)) {
        TTX_Free(blockText);
        Printf("[CMD] TTX_Cmd_CutBlk: FAIL (DeleteBlk failed)\n");
        return FALSE;
    }

    /* Free block text */
    TTX_Free(blockText);

    Printf("[CMD] TTX_Cmd_CutBlk: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_DeleteBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (!TT_SessionBuffer(session)->marking.enabled) {
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
    /* TODO: Return block text for ARexx */
    Printf("[CMD] TTX_Cmd_GetBlk: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetBlkInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return block info for ARexx */
    Printf("[CMD] TTX_Cmd_GetBlkInfo: not yet implemented\n");
    return FALSE;
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
    /* TODO: Implement clipboard opening */
    Printf("[CMD] TTX_Cmd_OpenClip: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_PasteClip(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement clipboard pasting */
    Printf("[CMD] TTX_Cmd_PasteClip: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_PrintClip(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement clipboard printing */
    Printf("[CMD] TTX_Cmd_PrintClip: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SaveClip(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement clipboard saving */
    Printf("[CMD] TTX_Cmd_SaveClip: not yet implemented\n");
    return FALSE;
}

/* ============================================================================
 * File Commands (some already implemented)
 * ============================================================================ */

BOOL TTX_Cmd_GetFileInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return file info for ARexx */
    Printf("[CMD] TTX_Cmd_GetFileInfo: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetFilePath(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return file path for ARexx */
    Printf("[CMD] TTX_Cmd_GetFilePath: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetFilePath(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set file path */
    Printf("[CMD] TTX_Cmd_SetFilePath: not yet implemented\n");
    return FALSE;
}

/* ============================================================================
 * Cursor Position Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_Find(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement find/search */
    Printf("[CMD] TTX_Cmd_Find: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetCursorPos(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return cursor position for ARexx */
    Printf("[CMD] TTX_Cmd_GetCursorPos: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_Move(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement generic move command */
    Printf("[CMD] TTX_Cmd_Move: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_MoveChar(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    LONG count = 1;
    
    if (!session || !TT_SessionBuffer(session)) {
        return FALSE;
    }
    
    /* Parse count from args */
    if (args && argCount > 0) {
        /* TODO: Parse numeric - for now use 1 */
        count = 1;
    }
    
    /* Move cursor by count characters (positive = right, negative = left) */
    if (count > 0) {
        return TTX_Cmd_MoveRight(app, session, args, argCount);
    } else if (count < 0) {
        return TTX_Cmd_MoveLeft(app, session, args, argCount);
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
    for (i = 0; i < (ULONG)count && TT_SessionBuffer(session)->cursorY < TT_SessionBuffer(session)->lineCount - 1; i++) {
        TT_SessionBuffer(session)->cursorY++;
        if (TT_SessionBuffer(session)->cursorX > TT_SessionBuffer(session)->lines[TT_SessionBuffer(session)->cursorY].length) {
            TT_SessionBuffer(session)->cursorX = TT_SessionBuffer(session)->lines[TT_SessionBuffer(session)->cursorY].length;
        }
    }
    
    ScrollToCursor(TT_SessionBuffer(session), session->window);
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
    TT_SessionBuffer(session)->cursorY += pageH;
    if (TT_SessionBuffer(session)->cursorY >= TT_SessionBuffer(session)->lineCount) {
        TT_SessionBuffer(session)->cursorY = TT_SessionBuffer(session)->lineCount - 1;
    }
    
    if (TT_SessionBuffer(session)->cursorX > TT_SessionBuffer(session)->lines[TT_SessionBuffer(session)->cursorY].length) {
        TT_SessionBuffer(session)->cursorX = TT_SessionBuffer(session)->lines[TT_SessionBuffer(session)->cursorY].length;
    }
    
    ScrollToCursor(TT_SessionBuffer(session), session->window);
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
        TT_SessionBuffer(session)->cursorY = TT_SessionBuffer(session)->lineCount - 1;
        TT_SessionBuffer(session)->cursorX = TT_SessionBuffer(session)->lines[TT_SessionBuffer(session)->cursorY].length;
        ScrollToCursor(TT_SessionBuffer(session), session->window);
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
    /* TODO: Move to last change position */
    Printf("[CMD] TTX_Cmd_MoveLastChange: not yet implemented\n");
    return FALSE;
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
        if (TT_SessionBuffer(session)->cursorX > 0) {
            TT_SessionBuffer(session)->cursorX--;
        } else if (TT_SessionBuffer(session)->cursorY > 0) {
            TT_SessionBuffer(session)->cursorY--;
            TT_SessionBuffer(session)->cursorX = TT_SessionBuffer(session)->lines[TT_SessionBuffer(session)->cursorY].length;
        } else {
            break;
        }
    }
    
    ScrollToCursor(TT_SessionBuffer(session), session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session);
    UpdateCursor(session->window, session);
    Printf("[CMD] TTX_Cmd_MoveLeft: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveMatchBkt(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Move to matching bracket */
    Printf("[CMD] TTX_Cmd_MoveMatchBkt: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_MoveNextTabStop(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Move to next tab stop */
    Printf("[CMD] TTX_Cmd_MoveNextTabStop: not yet implemented\n");
    return FALSE;
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
    /* TODO: Move to previous tab stop */
    Printf("[CMD] TTX_Cmd_MovePrevTabStop: not yet implemented\n");
    return FALSE;
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
        if (TT_SessionBuffer(session)->cursorY < TT_SessionBuffer(session)->lineCount) {
            if (TT_SessionBuffer(session)->cursorX < TT_SessionBuffer(session)->lines[TT_SessionBuffer(session)->cursorY].length) {
                TT_SessionBuffer(session)->cursorX++;
            } else if (TT_SessionBuffer(session)->cursorY < TT_SessionBuffer(session)->lineCount - 1) {
                TT_SessionBuffer(session)->cursorY++;
                TT_SessionBuffer(session)->cursorX = 0;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    
    ScrollToCursor(TT_SessionBuffer(session), session->window);
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
    TT_SessionBuffer(session)->cursorY = 0;
    TT_SessionBuffer(session)->cursorX = 0;
    ScrollToCursor(TT_SessionBuffer(session), session->window);
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
    for (i = 0; i < (ULONG)count && TT_SessionBuffer(session)->cursorY > 0; i++) {
        TT_SessionBuffer(session)->cursorY--;
        if (TT_SessionBuffer(session)->cursorX > TT_SessionBuffer(session)->lines[TT_SessionBuffer(session)->cursorY].length) {
            TT_SessionBuffer(session)->cursorX = TT_SessionBuffer(session)->lines[TT_SessionBuffer(session)->cursorY].length;
        }
    }
    
    ScrollToCursor(TT_SessionBuffer(session), session->window);
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
    if (TT_SessionBuffer(session)->cursorY >= pageH) {
        TT_SessionBuffer(session)->cursorY -= pageH;
    } else {
        TT_SessionBuffer(session)->cursorY = 0;
    }
    
    if (TT_SessionBuffer(session)->cursorX > TT_SessionBuffer(session)->lines[TT_SessionBuffer(session)->cursorY].length) {
        TT_SessionBuffer(session)->cursorX = TT_SessionBuffer(session)->lines[TT_SessionBuffer(session)->cursorY].length;
    }
    
    ScrollToCursor(TT_SessionBuffer(session), session->window);
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
    /* TODO: Implement bookmark clearing */
    Printf("[CMD] TTX_Cmd_ClearBookmark: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_MoveAutomark(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Move to automatic bookmark */
    Printf("[CMD] TTX_Cmd_MoveAutomark: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_MoveBookmark(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Move to bookmark */
    Printf("[CMD] TTX_Cmd_MoveBookmark: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetBookmark(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set bookmark */
    Printf("[CMD] TTX_Cmd_SetBookmark: not yet implemented\n");
    return FALSE;
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
    /* TODO: Implement find and change */
    Printf("[CMD] TTX_Cmd_FindChange: not yet implemented\n");
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
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "InsertLine", args, argCount)) {
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
            CalculateMaxScroll(TT_SessionBuffer(session), session->window);
            ScrollToCursor(TT_SessionBuffer(session), session->window);
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
    /* TODO: Undelete last deleted line */
    Printf("[CMD] TTX_Cmd_UndeleteLine: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_UndoLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Undo last change to line */
    Printf("[CMD] TTX_Cmd_UndoLine: not yet implemented\n");
    return FALSE;
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
    (void)app;
    (void)args;
    (void)argCount;

    Printf("[CMD] TTX_Cmd_GetWord: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_ReplaceWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    Printf("[CMD] TTX_Cmd_ReplaceWord: not yet implemented\n");
    (void)app;
    (void)session;
    (void)args;
    (void)argCount;
    return FALSE;
}

/* ============================================================================
 * Formatting Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_Center(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement line/block centering */
    Printf("[CMD] TTX_Cmd_Center: not yet implemented\n");
    return FALSE;
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
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "Conv2Spaces", args, argCount)) {
        Printf("[CMD] TTX_Cmd_Conv2Spaces: SUCCESS\n");
        return TRUE;
    }

    return FALSE;
}

BOOL TTX_Cmd_Conv2Tabs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !TT_SessionBuffer(session) || session->document->state.readOnly) {
        return FALSE;
    }

    if (TTX_DoEngineCommand(app, session, "Conv2Tabs", args, argCount)) {
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
    /* TODO: Implement paragraph formatting */
    Printf("[CMD] TTX_Cmd_FormatParagraph: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_Justify(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement text justification */
    Printf("[CMD] TTX_Cmd_Justify: not yet implemented\n");
    return FALSE;
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
    /* TODO: Execute ARexx macro */
    Printf("[CMD] TTX_Cmd_ExecARexxMacro: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_ExecARexxString(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Execute ARexx string */
    Printf("[CMD] TTX_Cmd_ExecARexxString: not yet implemented\n");
    return FALSE;
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
    /* TODO: Return preferences for ARexx */
    Printf("[CMD] TTX_Cmd_GetPrefs: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_OpenDefinitions(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Open definition file */
    Printf("[CMD] TTX_Cmd_OpenDefinitions: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_OpenPrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Open preferences requester */
    Printf("[CMD] TTX_Cmd_OpenPrefs: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SaveDefPrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Save default preferences */
    Printf("[CMD] TTX_Cmd_SaveDefPrefs: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SavePrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Save preferences */
    Printf("[CMD] TTX_Cmd_SavePrefs: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SetPrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Set preferences */
    Printf("[CMD] TTX_Cmd_SetPrefs: not yet implemented\n");
    return FALSE;
}

/* ============================================================================
 * ARexx Input Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_RequestBool(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Show boolean requester for ARexx */
    Printf("[CMD] TTX_Cmd_RequestBool: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_RequestChoice(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Show choice requester for ARexx */
    Printf("[CMD] TTX_Cmd_RequestChoice: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_RequestFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Show file requester for ARexx */
    Printf("[CMD] TTX_Cmd_RequestFile: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_RequestNum(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Show numeric requester for ARexx */
    Printf("[CMD] TTX_Cmd_RequestNum: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_RequestStr(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Show string requester for ARexx */
    Printf("[CMD] TTX_Cmd_RequestStr: not yet implemented\n");
    return FALSE;
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
    /* TODO: Return current directory for ARexx */
    Printf("[CMD] TTX_Cmd_GetCurrentDir: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_GetDocuments(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Return document list for ARexx */
    Printf("[CMD] TTX_Cmd_GetDocuments: not yet implemented\n");
    return FALSE;
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
    /* TODO: Return ARexx port name for ARexx */
    Printf("[CMD] TTX_Cmd_GetPort: not yet implemented\n");
    return FALSE;
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
    /* TODO: Set current directory */
    Printf("[CMD] TTX_Cmd_SetCurrentDir: not yet implemented\n");
    return FALSE;
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
		deferArgs[0] = (STRPTR)"FileReq";
		TTX_Cmd_OpenDoc(app, openSession, deferArgs, 1);
	} else if (action == TTX_DEFER_OPENDOC_FILEREQ) {
		deferArgs[0] = (STRPTR)"FileReq";
		TTX_Cmd_OpenDoc(app, openSession, deferArgs, 1);
	} else if (action == TTX_DEFER_OPENDOC_NEW) {
		TTX_Cmd_OpenDoc(app, openSession, NULL, 0);
	}
}
