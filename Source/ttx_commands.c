/*
 * TTX - Command Handler Functions
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx.h"

#include "seiso.h"

/* Cleanup function for menu strip (called by cleanup stack) - forward declaration */
VOID cleanupMenuStrip(APTR resource);

/* Cleanup function for ASL file requester (called by cleanup stack) */
static VOID cleanupFileRequester(APTR resource)
{
    struct FileRequester *fileReq = (struct FileRequester *)resource;
    if (fileReq && fileReq != INVALID_RESOURCE && AslBase) {
        FreeAslRequest(fileReq);
    }
}

/* Command dispatcher - maps command names to handler functions */
BOOL TTX_HandleCommand(struct TTXApplication *app, struct Session *session, STRPTR command, STRPTR *args, ULONG argCount)
{
    if (!app || !session || !command) {
        return FALSE;
    }
    
    Printf("[CMD] TTX_HandleCommand: command='%s' (argCount=%lu)\n", command, argCount);
    
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
    
    /* Menu 0 = Project menu */
    /* Note: In gadtools, menu items are numbered sequentially including bars */
    /* Menu structure: 0=Title, 1=Open, 2=OpenNew, 3=Insert, 4=Bar, 5=Save, 6=SaveAs, 7=Bar, 8=Clear, 9=Print, 10=Info, 11=Bar, 12=ReadOnly, 13=Bar, 14=Iconify, 15=Bar, 16=Close, 17=Quit */
    if (menuNumber == 0) {
        switch (itemNumber) {
            case 0:  /* Open... */
                command = "OpenFile";
                break;
            case 1:  /* Open... */
                command = "OpenFile";
                break;
            case 2:  /* Insert... */
                command = "InsertFile";
                break;
            case 3:  /* Bar - skip */
                Printf("[MENU] Bar item selected (ignored)\n");
                return TRUE;
            case 4:  /* Save */
                command = "SaveFile";
                break;
            case 5:  /* Save As... */
                command = "SaveFileAs";
                break;
            case 6:  /* Bar - skip */
                Printf("[MENU] Bar item selected (ignored)\n");
                return TRUE;
            case 7:  /* Clear */
                command = "ClearFile";
                break;
            case 8:  /* Print... */
                command = "PrintFile";
                break;
            case 9:  /* Info... */
                /* TODO: OpenRequester Info */
                Printf("[MENU] Info requester not yet implemented\n");
                return TRUE;
            case 10: /* Bar - skip */
                Printf("[MENU] Bar item selected (ignored)\n");
                return TRUE;
            case 11: /* Read-Only */
                command = "SetReadOnly";
                if (argCount < 10) {
                    args[argCount++] = "Toggle";
                }
                break;
            case 12: /* Iconify */
                command = "Iconify";
                break;
            case 13: /* Bar - skip */
                Printf("[MENU] Bar item selected (ignored)\n");
                return TRUE;
            case 14: /* Close Window */
                command = "CloseDoc";
                break;
            case 15: /* Quit */
                command = "Quit";
                break;
            default:
                Printf("[MENU] Unknown menu item: menu=%lu, item=%lu\n", menuNumber, itemNumber);
                return FALSE;
        }
    } else {
        Printf("[MENU] Other menus not yet implemented (menu=%lu, item=%lu)\n", menuNumber, itemNumber);
        return FALSE;
    }
    
    if (command) {
        result = TTX_HandleCommand(app, session, command, args, argCount);
    }
    
    return result;
}

/* Create menu strip matching DFN file structure */
BOOL TTX_CreateMenuStrip(struct Session *session)
{
    /* Store menu/item numbers in UserData for easy lookup */
    /* Format: (menuNumber << 8) | itemNumber */
    struct NewMenu newMenu[] = {
        /* Project menu */
        {NM_TITLE, "Project", NULL, 0, 0, NULL},
        {NM_ITEM, "Open...", "O", 0, 0, (APTR)((0UL << 8) | 0UL)},  /* menu 0, item 0 */
        {NM_ITEM, "Open...", "Y", 0, 0, (APTR)((0UL << 8) | 1UL)},  /* menu 0, item 1 */
        {NM_ITEM, "Insert...", NULL, 0, 0, (APTR)((0UL << 8) | 2UL)},  /* menu 0, item 2 */
        {NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, "Save", "S", 0, 0, (APTR)((0UL << 8) | 4UL)},  /* menu 0, item 4 */
        {NM_ITEM, "Save As...", "A", 0, 0, (APTR)((0UL << 8) | 5UL)},  /* menu 0, item 5 */
        {NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, "Clear", "K", 0, 0, (APTR)((0UL << 8) | 7UL)},  /* menu 0, item 7 */
        {NM_ITEM, "Print...", "P", 0, 0, (APTR)((0UL << 8) | 8UL)},  /* menu 0, item 8 */
        {NM_ITEM, "Info...", "?", 0, 0, (APTR)((0UL << 8) | 9UL)},  /* menu 0, item 9 */
        {NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, "Read-Only", NULL, 0, CHECKIT, (APTR)((0UL << 8) | 11UL)},  /* menu 0, item 11 */
        {NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, "Iconify", "I", 0, 0, (APTR)((0UL << 8) | 12UL)},  /* menu 0, item 12 */
        {NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, "Close Window", "Q", 0, 0, (APTR)((0UL << 8) | 13UL)},  /* menu 0, item 13 */
        {NM_ITEM, "Quit", NULL, 0, 0, (APTR)((0UL << 8) | 14UL)},  /* menu 0, item 14 */
        
        /* End marker */
        {NM_END, NULL, NULL, 0, 0, NULL}
    };
    struct Menu *menuStrip = NULL;
    struct VisualInfo *visInfo = NULL;
    
    if (!session || !session->window) {
        return FALSE;
    }
    
    Printf("[MENU] TTX_CreateMenuStrip: START\n");
    
    /* Create menu strip */
    menuStrip = CreateMenus(newMenu, TAG_DONE);
    if (!menuStrip) {
        Printf("[MENU] TTX_CreateMenuStrip: FAIL (CreateMenus failed)\n");
        return FALSE;
    }
    
    /* Get visual info for layout */
    visInfo = GetVisualInfo(session->window->WScreen, TAG_END);
    if (!visInfo) {
        Printf("[MENU] TTX_CreateMenuStrip: FAIL (GetVisualInfo failed)\n");
        FreeMenus(menuStrip);
        return FALSE;
    }
    
    /* Layout menus */
    if (!LayoutMenus(menuStrip, visInfo, 
                     GTMN_NewLookMenus, TRUE,
                     TAG_END)) {
        Printf("[MENU] TTX_CreateMenuStrip: FAIL (LayoutMenus failed)\n");
        FreeVisualInfo(visInfo);
        FreeMenus(menuStrip);
        return FALSE;
    }
    
    /* Set menu strip on window */
    if (!SetMenuStrip(session->window, menuStrip)) {
        Printf("[MENU] TTX_CreateMenuStrip: FAIL (SetMenuStrip failed)\n");
        FreeVisualInfo(visInfo);
        FreeMenus(menuStrip);
        return FALSE;
    }
    
    /* Store menu strip in session */
    session->menuStrip = menuStrip;
    
    /* Track menu strip on cleanup stack so it's automatically freed */
    /* Note: Menu must be freed BEFORE window, so we add it AFTER window on the stack */
    /* Cleanup stack processes in LIFO order, so menu (added last) will be freed first */
    /* Use RESOURCE_TYPE_MEMORY with custom cleanup function since there's no RESOURCE_TYPE_MENU */
    if (session->cleanupStack) {
        if (!PushResource(RESOURCE_TYPE_MEMORY, menuStrip, cleanupMenuStrip)) {
            Printf("[MENU] TTX_CreateMenuStrip: WARN (failed to track menu on cleanup stack)\n");
            /* Continue anyway - menu will be freed manually in TTX_FreeMenuStrip */
        } else {
            Printf("[MENU] TTX_CreateMenuStrip: menu tracked on cleanup stack\n");
        }
    }
    
    /* Free visual info (no longer needed after LayoutMenus) */
    FreeVisualInfo(visInfo);
    
    Printf("[MENU] TTX_CreateMenuStrip: SUCCESS\n");
    return TRUE;
}

