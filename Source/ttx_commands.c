/*
 * TTX - Command Handler Functions
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx.h"
#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <proto/gadtools.h>
#include <libraries/gadtools.h>
#include <intuition/intuition.h>
#include "seiso.h"

/* Cleanup function for menu strip (called by cleanup stack) - forward declaration */
VOID cleanupMenuStrip(APTR resource);

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
    
    Printf("[MENU] TTX_HandleMenuPick: menu=%lu, item=%lu\n", menuNumber, itemNumber);
    
    /* Check for MENUNULL (no selection) - menuNumber and itemNumber are both 0xFFFF */
    if (menuNumber == 0xFFFF && itemNumber == 0xFFFF) {
        return TRUE;  /* MENUNULL - no action needed */
    }
    
    /* Menu 0 = Project menu */
    if (menuNumber == 0) {
        switch (itemNumber) {
            case 0:  /* Open... */
                command = "OpenFile";
                break;
            case 1:  /* Open New... */
                command = "OpenDoc";
                if (argCount < 10) {
                    args[argCount++] = "FileReq";
                }
                break;
            case 2:  /* Insert... */
                command = "InsertFile";
                break;
            case 4:  /* Save */
                command = "SaveFile";
                break;
            case 5:  /* Save As... */
                command = "SaveFileAs";
                break;
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
            case 11: /* Read-Only */
                command = "SetReadOnly";
                if (argCount < 10) {
                    args[argCount++] = "Toggle";
                }
                break;
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
    struct NewMenu newMenu[] = {
        /* Project menu */
        {NM_TITLE, "Project", NULL, 0, 0, NULL},
        {NM_ITEM, "Open...", "O", 0, 0, NULL},
        {NM_ITEM, "Open New...", "Y", 0, 0, NULL},
        {NM_ITEM, "Insert...", NULL, 0, 0, NULL},
        {NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, "Save", "S", 0, 0, NULL},
        {NM_ITEM, "Save As...", "A", 0, 0, NULL},
        {NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, "Clear", "K", 0, 0, NULL},
        {NM_ITEM, "Print...", "P", 0, 0, NULL},
        {NM_ITEM, "Info...", "?", 0, 0, NULL},
        {NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, "Read-Only", NULL, 0, CHECKIT, NULL},
        {NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL},
        {NM_ITEM, "Close Window", "Q", 0, 0, NULL},
        {NM_ITEM, "Quit", NULL, 0, 0, NULL},
        
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

/* Project menu command handlers */

BOOL TTX_Cmd_OpenFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    /* TODO: Implement file requester */
    Printf("[CMD] TTX_Cmd_OpenFile: not yet implemented\n");
    return FALSE;
}

BOOL TTX_Cmd_OpenDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount)
{
    BOOL useFileReq = FALSE;
    
    if (args && argCount > 0 && Stricmp(args[0], "FileReq") == 0) {
        useFileReq = TRUE;
    }
    
    if (useFileReq) {
        /* TODO: Open with file requester */
        Printf("[CMD] TTX_Cmd_OpenDoc: FileReq not yet implemented\n");
        return FALSE;
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
    /* TODO: Implement file requester for Save As */
    Printf("[CMD] TTX_Cmd_SaveFileAs: not yet implemented\n");
    return FALSE;
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
