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
    
    /* Project menu commands */
    if (Stricmp(command, "OpenFile") == 0) {
        return TTX_Cmd_OpenFile(app, session, args, argCount);
    } else if (Stricmp(command, "OpenDoc") == 0) {
        return TTX_Cmd_OpenDoc(app, session, args, argCount);
    } else if (Stricmp(command, "InsertFile") == 0) {
        return TTX_Cmd_InsertFile(app, session, args, argCount);
    } else if (Stricmp(command, "SaveFile") == 0) {
        return TTX_Cmd_SaveFile(app, session, args, argCount);
    } else if (Stricmp(command, "SaveFileAs") == 0) {
        return TTX_Cmd_SaveFileAs(app, session, args, argCount);
    } else if (Stricmp(command, "ClearFile") == 0) {
        return TTX_Cmd_ClearFile(app, session, args, argCount);
    } else if (Stricmp(command, "PrintFile") == 0) {
        return TTX_Cmd_PrintFile(app, session, args, argCount);
    } else if (Stricmp(command, "CloseDoc") == 0) {
        return TTX_Cmd_CloseDoc(app, session, args, argCount);
    } else if (Stricmp(command, "SetReadOnly") == 0) {
        return TTX_Cmd_SetReadOnly(app, session, args, argCount);
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
    /* Menu structure: 0=Title, 1=Open, 2=OpenNew, 3=Insert, 4=Bar, 5=Save, 6=SaveAs, 7=Bar, 8=Clear, 9=Print, 10=Info, 11=Bar, 12=ReadOnly, 13=Bar, 14=Close, 15=Quit */
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
            case 12: /* Bar - skip */
                Printf("[MENU] Bar item selected (ignored)\n");
                return TRUE;
            case 13: /* Close Window */
                command = "CloseDoc";
                break;
            case 14: /* Quit */
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
        if (!PushResource(session->cleanupStack, RESOURCE_TYPE_MEMORY, menuStrip, cleanupMenuStrip)) {
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
            UntrackResource(session->cleanupStack, session->menuStrip);
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
    if (!PushResource(app->cleanupStack, RESOURCE_TYPE_MEMORY, fileReq, cleanupFileRequester)) {
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
            fullPath = (STRPTR)allocVec(app->cleanupStack, fullPathLen, MEMF_CLEAR);
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
                    freeVec(app->cleanupStack, fullPath);
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
        UntrackResource(app->cleanupStack, fileReq);
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
    if (!PushResource(app->cleanupStack, RESOURCE_TYPE_MEMORY, fileReq, cleanupFileRequester)) {
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
            fullPath = (STRPTR)allocVec(app->cleanupStack, fullPathLen, MEMF_CLEAR);
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
                    freeVec(app->cleanupStack, fullPath);
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
        UntrackResource(app->cleanupStack, fileReq);
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
    oldFileName = session->fileName;
    
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
        
        newFileName = allocVec(app->cleanupStack, fileNameLen, MEMF_CLEAR);
        if (newFileName) {
            endPtr = Strncpy(newFileName, fileName, fileNameLen);
            if (!endPtr) {
                /* String was truncated - this shouldn't happen since we allocated the right size */
                Printf("[CMD] TTX_Cmd_OpenFile: WARN (filename truncated)\n");
            }
            session->fileName = newFileName;
        } else {
            Printf("[CMD] TTX_Cmd_OpenFile: FAIL (could not allocate filename)\n");
            if (selectedFile && app->cleanupStack) {
                freeVec(app->cleanupStack, selectedFile);
            }
            return FALSE;
        }
    }
    
    /* Clear existing buffer and load new file */
    FreeTextBuffer(session->buffer, app->cleanupStack);
    if (!InitTextBuffer(session->buffer, app->cleanupStack)) {
        Printf("[CMD] TTX_Cmd_OpenFile: FAIL (InitTextBuffer failed)\n");
        if (selectedFile && app->cleanupStack) {
            freeVec(app->cleanupStack, selectedFile);
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
        freeVec(app->cleanupStack, oldFileName);
    }
    
    /* Free selected file path if we allocated it (the filename is now in session->fileName) */
    if (selectedFile && app->cleanupStack) {
        freeVec(app->cleanupStack, selectedFile);
    }
    
    /* Update window title */
    if (session->window && session->fileName) {
        STRPTR titleText = NULL;
        ULONG titleLen = 0;
        ULONG fileNameLen = 0;
        STRPTR endPtr = NULL;
        STRPTR tempPtr = NULL;
        
        /* Calculate filename length (utility.library V39 doesn't have Strlen) */
        tempPtr = session->fileName;
        while (tempPtr && *tempPtr != '\0') {
            fileNameLen++;
            tempPtr++;
        }
        
        titleLen = fileNameLen + 10; /* "TTX - " + filename + null */
        titleText = allocVec(app->cleanupStack, titleLen, MEMF_CLEAR);
        if (titleText) {
            /* Use Strncpy chaining to concatenate strings */
            endPtr = Strncpy(titleText, "TTX - ", titleLen);
            if (endPtr) {
                Strncpy(endPtr, session->fileName, titleLen - (ULONG)(endPtr - titleText));
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
            freeVec(app->cleanupStack, selectedFile);
        }
        
        return result;
    } else {
        /* Open new document */
        return TTX_CreateSession(app, NULL);
    }
}

BOOL TTX_Cmd_InsertFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement file insertion */
    Printf("[CMD] TTX_Cmd_InsertFile: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_SaveFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    if (!session || !session->buffer) {
        return FALSE;
    }
    
    if (!session->fileName) {
        /* No filename - use Save As instead */
        return TTX_Cmd_SaveFileAs(app, session, args, argCount);
    }
    
    if (SaveFile(session->fileName, session->buffer, app->cleanupStack)) {
        session->modified = FALSE;
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
    } else if (session->fileName) {
        /* Use current filename as initial value */
        fileName = session->fileName;
    }
    
    /* If no filename provided, show file requester */
    if (!fileName) {
        if (!AslBase) {
            Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (ASL library not available)\n");
            return FALSE;
        }
        
        /* Extract initial file and drawer from current filename if available */
        /* Note: We need to copy the strings since we can't modify the original */
        if (session->fileName) {
            STRPTR currentFileName = session->fileName;
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
                    initialDrawer = (STRPTR)allocVec(app->cleanupStack, drawerLen + 1, MEMF_CLEAR);
                    if (initialDrawer) {
                        CopyMem(currentFileName, initialDrawer, drawerLen);
                        initialDrawer[drawerLen] = '\0';
                    }
                }
                
                /* Allocate file string */
                if (fileLen > 0) {
                    initialFile = (STRPTR)allocVec(app->cleanupStack, fileLen + 1, MEMF_CLEAR);
                    if (initialFile) {
                        CopyMem(&currentFileName[lastSlash + 1], initialFile, fileLen);
                        initialFile[fileLen] = '\0';
                    }
                }
            } else {
                /* No '/' - just file name */
                if (len > 0) {
                    initialFile = (STRPTR)allocVec(app->cleanupStack, len + 1, MEMF_CLEAR);
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
                freeVec(app->cleanupStack, initialFile);
            }
            if (initialDrawer && app->cleanupStack) {
                freeVec(app->cleanupStack, initialDrawer);
            }
            return FALSE;
        }
        fileName = selectedFile;
    }
    
    /* Save old filename to free later if we allocated a new one */
    oldFileName = session->fileName;
    
    /* Allocate and copy new filename */
    if (fileName) {
        ULONG len = 0;
        while (fileName[len] != '\0') {
            len++;
        }
        if (len > 0) {
            session->fileName = (STRPTR)allocVec(app->cleanupStack, len + 1, MEMF_CLEAR);
            if (session->fileName) {
                CopyMem(fileName, session->fileName, len);
                session->fileName[len] = '\0';
            } else {
                Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (allocVec fileName failed)\n");
                /* Free selected file if we allocated it */
                if (selectedFile && app->cleanupStack) {
                    freeVec(app->cleanupStack, selectedFile);
                }
                return FALSE;
            }
        }
    }
    
    /* Save file */
    if (SaveFile(session->fileName, session->buffer, app->cleanupStack)) {
        session->modified = FALSE;
        session->buffer->modified = FALSE;
        result = TRUE;
        Printf("[CMD] TTX_Cmd_SaveFileAs: SUCCESS (saved to '%s')\n", session->fileName);
        
        /* Free old filename if we replaced it */
        if (oldFileName && oldFileName != session->fileName && app->cleanupStack) {
            freeVec(app->cleanupStack, oldFileName);
        }
    } else {
        LONG errorCode = IoErr();
        if (errorCode != 0) {
            PrintFault(errorCode, "TTX");
            SetIoErr(0);
        }
        Printf("[CMD] TTX_Cmd_SaveFileAs: FAIL (SaveFile failed)\n");
        
        /* Restore old filename on failure */
        if (session->fileName && session->fileName != oldFileName && app->cleanupStack) {
            freeVec(app->cleanupStack, session->fileName);
        }
        session->fileName = oldFileName;
        result = FALSE;
    }
    
    /* Free selected file path if we allocated it */
    if (selectedFile && app->cleanupStack) {
        freeVec(app->cleanupStack, selectedFile);
    }
    
    /* Free initial file and drawer strings if we allocated them */
    if (initialFile && app->cleanupStack && initialFile != fileName) {
        freeVec(app->cleanupStack, initialFile);
    }
    if (initialDrawer && app->cleanupStack) {
        freeVec(app->cleanupStack, initialDrawer);
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
            freeVec(session->cleanupStack, session->buffer->lines[i].text);
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
    session->modified = TRUE;
    
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
        session->readOnly = !session->readOnly;
    } else {
        session->readOnly = TRUE;
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
                if (session->readOnly) {
                    item->Flags |= CHECKED;
                } else {
                    item->Flags &= ~CHECKED;
                }
                /* Note: Flag changes are automatically reflected when menu is next displayed */
                /* No need to call OnMenu() - that's for enabling/disabling menu items, not refreshing */
            }
        }
    }
    
    Printf("[CMD] TTX_Cmd_SetReadOnly: SUCCESS (readOnly=%s)\n", session->readOnly ? "TRUE" : "FALSE");
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