/* Cleanup function for menu strip (called by cleanup stack) */
VOID cleanupMenuStrip(APTR resource)
{
    struct Menu *menuStrip = (struct Menu *)resource;
    
    if (!menuStrip || menuStrip == INVALID_RESOURCE) {
        return;
    }
    
    /* Note: We can't access the window here since we only have the menu pointer */
    /* The menu will be cleared when the window is closed, but we still need to free it */
    Printf("[CLEANUP] cleanupMenuStrip: freeing menu strip=%lx\n", (ULONG)menuStrip);
    FreeMenus(menuStrip);
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
        
        /* Remove from cleanup stack tracking (if tracked) */
        if (session->cleanupStack) {
            UntrackResource(session->menuStrip);
        }
        
        /* Free the menu */
        FreeMenus(session->menuStrip);
        session->menuStrip = NULL;
    }
}

/* Helper function to show file requester for opening files */
static STRPTR TTX_ShowFileRequester(struct TTXApplication *app, struct Session *session, STRPTR initialFile, STRPTR initialDrawer)
{
    struct FileRequester *fileReq = NULL;
    STRPTR selectedFile = NULL;
    STRPTR selectedDrawer = NULL;
    STRPTR fullPath = NULL;
    ULONG fullPathLen = 0;
    ULONG drawerLen = 0;
    ULONG fileLen = 0;
    struct Window *window = NULL;
    struct TagItem tags[11];
    
    if (!app || !app->cleanupStack || !AslBase) {
        Printf("[ASL] TTX_ShowFileRequester: FAIL (ASL library not available)\n");
        return NULL;
    }
    
    Printf("[ASL] TTX_ShowFileRequester: START\n");
    
    /* Calculate window pointer (C89 doesn't allow ternary in constant expressions) */
    if (session) {
        window = session->window;
    }
    
    /* Initialize TagItem array (C89 compliant - all declarations at top) */
    tags[0].ti_Tag = ASLFR_Window;
    tags[0].ti_Data = (ULONG)window;
    tags[1].ti_Tag = ASLFR_TitleText;
    tags[1].ti_Data = (ULONG)"Open File";
    tags[2].ti_Tag = ASLFR_PositiveText;
    tags[2].ti_Data = (ULONG)"Open";
    tags[3].ti_Tag = ASLFR_NegativeText;
    tags[3].ti_Data = (ULONG)"Cancel";
    tags[4].ti_Tag = ASLFR_InitialFile;
    tags[4].ti_Data = (ULONG)initialFile;
    tags[5].ti_Tag = ASLFR_InitialDrawer;
    tags[5].ti_Data = (ULONG)initialDrawer;
    tags[6].ti_Tag = ASLFR_DoPatterns;
    tags[6].ti_Data = TRUE;
    tags[7].ti_Tag = ASLFR_RejectIcons;
    tags[7].ti_Data = TRUE;
    tags[8].ti_Tag = ASLFR_DoSaveMode;
    tags[8].ti_Data = FALSE;
    tags[9].ti_Tag = ASLFR_DoMultiSelect;
    tags[9].ti_Data = FALSE;
    tags[10].ti_Tag = TAG_DONE;
    tags[10].ti_Data = 0;
    
    fileReq = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, tags);
    
    if (!fileReq) {
        Printf("[ASL] TTX_ShowFileRequester: FAIL (AllocAslRequest failed)\n");
        return NULL;
    }
    
    /* Track requester on cleanup stack */
        if (!PushResource(RESOURCE_TYPE_MEMORY, fileReq, cleanupFileRequester)) {
        Printf("[ASL] TTX_ShowFileRequester: WARN (failed to track requester on cleanup stack)\n");
    }
    
    /* Show requester */
    if (AslRequest(fileReq, NULL)) {
        /* User clicked Open - extract file path */
        selectedFile = fileReq->fr_File;
        selectedDrawer = fileReq->fr_Drawer;
        
        /* Build full path: drawer + file */
        if (selectedDrawer) {
            drawerLen = 0;
            while (selectedDrawer[drawerLen] != '\0') {
                drawerLen++;
            }
        }
        if (selectedFile) {
            fileLen = 0;
            while (selectedFile[fileLen] != '\0') {
                fileLen++;
            }
        }
        
        /* Allocate full path string */
        fullPathLen = drawerLen + fileLen + 2;  /* +2 for '/' and '\0' */
        if (fullPathLen > 2) {
            fullPath = (STRPTR)allocVec(fullPathLen, MEMF_CLEAR);
            if (fullPath) {
                /* Copy drawer */
                if (selectedDrawer && drawerLen > 0) {
                    CopyMem(selectedDrawer, fullPath, drawerLen);
                    fullPath[drawerLen] = '/';
                    /* Copy file */
                    if (selectedFile && fileLen > 0) {
                        CopyMem(selectedFile, &fullPath[drawerLen + 1], fileLen);
                        fullPath[drawerLen + 1 + fileLen] = '\0';
                    } else {
                        fullPath[drawerLen + 1] = '\0';
                    }
                } else if (selectedFile && fileLen > 0) {
                    /* No drawer, just file */
                    CopyMem(selectedFile, fullPath, fileLen);
                    fullPath[fileLen] = '\0';
                } else {
                    /* Empty selection */
                    freeVec(fullPath);
                    fullPath = NULL;
                }
                
                if (fullPath) {
                    Printf("[ASL] TTX_ShowFileRequester: selected file='%s'\n", fullPath);
                }
            }
        }
    } else {
        Printf("[ASL] TTX_ShowFileRequester: user cancelled\n");
    }
    
    /* Remove from cleanup stack and free requester */
    if (fileReq) {
        UntrackResource(fileReq);
        if (AslBase) {
            FreeAslRequest(fileReq);
        }
    }
    
    return fullPath;
}

/* Helper function to show file requester for saving files */
static STRPTR TTX_ShowSaveFileRequester(struct TTXApplication *app, struct Session *session, STRPTR initialFile, STRPTR initialDrawer)
{
    struct FileRequester *fileReq = NULL;
    STRPTR selectedFile = NULL;
    STRPTR selectedDrawer = NULL;
    STRPTR fullPath = NULL;
    ULONG fullPathLen = 0;
    ULONG drawerLen = 0;
    ULONG fileLen = 0;
    struct Window *window = NULL;
    struct TagItem tags[11];
    
    if (!app || !app->cleanupStack || !AslBase) {
        Printf("[ASL] TTX_ShowSaveFileRequester: FAIL (ASL library not available)\n");
        return NULL;
    }
    
    Printf("[ASL] TTX_ShowSaveFileRequester: START\n");
    
    /* Calculate window pointer (C89 doesn't allow ternary in constant expressions) */
    if (session) {
        window = session->window;
    }
    
    /* Initialize TagItem array (C89 compliant - all declarations at top) */
    tags[0].ti_Tag = ASLFR_Window;
    tags[0].ti_Data = (ULONG)window;
    tags[1].ti_Tag = ASLFR_TitleText;
    tags[1].ti_Data = (ULONG)"Save File As";
    tags[2].ti_Tag = ASLFR_PositiveText;
    tags[2].ti_Data = (ULONG)"Save";
    tags[3].ti_Tag = ASLFR_NegativeText;
    tags[3].ti_Data = (ULONG)"Cancel";
    tags[4].ti_Tag = ASLFR_InitialFile;
    tags[4].ti_Data = (ULONG)initialFile;
    tags[5].ti_Tag = ASLFR_InitialDrawer;
    tags[5].ti_Data = (ULONG)initialDrawer;
    tags[6].ti_Tag = ASLFR_DoPatterns;
    tags[6].ti_Data = FALSE;
    tags[7].ti_Tag = ASLFR_RejectIcons;
    tags[7].ti_Data = TRUE;
    tags[8].ti_Tag = ASLFR_DoSaveMode;
    tags[8].ti_Data = TRUE;
    tags[9].ti_Tag = ASLFR_DoMultiSelect;
    tags[9].ti_Data = FALSE;
    tags[10].ti_Tag = TAG_DONE;
    tags[10].ti_Data = 0;
    
    fileReq = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, tags);
    
    if (!fileReq) {
        Printf("[ASL] TTX_ShowSaveFileRequester: FAIL (AllocAslRequest failed)\n");
        return NULL;
    }
    
    /* Track requester on cleanup stack */
        if (!PushResource(RESOURCE_TYPE_MEMORY, fileReq, cleanupFileRequester)) {
        Printf("[ASL] TTX_ShowSaveFileRequester: WARN (failed to track requester on cleanup stack)\n");
    }
    
    /* Show requester */
    if (AslRequest(fileReq, NULL)) {
        /* User clicked Save - extract file path */
        selectedFile = fileReq->fr_File;
        selectedDrawer = fileReq->fr_Drawer;
        
        /* Build full path: drawer + file */
        if (selectedDrawer) {
            drawerLen = 0;
            while (selectedDrawer[drawerLen] != '\0') {
                drawerLen++;
            }
        }
        if (selectedFile) {
            fileLen = 0;
            while (selectedFile[fileLen] != '\0') {
                fileLen++;
            }
        }
        
        /* Allocate full path string */
        fullPathLen = drawerLen + fileLen + 2;  /* +2 for '/' and '\0' */
        if (fullPathLen > 2) {
            fullPath = (STRPTR)allocVec(fullPathLen, MEMF_CLEAR);
            if (fullPath) {
                /* Copy drawer */
                if (selectedDrawer && drawerLen > 0) {
                    CopyMem(selectedDrawer, fullPath, drawerLen);
                    fullPath[drawerLen] = '/';
                    /* Copy file */
                    if (selectedFile && fileLen > 0) {
                        CopyMem(selectedFile, &fullPath[drawerLen + 1], fileLen);
                        fullPath[drawerLen + 1 + fileLen] = '\0';
                    } else {
                        fullPath[drawerLen + 1] = '\0';
                    }
                } else if (selectedFile && fileLen > 0) {
                    /* No drawer, just file */
                    CopyMem(selectedFile, fullPath, fileLen);
                    fullPath[fileLen] = '\0';
                } else {
                    /* Empty selection */
                    freeVec(fullPath);
                    fullPath = NULL;
                }
                
                if (fullPath) {
                    Printf("[ASL] TTX_ShowSaveFileRequester: selected file='%s'\n", fullPath);
                }
            }
        }
    } else {
        Printf("[ASL] TTX_ShowSaveFileRequester: user cancelled\n");
    }
    
    /* Remove from cleanup stack and free requester */
    if (fileReq) {
        UntrackResource(fileReq);
        if (AslBase) {
            FreeAslRequest(fileReq);
        }
    }
    
    return fullPath;
}

/* Project menu command handlers */

BOOL TTX_Cmd_OpenFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    STRPTR fileName = NULL;
    STRPTR selectedFile = NULL;
    STRPTR oldFileName = NULL;
    
    Printf("[CMD] TTX_Cmd_OpenFile: START\n");
    
    if (!app || !session || !session->buffer) {
        Printf("[CMD] TTX_Cmd_OpenFile: FAIL (app=%lx, session=%lx, buffer=%lx)\n", 
               (ULONG)app, (ULONG)session, (ULONG)(session ? session->buffer : NULL));
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
    oldFileName = session->docState.fileName;
    
    /* Allocate new filename and copy it */
    if (fileName && app->cleanupStack) {
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
        
        newFileName = allocVec(fileNameLen, MEMF_CLEAR);
        if (newFileName) {
            endPtr = Strncpy(newFileName, fileName, fileNameLen);
            if (!endPtr) {
                /* String was truncated - this shouldn't happen since we allocated the right size */
                Printf("[CMD] TTX_Cmd_OpenFile: WARN (filename truncated)\n");
            }
            session->docState.fileName = newFileName;
        } else {
            Printf("[CMD] TTX_Cmd_OpenFile: FAIL (could not allocate filename)\n");
            if (selectedFile && app->cleanupStack) {
                freeVec(selectedFile);
            }
            return FALSE;
        }
    }
    
    /* Clear existing buffer and load new file */
    FreeTextBuffer(session->buffer, app->cleanupStack);
    if (!InitTextBuffer(session->buffer, app->cleanupStack)) {
        Printf("[CMD] TTX_Cmd_OpenFile: FAIL (InitTextBuffer failed)\n");
        if (selectedFile && app->cleanupStack) {
            freeVec(selectedFile);
        }
        return FALSE;
    }
    
    /* Load file into buffer */
    if (!LoadFile(fileName, session->buffer, app->cleanupStack)) {
        Printf("[CMD] TTX_Cmd_OpenFile: WARN (LoadFile failed, continuing with empty buffer)\n");
        /* Continue with empty buffer - file might not exist yet */
    }
    
    /* Free old filename if we had one */
        if (oldFileName && app->cleanupStack) {
            freeVec(oldFileName);
    }
    
    /* Free selected file path if we allocated it (the filename is now in session->fileName) */
    if (selectedFile && app->cleanupStack) {
        freeVec(selectedFile);
    }
    
    /* Update window title */
    if (session->window && session->docState.fileName) {
        STRPTR titleText = NULL;
        ULONG titleLen = 0;
        ULONG fileNameLen = 0;
        STRPTR endPtr = NULL;
        STRPTR tempPtr = NULL;
        
        /* Calculate filename length (utility.library V39 doesn't have Strlen) */
        tempPtr = session->docState.fileName;
        while (tempPtr && *tempPtr != '\0') {
            fileNameLen++;
            tempPtr++;
        }
        
        titleLen = fileNameLen + 10; /* "TTX - " + filename + null */
        titleText = allocVec(titleLen, MEMF_CLEAR);
        if (titleText) {
            /* Use Strncpy chaining to concatenate strings */
            endPtr = Strncpy(titleText, "TTX - ", titleLen);
            if (endPtr) {
                Strncpy(endPtr, session->docState.fileName, titleLen - (ULONG)(endPtr - titleText));
            }
            SetWindowTitles(session->window, titleText, (STRPTR)-1);
        }
    }
    
    /* Calculate max scroll values and update scroll bars */
    if (session->buffer) {
        CalculateMaxScroll(session->buffer, session->window);
        UpdateScrollBars(session);
    }
    
    /* Reset cursor to top */
    if (session->buffer) {
        session->buffer->cursorX = 0;
        session->buffer->cursorY = 0;
    }
    
    /* Refresh display */
    if (session->buffer && session->window) {
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
    }
    
    Printf("[CMD] TTX_Cmd_OpenFile: SUCCESS\n");
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
    
    if (useFileReq) {
        /* Open with file requester */
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
        
        /* Open file in new session */
        result = TTX_CreateSession(app, selectedFile);
        
        /* Free selected file path */
        if (selectedFile && app->cleanupStack) {
            freeVec(selectedFile);
        }
        
        return result;
    } else {
        /* Open new document */
        return TTX_CreateSession(app, NULL);
    }
}

BOOL TTX_Cmd_InsertFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    STRPTR fileName = NULL;
    STRPTR selectedFile = NULL;
    struct TextBuffer *tempBuffer = NULL;
    ULONG savedCursorX = 0;
    ULONG savedCursorY = 0;
    ULONG i = 0;
    ULONG j = 0;
    
    if (!app || !session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    /* Check if filename provided in args */
    if (args && argCount > 0 && args[0]) {
        fileName = args[0];
    }
    
    if (!fileName) {
        /* No filename provided - show file requester */
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
    
    /* Save cursor position */
    savedCursorX = session->buffer->cursorX;
    savedCursorY = session->buffer->cursorY;
    
    /* Create temporary buffer to load file */
    tempBuffer = (struct TextBuffer *)allocVec(sizeof(struct TextBuffer), MEMF_CLEAR);
    if (!tempBuffer) {
        if (selectedFile) {
            freeVec(selectedFile);
        }
        return FALSE;
    }
    
    if (!InitTextBuffer(tempBuffer, app->cleanupStack)) {
        freeVec(tempBuffer);
        if (selectedFile) {
            freeVec(selectedFile);
        }
        return FALSE;
    }
    
    /* Load file into temporary buffer */
    if (!LoadFile(fileName, tempBuffer, app->cleanupStack)) {
        FreeTextBuffer(tempBuffer, app->cleanupStack);
        freeVec(tempBuffer);
        if (selectedFile) {
            freeVec(selectedFile);
        }
        Printf("[CMD] TTX_Cmd_InsertFile: FAIL (LoadFile failed)\n");
        return FALSE;
    }
    
    /* Insert all lines from temp buffer at cursor */
    for (i = 0; i < tempBuffer->lineCount; i++) {
        if (i > 0) {
            /* Insert newline for each line after first */
            if (!InsertNewline(session->buffer, session->cleanupStack)) {
                break;
            }
        }
        
        /* Insert line text */
        if (tempBuffer->lines[i].text && tempBuffer->lines[i].length > 0) {
            for (j = 0; j < tempBuffer->lines[i].length; j++) {
                if (!InsertChar(session->buffer, (UBYTE)tempBuffer->lines[i].text[j], session->cleanupStack)) {
                    break;
                }
            }
        }
    }
    
    /* Free temporary buffer */
    FreeTextBuffer(tempBuffer, app->cleanupStack);
    freeVec(tempBuffer);
    
    /* Free selected file path if we allocated it */
    if (selectedFile && app->cleanupStack) {
        freeVec(selectedFile);
    }
    
    /* Restore cursor position (it may have moved during insertion) */
    session->buffer->cursorX = savedCursorX;
    session->buffer->cursorY = savedCursorY;
    
    /* Update display */
    CalculateMaxScroll(session->buffer, session->window);
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    session->docState.modified = session->buffer->modified;
    
    Printf("[CMD] TTX_Cmd_InsertFile: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_SaveFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    if (!session->docState.fileName) {
        /* No filename - use Save As instead */
        return TTX_Cmd_SaveFileAs(app, session, args, argCount);
    }
    
    if (SaveFile(session->docState.fileName, session->buffer, app->cleanupStack)) {
        session->docState.modified = FALSE;
        session->buffer->modified = FALSE;
        Printf("[CMD] TTX_Cmd_SaveFile: SUCCESS\n");
        return TRUE;
    } else {
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
    
    if (!app || !session || !session->buffer) {
        Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (app=%lx, session=%lx, buffer=%lx)\n", 
               (ULONG)app, (ULONG)session, session ? (ULONG)session->buffer : 0);
        return FALSE;
    }
    
    /* Check if filename provided in args */
    if (args && argCount > 0 && args[0]) {
        fileName = args[0];
    } else if (session->docState.fileName) {
        /* Use current filename as initial value */
        fileName = session->docState.fileName;
    }
    
    /* If no filename provided, show file requester */
    if (!fileName) {
        if (!AslBase) {
            Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (ASL library not available)\n");
            return FALSE;
        }
        
        /* Extract initial file and drawer from current filename if available */
        /* Note: We need to copy the strings since we can't modify the original */
        if (session->docState.fileName) {
            STRPTR currentFileName = session->docState.fileName;
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
                    initialDrawer = (STRPTR)allocVec(drawerLen + 1, MEMF_CLEAR);
                    if (initialDrawer) {
                        CopyMem(currentFileName, initialDrawer, drawerLen);
                        initialDrawer[drawerLen] = '\0';
                    }
                }
                
                /* Allocate file string */
                if (fileLen > 0) {
                    initialFile = (STRPTR)allocVec(fileLen + 1, MEMF_CLEAR);
                    if (initialFile) {
                        CopyMem(&currentFileName[lastSlash + 1], initialFile, fileLen);
                        initialFile[fileLen] = '\0';
                    }
                }
            } else {
                /* No '/' - just file name */
                if (len > 0) {
                    initialFile = (STRPTR)allocVec(len + 1, MEMF_CLEAR);
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
            if (initialFile && app->cleanupStack) {
                freeVec(initialFile);
            }
            if (initialDrawer && app->cleanupStack) {
                freeVec(initialDrawer);
            }
            return FALSE;
        }
        fileName = selectedFile;
    }
    
    /* Save old filename to free later if we allocated a new one */
    oldFileName = session->docState.fileName;
    
    /* Allocate and copy new filename */
    if (fileName) {
        ULONG len = 0;
        while (fileName[len] != '\0') {
            len++;
        }
        if (len > 0) {
            session->docState.fileName = (STRPTR)allocVec(len + 1, MEMF_CLEAR);
            if (session->docState.fileName) {
                CopyMem(fileName, session->docState.fileName, len);
                session->docState.fileName[len] = '\0';
            } else {
                Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (allocVec fileName failed)\n");
                /* Free selected file if we allocated it */
                if (selectedFile && app->cleanupStack) {
                    freeVec(selectedFile);
                }
                return FALSE;
            }
        }
    }
    
    /* Save file */
    if (SaveFile(session->docState.fileName, session->buffer, app->cleanupStack)) {
        session->docState.modified = FALSE;
        session->buffer->modified = FALSE;
        result = TRUE;
        Printf("[CMD] TTX_Cmd_SaveFileAs: SUCCESS (saved to '%s')\n", session->docState.fileName);
        
        /* Free old filename if we replaced it */
        if (oldFileName && oldFileName != session->docState.fileName && app->cleanupStack) {
            freeVec(oldFileName);
        }
    } else {
        LONG errorCode = IoErr();
        if (errorCode != 0) {
            PrintFault(errorCode, "TTX");
            SetIoErr(0);
        }
        Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (SaveFile failed)\n");
        
        /* Restore old filename on failure */
        if (session->docState.fileName && session->docState.fileName != oldFileName && app->cleanupStack) {
            freeVec(session->docState.fileName);
        }
        session->docState.fileName = oldFileName;
        result = FALSE;
    }
    
    /* Free selected file path if we allocated it */
    if (selectedFile && app->cleanupStack) {
        freeVec(selectedFile);
    }
    
    /* Free initial file and drawer strings if we allocated them */
    if (initialFile && app->cleanupStack && initialFile != fileName) {
        freeVec(initialFile);
    }
    if (initialDrawer && app->cleanupStack) {
        freeVec(initialDrawer);
    }
    
    return result;
}

BOOL TTX_Cmd_ClearFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    ULONG i = 0;
    
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    /* Clear all lines except first empty line */
    for (i = 1; i < session->buffer->lineCount; i++) {
        if (session->buffer->lines[i].text) {
            freeVec(session->buffer->lines[i].text);
            session->buffer->lines[i].text = NULL;
        }
    }
    session->buffer->lineCount = 1;
    session->buffer->lines[0].length = 0;
    if (session->buffer->lines[0].text) {
        session->buffer->lines[0].text[0] = '\0';
    }
    session->buffer->cursorX = 0;
    session->buffer->cursorY = 0;
    session->buffer->scrollX = 0;
    session->buffer->scrollY = 0;
    session->buffer->modified = TRUE;
    session->docState.modified = TRUE;
    
    /* Force full redraw */
    if (session->buffer) {
        session->buffer->needsFullRedraw = TRUE;
    }
    
    ScrollToCursor(session->buffer, session->window);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    
    Printf("[CMD] TTX_Cmd_ClearFile: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_PrintFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement printing */
    Printf("[CMD] TTX_Cmd_PrintFile: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_CloseDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!app || !session) {
        return FALSE;
    }
    
    TTX_DestroySession(app, session);
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
        session->docState.readOnly = !session->docState.readOnly;
    } else {
        session->docState.readOnly = TRUE;
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
                if (session->docState.readOnly) {
                    item->Flags |= CHECKED;
                } else {
                    item->Flags &= ~CHECKED;
                }
                /* Note: Flag changes are automatically reflected when menu is next displayed */
                /* No need to call OnMenu() - that's for enabling/disabling menu items, not refreshing */
            }
        }
    }
    
    Printf("[CMD] TTX_Cmd_SetReadOnly: SUCCESS (readOnly=%s)\n", session->docState.readOnly ? "TRUE" : "FALSE");
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
    
    if (!app) {
        return FALSE;
    }
    
    Printf("[CMD] TTX_Cmd_Quit: START (sessionCount=%lu)\n", app->sessionCount);
    
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
    Printf("[CMD] TTX_Cmd_GetReadOnly: readOnly=%s\n", session->docState.readOnly ? "TRUE" : "FALSE");
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
        CloseWindow(session->window);
        session->window = NULL;
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
    LONG leftEdge = 0;
    LONG topEdge = 0;
    
    if (!session || !session->window) {
        return FALSE;
    }
    
    /* Parse position from args */
    if (args && argCount >= 2) {
        /* TODO: Parse numeric args */
        Printf("[CMD] TTX_Cmd_MoveWindow: numeric parsing not yet implemented\n");
        return FALSE;
    }
    
    /* Use current position for now */
    MoveWindow(session->window, session->windowState.leftEdge, session->windowState.topEdge);
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
    
    if (!session || !session->buffer) {
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
        session->buffer->scrollY += (ULONG)deltaY;
        if (session->buffer->scrollY > session->buffer->maxScrollY) {
            session->buffer->scrollY = session->buffer->maxScrollY;
        }
    } else if (deltaY < 0) {
        if ((ULONG)(-deltaY) > session->buffer->scrollY) {
            session->buffer->scrollY = 0;
        } else {
            session->buffer->scrollY -= (ULONG)(-deltaY);
        }
    }
    
    if (deltaX > 0) {
        session->buffer->scrollX += (ULONG)deltaX;
        if (session->buffer->scrollX > session->buffer->maxScrollX) {
            session->buffer->scrollX = session->buffer->maxScrollX;
        }
    } else if (deltaX < 0) {
        if ((ULONG)(-deltaX) > session->buffer->scrollX) {
            session->buffer->scrollX = 0;
        } else {
            session->buffer->scrollX -= (ULONG)(-deltaX);
        }
    }
    
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
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
    if (session->buffer) {
        CalculateMaxScroll(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
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
    if (!session || !session->window || !session->buffer) {
        return FALSE;
    }
    
    /* Force full redraw */
    session->buffer->needsFullRedraw = TRUE;
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    Printf("[CMD] TTX_Cmd_UpdateView: SUCCESS\n");
    return TRUE;
}

/* ============================================================================
 * Selection Block Commands (stubs)
 * ============================================================================ */

BOOL TTX_Cmd_CopyBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    STRPTR blockText = NULL;
    
    if (!session || !session->buffer || !session->buffer->marking.enabled) {
        Printf("[CMD] TTX_Cmd_CopyBlk: FAIL (no selection)\n");
        return FALSE;
    }
    
    /* Get selected text */
    blockText = GetBlock(session->buffer, session->cleanupStack);
    if (!blockText) {
        Printf("[CMD] TTX_Cmd_CopyBlk: FAIL (GetBlock failed)\n");
        return FALSE;
    }
    
    /* TODO: Copy to clipboard - for now just print */
    Printf("[CMD] TTX_Cmd_CopyBlk: SUCCESS (text='%s')\n", blockText);
    
    /* Free block text */
    freeVec(blockText);
    
    return TRUE;
}

BOOL TTX_Cmd_CutBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    STRPTR blockText = NULL;
    
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (!session->buffer->marking.enabled) {
        Printf("[CMD] TTX_Cmd_CutBlk: FAIL (no selection)\n");
        return FALSE;
    }
    
    /* Get selected text */
    blockText = GetBlock(session->buffer, session->cleanupStack);
    if (!blockText) {
        Printf("[CMD] TTX_Cmd_CutBlk: FAIL (GetBlock failed)\n");
        return FALSE;
    }
    
    /* TODO: Copy to clipboard - for now just print */
    Printf("[CMD] TTX_Cmd_CutBlk: SUCCESS (text='%s')\n", blockText);
    
    /* Delete the block */
    if (!DeleteBlock(session->buffer, session->cleanupStack)) {
        freeVec(blockText);
        Printf("[CMD] TTX_Cmd_CutBlk: FAIL (DeleteBlock failed)\n");
        return FALSE;
    }
    
    /* Free block text */
    freeVec(blockText);
    
    /* Update display */
    CalculateMaxScroll(session->buffer, session->window);
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    session->docState.modified = session->buffer->modified;
    
    Printf("[CMD] TTX_Cmd_CutBlk: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_DeleteBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (!session->buffer->marking.enabled) {
        Printf("[CMD] TTX_Cmd_DeleteBlk: FAIL (no selection)\n");
        return FALSE;
    }
    
    /* Delete the block */
    if (!DeleteBlock(session->buffer, session->cleanupStack)) {
        Printf("[CMD] TTX_Cmd_DeleteBlk: FAIL (DeleteBlock failed)\n");
        return FALSE;
    }
    
    /* Update display */
    CalculateMaxScroll(session->buffer, session->window);
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    session->docState.modified = session->buffer->modified;
    
    Printf("[CMD] TTX_Cmd_DeleteBlk: SUCCESS\n");
    return TRUE;
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
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    /* Mark all text */
    MarkAllBlock(session->buffer);
    
    /* Update display to show selection */
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    
    Printf("[CMD] TTX_Cmd_MarkBlk: SUCCESS\n");
    return TRUE;
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
    
    if (!session || !session->buffer) {
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
    
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    /* Parse count from args */
    if (args && argCount > 0) {
        /* TODO: Parse numeric - for now use 1 */
        count = 1;
    }
    
    /* Move cursor down by count lines */
    for (i = 0; i < (ULONG)count && session->buffer->cursorY < session->buffer->lineCount - 1; i++) {
        session->buffer->cursorY++;
        if (session->buffer->cursorX > session->buffer->lines[session->buffer->cursorY].length) {
            session->buffer->cursorX = session->buffer->lines[session->buffer->cursorY].length;
        }
    }
    
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    Printf("[CMD] TTX_Cmd_MoveDown: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveDownScr(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    ULONG pageH = 0;
    
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    pageH = session->buffer->pageH;
    if (pageH == 0) {
        pageH = 20;  /* Default if not calculated */
    }
    
    /* Move cursor down by screen height */
    session->buffer->cursorY += pageH;
    if (session->buffer->cursorY >= session->buffer->lineCount) {
        session->buffer->cursorY = session->buffer->lineCount - 1;
    }
    
    if (session->buffer->cursorX > session->buffer->lines[session->buffer->cursorY].length) {
        session->buffer->cursorX = session->buffer->lines[session->buffer->cursorY].length;
    }
    
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    Printf("[CMD] TTX_Cmd_MoveDownScr: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveEOF(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    /* Move to end of file */
    if (session->buffer->lineCount > 0) {
        session->buffer->cursorY = session->buffer->lineCount - 1;
        session->buffer->cursorX = session->buffer->lines[session->buffer->cursorY].length;
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        Printf("[CMD] TTX_Cmd_MoveEOF: SUCCESS\n");
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_MoveEOL(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    /* Move to end of line */
    if (!MoveEndOfLine(session->buffer)) {
        return FALSE;
    }
    
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    Printf("[CMD] TTX_Cmd_MoveEOL: SUCCESS\n");
    return TRUE;
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
    
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    /* Parse count from args */
    if (args && argCount > 0) {
        /* TODO: Parse numeric - for now use 1 */
        count = 1;
    }
    
    /* Move cursor left by count characters */
    for (i = 0; i < (ULONG)count; i++) {
        if (session->buffer->cursorX > 0) {
            session->buffer->cursorX--;
        } else if (session->buffer->cursorY > 0) {
            session->buffer->cursorY--;
            session->buffer->cursorX = session->buffer->lines[session->buffer->cursorY].length;
        } else {
            break;
        }
    }
    
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
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
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    if (!MoveNextWord(session->buffer)) {
        return FALSE;
    }
    
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    
    Printf("[CMD] TTX_Cmd_MoveNextWord: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MovePrevTabStop(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Move to previous tab stop */
    Printf("[CMD] TTX_Cmd_MovePrevTabStop: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_MovePrevWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    if (!MovePrevWord(session->buffer)) {
        return FALSE;
    }
    
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    
    Printf("[CMD] TTX_Cmd_MovePrevWord: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveRight(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    LONG count = 1;
    ULONG i = 0;
    
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    /* Parse count from args */
    if (args && argCount > 0) {
        /* TODO: Parse numeric - for now use 1 */
        count = 1;
    }
    
    /* Move cursor right by count characters */
    for (i = 0; i < (ULONG)count; i++) {
        if (session->buffer->cursorY < session->buffer->lineCount) {
            if (session->buffer->cursorX < session->buffer->lines[session->buffer->cursorY].length) {
                session->buffer->cursorX++;
            } else if (session->buffer->cursorY < session->buffer->lineCount - 1) {
                session->buffer->cursorY++;
                session->buffer->cursorX = 0;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    Printf("[CMD] TTX_Cmd_MoveRight: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveSOF(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    /* Move to start of file */
    session->buffer->cursorY = 0;
    session->buffer->cursorX = 0;
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    Printf("[CMD] TTX_Cmd_MoveSOF: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveSOL(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    /* Move to start of line */
    if (!MoveStartOfLine(session->buffer)) {
        return FALSE;
    }
    
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    Printf("[CMD] TTX_Cmd_MoveSOL: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveUp(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    LONG count = 1;
    ULONG i = 0;
    
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    /* Parse count from args */
    if (args && argCount > 0) {
        /* TODO: Parse numeric - for now use 1 */
        count = 1;
    }
    
    /* Move cursor up by count lines */
    for (i = 0; i < (ULONG)count && session->buffer->cursorY > 0; i++) {
        session->buffer->cursorY--;
        if (session->buffer->cursorX > session->buffer->lines[session->buffer->cursorY].length) {
            session->buffer->cursorX = session->buffer->lines[session->buffer->cursorY].length;
        }
    }
    
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
    Printf("[CMD] TTX_Cmd_MoveUp: SUCCESS\n");
    return TRUE;
}

BOOL TTX_Cmd_MoveUpScr(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    ULONG pageH = 0;
    
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    pageH = session->buffer->pageH;
    if (pageH == 0) {
        pageH = 20;  /* Default if not calculated */
    }
    
    /* Move cursor up by screen height */
    if (session->buffer->cursorY >= pageH) {
        session->buffer->cursorY -= pageH;
    } else {
        session->buffer->cursorY = 0;
    }
    
    if (session->buffer->cursorX > session->buffer->lines[session->buffer->cursorY].length) {
        session->buffer->cursorX = session->buffer->lines[session->buffer->cursorY].length;
    }
    
    ScrollToCursor(session->buffer, session->window);
    UpdateScrollBars(session);
    RenderText(session->window, session->buffer);
    UpdateCursor(session->window, session->buffer);
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
    /* Delete character at cursor (backspace) */
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (DeleteChar(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
        Printf("[CMD] TTX_Cmd_Delete: SUCCESS\n");
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_DeleteEOL(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (DeleteEOL(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
        Printf("[CMD] TTX_Cmd_DeleteEOL: SUCCESS\n");
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_DeleteEOW(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (DeleteEOW(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
        Printf("[CMD] TTX_Cmd_DeleteEOW: SUCCESS\n");
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_DeleteLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (DeleteLine(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
        Printf("[CMD] TTX_Cmd_DeleteLine: SUCCESS\n");
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_DeleteSOL(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (DeleteSOL(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
        Printf("[CMD] TTX_Cmd_DeleteSOL: SUCCESS\n");
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_DeleteSOW(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (DeleteSOW(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
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
    
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    ch = GetCharAtCursor(session->buffer);
    Printf("[CMD] TTX_Cmd_GetChar: character='%c' (0x%02x)\n", (ch >= 32 && ch < 127) ? ch : '?', (unsigned int)ch);
    /* TODO: Return to ARexx via RESULT */
    return TRUE;
}

BOOL TTX_Cmd_GetLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    STRPTR lineText = NULL;
    
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    lineText = GetCurrentLine(session->buffer, session->cleanupStack);
    if (lineText) {
        Printf("[CMD] TTX_Cmd_GetLine: line='%s'\n", lineText);
        /* TODO: Return to ARexx via RESULT */
        freeVec(lineText);
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_Insert(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (args && argCount > 0 && args[0]) {
        if (InsertText(session->buffer, args[0], session->cleanupStack)) {
            CalculateMaxScroll(session->buffer, session->window);
            ScrollToCursor(session->buffer, session->window);
            UpdateScrollBars(session);
            RenderText(session->window, session->buffer);
            UpdateCursor(session->window, session->buffer);
            session->docState.modified = session->buffer->modified;
            Printf("[CMD] TTX_Cmd_Insert: SUCCESS\n");
            return TRUE;
        }
    }
    
    return FALSE;
}

BOOL TTX_Cmd_InsertLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    /* Insert newline at cursor */
    if (InsertNewline(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
        Printf("[CMD] TTX_Cmd_InsertLine: SUCCESS\n");
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_SetChar(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    UBYTE ch = 0;
    
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (args && argCount > 0 && args[0] && args[0][0] != '\0') {
        ch = (UBYTE)args[0][0];
        if (SetCharAtCursor(session->buffer, ch, session->cleanupStack)) {
            CalculateMaxScroll(session->buffer, session->window);
            ScrollToCursor(session->buffer, session->window);
            UpdateScrollBars(session);
            RenderText(session->window, session->buffer);
            UpdateCursor(session->window, session->buffer);
            session->docState.modified = session->buffer->modified;
            Printf("[CMD] TTX_Cmd_SetChar: SUCCESS\n");
            return TRUE;
        }
    }
    
    return FALSE;
}

BOOL TTX_Cmd_SwapChars(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (SwapChars(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
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
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (ToggleCharCase(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
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
    STRPTR word = NULL;
    
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    word = GetWordAtCursor(session->buffer, session->cleanupStack);
    if (word) {
        Printf("[CMD] TTX_Cmd_GetWord: word='%s'\n", word);
        /* TODO: Return to ARexx via RESULT */
        freeVec(word);
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_ReplaceWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (args && argCount > 0 && args[0]) {
        if (ReplaceWordAtCursor(session->buffer, args[0], session->cleanupStack)) {
            CalculateMaxScroll(session->buffer, session->window);
            ScrollToCursor(session->buffer, session->window);
            UpdateScrollBars(session);
            RenderText(session->window, session->buffer);
            UpdateCursor(session->window, session->buffer);
            session->docState.modified = session->buffer->modified;
            Printf("[CMD] TTX_Cmd_ReplaceWord: SUCCESS\n");
            return TRUE;
        }
    }
    
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
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (!session->buffer->marking.enabled) {
        Printf("[CMD] TTX_Cmd_Conv2Lower: no selection\n");
        return FALSE;
    }
    
    if (ConvertToLower(session->buffer, session->cleanupStack)) {
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
        Printf("[CMD] TTX_Cmd_Conv2Lower: SUCCESS\n");
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_Conv2Spaces(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (ConvertTabsToSpaces(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
        Printf("[CMD] TTX_Cmd_Conv2Spaces: SUCCESS\n");
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_Conv2Tabs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (ConvertSpacesToTabs(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
        Printf("[CMD] TTX_Cmd_Conv2Tabs: SUCCESS\n");
        return TRUE;
    }
    
    Printf("[CMD] TTX_Cmd_Conv2Tabs: not yet fully implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_Conv2Upper(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (!session->buffer->marking.enabled) {
        Printf("[CMD] TTX_Cmd_Conv2Upper: no selection\n");
        return FALSE;
    }
    
    if (ConvertToUpper(session->buffer, session->cleanupStack)) {
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
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
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (ShiftLeft(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
        Printf("[CMD] TTX_Cmd_ShiftLeft: SUCCESS\n");
        return TRUE;
    }
    
    return FALSE;
}

BOOL TTX_Cmd_ShiftRight(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer || session->docState.readOnly) {
        return FALSE;
    }
    
    if (ShiftRight(session->buffer, session->cleanupStack)) {
        CalculateMaxScroll(session->buffer, session->window);
        ScrollToCursor(session->buffer, session->window);
        UpdateScrollBars(session);
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
        session->docState.modified = session->buffer->modified;
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
