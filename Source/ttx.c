/*
 * TTX - Text Editor for AmigaOS
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx.h"

static const char *verstag = "$VER: TTX 3.0 (12/1/2026)\n";
static const char *stack_cookie = "$STACK: 4096\n";

/* Forward declaration for cleanup stack access */
static struct CleanupStack *g_ttxStack = NULL;

/* Initialize required libraries */
BOOL TTX_InitLibraries(struct CleanupStack *stack)
{
    Printf("[INIT] TTX_InitLibraries: START\n");
    if (!stack) {
        Printf("[INIT] TTX_InitLibraries: FAIL (stack=NULL)\n");
        return FALSE;
    }
    
    IntuitionBase = (struct IntuitionBase *)openLibrary("intuition.library", 39L);
    if (!IntuitionBase) {
        Printf("[INIT] TTX_InitLibraries: FAIL (intuition.library)\n");
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }
    Printf("[INIT] TTX_InitLibraries: intuition.library=%lx\n", (ULONG)IntuitionBase);
    
    UtilityBase = openLibrary("utility.library", 39L);
    if (!UtilityBase) {
        Printf("[INIT] TTX_InitLibraries: FAIL (utility.library)\n");
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }
    Printf("[INIT] TTX_InitLibraries: utility.library=%lx\n", (ULONG)UtilityBase);
    
    GfxBase = (struct GfxBase *)openLibrary("graphics.library", 39L);
    if (!GfxBase) {
        Printf("[INIT] TTX_InitLibraries: FAIL (graphics.library)\n");
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }
    Printf("[INIT] TTX_InitLibraries: graphics.library=%lx\n", (ULONG)GfxBase);
    
    IconBase = openLibrary("icon.library", 39L);
    if (!IconBase) {
        Printf("[INIT] TTX_InitLibraries: FAIL (icon.library)\n");
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }
    Printf("[INIT] TTX_InitLibraries: icon.library=%lx\n", (ULONG)IconBase);
    
    WorkbenchBase = openLibrary("workbench.library", 36L);
    if (!WorkbenchBase) {
        /* Workbench library is optional - app icon support won't work without it */
        Printf("[INIT] TTX_InitLibraries: WARN (workbench.library optional, not found)\n");
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
    } else {
        Printf("[INIT] TTX_InitLibraries: workbench.library=%lx\n", (ULONG)WorkbenchBase);
    }
    
    CxBase = openLibrary("commodities.library", 0L);
    if (!CxBase) {
        /* Commodities is optional for single-instance, but preferred */
        Printf("[INIT] TTX_InitLibraries: WARN (commodities.library optional, not found)\n");
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
    } else {
        Printf("[INIT] TTX_InitLibraries: commodities.library=%lx\n", (ULONG)CxBase);
    }
    
    /* Open keymap.library for MapRawKey */
    KeymapBase = openLibrary("keymap.library", 0L);
    if (!KeymapBase) {
        /* Keymap.library is required for keyboard input conversion */
        Printf("[INIT] TTX_InitLibraries: FAIL (keymap.library)\n");
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }
    Printf("[INIT] TTX_InitLibraries: keymap.library=%lx\n", (ULONG)KeymapBase);
    
    /* Open asl.library for file requesters */
    AslBase = openLibrary("asl.library", 36L);
    if (!AslBase) {
        /* ASL library is optional - file requesters won't work without it */
        Printf("[INIT] TTX_InitLibraries: WARN (asl.library optional, not found)\n");
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
    } else {
        Printf("[INIT] TTX_InitLibraries: asl.library=%lx\n", (ULONG)AslBase);
    }
    
    Printf("[INIT] TTX_InitLibraries: SUCCESS\n");
    return TRUE;
}

/* Cleanup libraries - now handled automatically by Seiso cleanup stack */
VOID TTX_CleanupLibraries(VOID)
{
    /* Libraries are automatically closed by Seiso cleanup stack */
    /* This function is kept for compatibility but does nothing */
    Printf("[CLEANUP] TTX_CleanupLibraries: (handled by Seiso cleanup stack)\n");
}

/* TurboText-style command line template */
static const char *ttxArgTemplate = "FILES/M,STARTUP/K,WINDOW/K,PUBSCREEN/K,SETTINGS/K,DEFINITIONS/K,NOWINDOW/S,WAIT/S,BACKGROUND/S,UNLOAD/S";

/* Parse command-line arguments - TurboText style */
BOOL TTX_ParseArguments(struct TTXArgs *args, struct CleanupStack *stack)
{
    LONG argArray[10];
    ULONG i = 0;
    BOOL result = FALSE;
    
    if (!args || !stack) {
        return FALSE;
    }
    
    /* Clear args structure */
    for (i = 0; i < sizeof(struct TTXArgs); i++) {
        ((UBYTE *)args)[i] = 0;
    }
    
    /* ReadArgs resources will be tracked on the provided cleanup stack */
    
    /* Initialize arg array */
    for (i = 0; i < 10; i++) {
        argArray[i] = 0;
    }
    
    /* ReadArgs allocates memory that must be freed with FreeArgs */
    /* Track it with cleanup stack */
    /* Cast const char * to STRPTR for ReadArgs compatibility */
    /* Clear IoErr() before ReadArgs to ensure clean state */
    SetIoErr(0);
    args->rda = readArgs((STRPTR)ttxArgTemplate, argArray, NULL);
    if (args->rda) {
        /* ReadArgs succeeded - extract arguments */
        /* CRITICAL: Copy file names before freeing RDArgs - they point to memory owned by RDArgs */
        /* ReadArgs allocates memory for /M (multiple) parameters that gets freed with FreeArgs() */
        if (argArray[0]) {
            STRPTR *readArgsFiles = (STRPTR *)argArray[0];
            ULONG fileCount = 0;
            ULONG i = 0;
            STRPTR *copiedFiles = NULL;
            
            /* Count files */
            while (readArgsFiles[fileCount]) {
                fileCount++;
            }
            
            /* Allocate array for copied file pointers */
            if (fileCount > 0) {
                copiedFiles = (STRPTR *)allocVec((fileCount + 1) * sizeof(STRPTR), MEMF_CLEAR);
                if (copiedFiles) {
                    /* Copy each file name string */
                    for (i = 0; i < fileCount; i++) {
                        ULONG len = 0;
                        STRPTR copy = NULL;
                        
                        /* Calculate length */
                        while (readArgsFiles[i][len] != '\0') {
                            len++;
                        }
                        
                        /* Allocate and copy string */
                        if (len > 0) {
                            copy = (STRPTR)allocVec(len + 1, MEMF_CLEAR);
                            if (copy) {
                                CopyMem(readArgsFiles[i], copy, len);
                                copy[len] = '\0';
                                copiedFiles[i] = copy;
                            } else {
                                /* Allocation failed - free what we've allocated so far */
                                for (i = 0; i < fileCount && copiedFiles[i]; i++) {
                                    freeVec(copiedFiles[i]);
                                }
                                freeVec(copiedFiles);
                                copiedFiles = NULL;
                                break;
                            }
                        } else {
                            copiedFiles[i] = NULL;
                        }
                    }
                    if (copiedFiles) {
                        copiedFiles[fileCount] = NULL;
                        args->files = copiedFiles;
                    }
                }
            }
        } else {
            args->files = NULL;
        }
        
        /* Copy other string arguments (they also point to RDArgs memory) */
        if (argArray[1]) {
            ULONG len = 0;
            STRPTR copy = NULL;
            while (((STRPTR)argArray[1])[len] != '\0') {
                len++;
            }
            if (len > 0) {
                copy = (STRPTR)allocVec(len + 1, MEMF_CLEAR);
                if (copy) {
                    CopyMem((STRPTR)argArray[1], copy, len);
                    copy[len] = '\0';
                    args->startup = copy;
                }
            }
        } else {
            args->startup = NULL;
        }
        
        /* Copy remaining string arguments similarly */
        if (argArray[2]) {
            ULONG len = 0;
            STRPTR copy = NULL;
            while (((STRPTR)argArray[2])[len] != '\0') {
                len++;
            }
            if (len > 0) {
                copy = (STRPTR)allocVec(len + 1, MEMF_CLEAR);
                if (copy) {
                    CopyMem((STRPTR)argArray[2], copy, len);
                    copy[len] = '\0';
                    args->window = copy;
                }
            }
        } else {
            args->window = NULL;
        }
        
        if (argArray[3]) {
            ULONG len = 0;
            STRPTR copy = NULL;
            while (((STRPTR)argArray[3])[len] != '\0') {
                len++;
            }
            if (len > 0) {
                copy = (STRPTR)allocVec(len + 1, MEMF_CLEAR);
                if (copy) {
                    CopyMem((STRPTR)argArray[3], copy, len);
                    copy[len] = '\0';
                    args->pubscreen = copy;
                }
            }
        } else {
            args->pubscreen = NULL;
        }
        
        if (argArray[4]) {
            ULONG len = 0;
            STRPTR copy = NULL;
            while (((STRPTR)argArray[4])[len] != '\0') {
                len++;
            }
            if (len > 0) {
                copy = (STRPTR)allocVec(len + 1, MEMF_CLEAR);
                if (copy) {
                    CopyMem((STRPTR)argArray[4], copy, len);
                    copy[len] = '\0';
                    args->settings = copy;
                }
            }
        } else {
            args->settings = NULL;
        }
        
        if (argArray[5]) {
            ULONG len = 0;
            STRPTR copy = NULL;
            while (((STRPTR)argArray[5])[len] != '\0') {
                len++;
            }
            if (len > 0) {
                copy = (STRPTR)allocVec(len + 1, MEMF_CLEAR);
                if (copy) {
                    CopyMem((STRPTR)argArray[5], copy, len);
                    copy[len] = '\0';
                    args->definitions = copy;
                }
            }
        } else {
            args->definitions = NULL;
        }
        
        /* Boolean arguments are just values, not pointers */
        args->noWindow = (BOOL)argArray[6];
        args->wait = (BOOL)argArray[7];
        args->background = (BOOL)argArray[8];
        args->unload = (BOOL)argArray[9];
        
        /* Now safe to free RDArgs - we've copied all the strings */
        if (args->rda && stack) {
            freeArgs(args->rda);
            args->rda = NULL;
        }
        
        /* Clear IoErr() after successful ReadArgs to prevent interference */
        /* ReadArgs may set error codes even on success in some cases */
        SetIoErr(0);
        result = TRUE;
    } else {
        /* ReadArgs failed - check error code */
        LONG errorCode = IoErr();
        if (errorCode != 0 && errorCode != ERROR_REQUIRED_ARG_MISSING) {
            /* Only print error if it's not "required argument missing" */
            /* ERROR_REQUIRED_ARG_MISSING is normal when no args provided */
            PrintFault(errorCode, "TTX");
        }
        /* Clear error code after handling to prevent interference with subsequent operations */
        SetIoErr(0);
        /* Ensure all args fields remain NULL/0 (already cleared at start) */
        args->rda = NULL;
        /* No arguments is OK - we'll open default window */
        result = TRUE;
    }
    
    return result;
}

/* Parse ToolTypes from icon - called when argc == 0 (Workbench launch) */
BOOL TTX_ParseToolTypes(STRPTR *fileName, struct WBStartup *wbMsg, struct CleanupStack *stack)
{
    struct DiskObject *icon = NULL;
    STRPTR *toolTypes = NULL;
    STRPTR fileArg = NULL;
    ULONG i = 0;
    BOOL result = FALSE;
    
    if (!fileName || !wbMsg || !stack) {
        return FALSE;
    }
    
    *fileName = NULL;
    
    /* Get icon for this program using cleanup stack */
    /* Clear IoErr() before GetDiskObject to ensure clean state */
    SetIoErr(0);
    icon = getDiskObject(wbMsg->sm_ArgList[0].wa_Name);
    if (!icon) {
        /* GetDiskObject failed - check error code and clear it */
        LONG errorCode = IoErr();
        if (errorCode != 0) {
            /* Clear error to prevent icon.library from being left in undefined state */
            SetIoErr(0);
        }
        return FALSE;
    } else {
        /* GetDiskObject succeeded - clear any error code that may have been set */
        SetIoErr(0);
    }
    
    toolTypes = icon->do_ToolTypes;
    if (toolTypes) {
        /* Look for FILE= tooltype */
        i = 0;
        while (toolTypes[i]) {
            if (toolTypes[i][0] == 'F' && 
                toolTypes[i][1] == 'I' && 
                toolTypes[i][2] == 'L' && 
                toolTypes[i][3] == 'E' && 
                toolTypes[i][4] == '=') {
                fileArg = &toolTypes[i][5];
                break;
            }
            i++;
        }
    }
    
    if (fileArg && fileArg[0] != '\0') {
        ULONG len = 0;
        STRPTR copy = NULL;
        
        /* Calculate length */
        while (fileArg[len] != '\0') {
            len++;
        }
        
        /* Allocate and copy using cleanup stack */
        copy = (STRPTR)allocVec(len + 1, MEMF_CLEAR);
        if (copy) {
            Printf("[INIT] TTX_ParseToolTypes: allocated copy=%lx\n", (ULONG)copy);
            CopyMem(fileArg, copy, len);
            copy[len] = '\0';
            *fileName = copy;
            result = TRUE;
        }
    }
    
    /* Remove DiskObject from tracking and free it explicitly */
    freeDiskObject(icon);
    
    return result;
}

/* Check if another instance is already running */
BOOL TTX_CheckExistingInstance(STRPTR fileName)
{
    struct MsgPort *existingPort = NULL;
    BOOL result = FALSE;
    
    /* Try to find existing message port */
    /* Note: FindPort() MUST be called with Forbid()/Permit() protection */
    /* We must validate the port - FindPort might return a stale pointer
     * if a previous instance's port wasn't fully cleaned up */
    Forbid();
    existingPort = FindPort(TTX_MESSAGE_PORT_NAME);
    Permit();
    /* Verify the port is valid before using it */
    /* All validation must happen inside Forbid() to prevent race conditions */
    
    if (existingPort) {
        /* Another instance is running, send message to it */
        /* Use global cleanup stack for inter-instance messages */
        if (fileName) {
            result = TTX_SendToExistingInstance(g_ttxStack, TTX_MSG_OPEN_FILE, fileName);
        } else {
            result = TTX_SendToExistingInstance(g_ttxStack, TTX_MSG_OPEN_NEW, NULL);
        }
    }
    
    return result;
}

/* Send message to existing instance */
BOOL TTX_SendToExistingInstance(struct CleanupStack *stack, ULONG msgType, STRPTR fileName)
{
    struct MsgPort *existingPort = NULL;
    struct TTXMessage *msg = NULL;
    ULONG fileNameLen = 0;
    STRPTR fileNameCopy = NULL;
    BOOL result = FALSE;
    
    if (!stack) {
        return FALSE;
    }
    
    /* Find existing message port */
    Forbid();
    existingPort = FindPort(TTX_MESSAGE_PORT_NAME);
    Permit();
    
    if (!existingPort) {
        return FALSE;
    }
    
    /* Calculate filename length */
    if (fileName) {
        while (fileName[fileNameLen] != '\0') {
            fileNameLen++;
        }
    }
    
    /* Allocate message using cleanup stack */
    /* Note: Once sent, the receiving instance will free it, so we remove it from tracking */
    msg = (struct TTXMessage *)allocVec(sizeof(struct TTXMessage), MEMF_CLEAR);
    if (!msg) {
        return FALSE;
    }
    Printf("[INIT] TTX_SendToExistingInstance: allocated msg=%lx\n", (ULONG)msg);
    
    /* Fill in message */
    msg->msg.mn_Node.ln_Type = NT_MESSAGE;
    msg->msg.mn_Length = sizeof(struct TTXMessage);
    msg->msg.mn_ReplyPort = NULL;
    msg->type = msgType;
    msg->fileName = NULL;
    msg->fileNameLen = fileNameLen;
    
    /* Allocate and copy filename if provided */
    if (fileName && fileNameLen > 0) {
        fileNameCopy = (STRPTR)allocVec(fileNameLen + 1, MEMF_CLEAR);
        if (fileNameCopy) {
            Printf("[INIT] TTX_SendToExistingInstance: allocated fileNameCopy=%lx\n", (ULONG)fileNameCopy);
            CopyMem(fileName, fileNameCopy, fileNameLen);
            fileNameCopy[fileNameLen] = '\0';
            msg->fileName = fileNameCopy;
        } else {
            Printf("[CLEANUP] TTX_SendToExistingInstance: freeing msg=%lx (error path)\n", (ULONG)msg);
            freeVec(msg);
            return FALSE;
        }
    }
    
    /* Send message - for one-way messages (mn_ReplyPort=NULL), receiver must free after ReplyMsg() */
    /* According to Exec docs: ALL messages must be replied to with ReplyMsg() */
    /* If mn_ReplyPort is NULL, ReplyMsg() does nothing, but receiver still calls it */
    /* Receiver will free the message after calling ReplyMsg() */
    PutMsg(existingPort, (struct Message *)msg);
    /* Untrack from our stack - receiver takes ownership and will free it after ReplyMsg() */
    /* Use UntrackResource to remove from tracking without calling cleanup function */
    if (msg->fileName) {
        UntrackResource(msg->fileName);
    }
    UntrackResource(msg);
    result = TRUE;
    
    return result;
}

/* Setup message port for single-instance operation */
BOOL TTX_SetupMessagePort(struct TTXApplication *app)
{
    Printf("[INIT] TTX_SetupMessagePort: START\n");
    if (!app || !app->cleanupStack) {
        Printf("[INIT] TTX_SetupMessagePort: FAIL (app=%lx, stack=%lx)\n", (ULONG)app, app ? (ULONG)app->cleanupStack : 0);
        return FALSE;
    }
    
    /* Create application message port using Seiso */
    app->appPort = createMsgPort();
    if (!app->appPort) {
        Printf("[INIT] TTX_SetupMessagePort: FAIL (createMsgPort failed)\n");
        return FALSE;
    }
    
    /* Set port name but DON'T add it yet - we'll add it after checking for existing instances */
    /* This prevents us from finding our own port when checking */
    Forbid();
    app->appPort->mp_Node.ln_Name = TTX_MESSAGE_PORT_NAME;
    Permit();
    
    Printf("[INIT] TTX_SetupMessagePort: SUCCESS (port=%lx, name=%s, not yet added)\n", (ULONG)app->appPort, TTX_MESSAGE_PORT_NAME);
    return TRUE;
}

/* Add message port to system (call this after checking for existing instances) */
BOOL TTX_AddMessagePort(struct TTXApplication *app)
{
    if (!app || !app->appPort) {
        return FALSE;
    }
    
    /* Add port to system port list so FindPort() can find it */
    /* This makes the port "public" so other instances can send messages */
    Forbid();
    AddPort(app->appPort);
    Permit();
    
    Printf("[INIT] TTX_AddMessagePort: port added to system\n");
    return TRUE;
}

/* Remove message port - now handled automatically by cleanup stack */
VOID TTX_RemoveMessagePort(struct TTXApplication *app)
{
    /* Resources are automatically cleaned up by Seiso cleanup stack */
    /* This function is kept for compatibility but does nothing */
    /* Messages are cleaned up in TTX_Cleanup before stack deletion */
    Printf("[CLEANUP] TTX_RemoveMessagePort: (handled by Seiso cleanup stack)\n");
}

/* Setup commodity for single-instance (appears in Exchange) */
BOOL TTX_SetupCommodity(struct TTXApplication *app)
{
    struct NewBroker nb;
    LONG brokerError = 0;
    BOOL result = FALSE;
    
    Printf("[INIT] TTX_SetupCommodity: START\n");
    if (!app || !CxBase) {
        Printf("[INIT] TTX_SetupCommodity: FAIL (app=%lx, CxBase=%lx)\n", (ULONG)app, (ULONG)CxBase);
        return FALSE;
    }
    
    if (!app->cleanupStack) {
        Printf("[INIT] TTX_SetupCommodity: FAIL (cleanupStack=NULL)\n");
        return FALSE;
    }
    
    /* Create message port for commodity broker using Seiso */
    app->brokerPort = createMsgPort();
    if (!app->brokerPort) {
        Printf("[INIT] TTX_SetupCommodity: FAIL (createMsgPort failed)\n");
        return FALSE;
    }
    Printf("[INIT] TTX_SetupCommodity: brokerPort=%lx\n", (ULONG)app->brokerPort);
    
    /* Create broker using Seiso - mirrors CxBroker() API */
    /* COF_SHOW_HIDE enables show/hide commands from Exchange */
    /* NBU_UNIQUE | NBU_NOTIFY: single instance, notify on duplicate */
    {
        struct NewBroker nb;
        LONG brokerError = 0;
        
        /* Initialize NewBroker structure */
        nb.nb_Version = NB_VERSION;
        nb.nb_Name = (STRPTR)"TTX";
        nb.nb_Title = (STRPTR)"TTX";
        nb.nb_Descr = (STRPTR)"Text Editor";
        nb.nb_Unique = NBU_UNIQUE | NBU_NOTIFY;  /* Unique name, notify on duplicate */
        nb.nb_Flags = COF_SHOW_HIDE;             /* Support show/hide commands */
        nb.nb_Pri = 0;                           /* Normal priority */
        nb.nb_Port = app->brokerPort;
        nb.nb_ReservedChannel = 0;
        
        Printf("[INIT] TTX_SetupCommodity: creating broker with COF_SHOW_HIDE\n");
        app->broker = cxBroker(&nb, &brokerError);
        if (!app->broker) {
            /* Broker creation failed - could be duplicate instance or other error */
            Printf("[INIT] TTX_SetupCommodity: FAIL (CxBroker failed, error=%ld)\n", brokerError);
            /* Duplicate instance detection is handled by TTX_CheckExistingInstance */
            deleteMsgPort(app->brokerPort);
            app->brokerPort = NULL;
            return FALSE;
        }
    }
    
    /* Verify broker is active (cxBroker activates it, but verify) */
    /* ActivateCxObj returns non-zero on success, 0 on failure */
    if (ActivateCxObj(app->broker, TRUE) == 0) {
        /* Broker failed to activate */
        Printf("[INIT] TTX_SetupCommodity: broker activation failed\n");
        deleteCxObjAll(app->broker);
        app->broker = NULL;
        deleteMsgPort(app->brokerPort);
        app->brokerPort = NULL;
        return FALSE;
    }
    
    /* Check for any broker errors */
    if (CxObjError(app->broker) != 0) {
        Printf("[INIT] TTX_SetupCommodity: WARN (broker has errors, continuing)\n");
    }
    
    result = TRUE;
    Printf("[INIT] TTX_SetupCommodity: SUCCESS (broker=%lx)\n", (ULONG)app->broker);
    return result;
}

/* Remove commodity - now handled automatically by cleanup stack */
VOID TTX_RemoveCommodity(struct TTXApplication *app)
{
    /* Resources are automatically cleaned up by Seiso cleanup stack */
    /* This function is kept for compatibility but does nothing */
    /* Messages are cleaned up in TTX_Cleanup before stack deletion */
    Printf("[CLEANUP] TTX_RemoveCommodity: (handled by Seiso cleanup stack)\n");
}

/* Setup app icon for application-level iconification */
BOOL TTX_SetupAppIcon(struct TTXApplication *app)
{
    Printf("[INIT] TTX_SetupAppIcon: START\n");
    if (!app || !WorkbenchBase || !IconBase) {
        Printf("[INIT] TTX_SetupAppIcon: FAIL (app=%lx, WorkbenchBase=%lx, IconBase=%lx)\n", 
               (ULONG)app, (ULONG)WorkbenchBase, (ULONG)IconBase);
        return FALSE;
    }
    
    /* App icon will be created when iconifying, not at startup */
    /* Just initialize the fields */
    app->appIconPort = NULL;
    app->appIcon = NULL;
    app->appIconDO = NULL;
    app->iconified = FALSE;
    app->iconifyDeferred = FALSE;
    app->iconifyState = FALSE;
    
    Printf("[INIT] TTX_SetupAppIcon: SUCCESS (deferred)\n");
    return TRUE;
}

/* Remove app icon */
VOID TTX_RemoveAppIcon(struct TTXApplication *app)
{
    if (!app) {
        return;
    }
    
    Printf("[CLEANUP] TTX_RemoveAppIcon: START\n");
    
    /* Remove app icon if it exists */
    if (app->appIcon && WorkbenchBase) {
        RemoveAppIcon(app->appIcon);
        app->appIcon = NULL;
    }
    
    /* Free disk object if it exists */
    if (app->appIconDO && IconBase) {
        FreeDiskObject(app->appIconDO);
        app->appIconDO = NULL;
    }
    
    /* Delete message port if it exists */
    if (app->appIconPort) {
        struct Message *msg = NULL;
        /* Clean up any pending messages */
        Forbid();
        while ((msg = GetMsg(app->appIconPort)) != NULL) {
            ReplyMsg(msg);
        }
        Permit();
        DeleteMsgPort(app->appIconPort);
        app->appIconPort = NULL;
    }
    
    app->iconified = FALSE;
    
    Printf("[CLEANUP] TTX_RemoveAppIcon: DONE\n");
}

/* Deferred iconification - sets flag for main loop to process */
VOID TTX_Iconify(struct TTXApplication *app, BOOL iconify)
{
    if (!app) {
        return;
    }
    
    Printf("[ICONIFY] TTX_Iconify: deferring iconify=%s\n", iconify ? "TRUE" : "FALSE");
    app->iconifyDeferred = TRUE;
    app->iconifyState = iconify;
}

/* Save window state before closing */
BOOL TTX_SaveWindowState(struct Session *session)
{
    if (!session || !session->window) {
        return FALSE;
    }
    
    Printf("[WINDOW] TTX_SaveWindowState: saving state for session %lu\n", session->sessionID);
    
    /* Save window position and size */
    session->windowState.leftEdge = session->window->LeftEdge;
    session->windowState.topEdge = session->window->TopEdge;
    session->windowState.innerWidth = session->window->Width - session->window->BorderLeft - session->window->BorderRight;
    session->windowState.innerHeight = session->window->Height - session->window->BorderTop - session->window->BorderBottom;
    session->windowState.flags = session->window->Flags;
    
    Printf("[WINDOW] TTX_SaveWindowState: saved pos=(%ld,%ld) size=(%lu,%lu) flags=0x%08lx\n",
           session->windowState.leftEdge, session->windowState.topEdge,
           session->windowState.innerWidth, session->windowState.innerHeight,
           session->windowState.flags);
    
    return TRUE;
}

/* Restore window from saved state */
BOOL TTX_RestoreWindow(struct TTXApplication *app, struct Session *session)
{
    struct Screen *screen = NULL;
    struct TagItem windowTags[17];
    
    if (!app || !session || !app->cleanupStack) {
        Printf("[WINDOW] TTX_RestoreWindow: FAIL (app=%lx, session=%lx)\n", (ULONG)app, (ULONG)session);
        return FALSE;
    }
    
    if (session->window) {
        Printf("[WINDOW] TTX_RestoreWindow: window already open for session %lu\n", session->sessionID);
        return TRUE;
    }
    
    Printf("[WINDOW] TTX_RestoreWindow: restoring window for session %lu\n", session->sessionID);
    
    /* Lock public screen */
    screen = LockPubScreen(session->windowState.pubScreenName ? session->windowState.pubScreenName : (STRPTR)"Workbench");
    if (!screen) {
        Printf("[WINDOW] TTX_RestoreWindow: FAIL (LockPubScreen failed)\n");
        return FALSE;
    }
    
    /* Build window tags from saved state */
    windowTags[0].ti_Tag = WA_Flags;
    windowTags[0].ti_Data = session->windowState.flags;
    windowTags[1].ti_Tag = WA_InnerWidth;
    windowTags[1].ti_Data = session->windowState.innerWidth;
    windowTags[2].ti_Tag = WA_InnerHeight;
    windowTags[2].ti_Data = session->windowState.innerHeight;
    windowTags[3].ti_Tag = WA_Title;
    windowTags[3].ti_Data = (ULONG)(session->windowState.title ? session->windowState.title : (STRPTR)"Untitled");
    windowTags[4].ti_Tag = WA_IDCMP;
    windowTags[4].ti_Data = session->windowState.idcmpFlags;
    windowTags[5].ti_Tag = WA_ScreenTitle;
    windowTags[5].ti_Data = (ULONG)(session->windowState.screenTitle ? session->windowState.screenTitle : (STRPTR)"TTX");
    windowTags[6].ti_Tag = WA_AutoAdjust;
    windowTags[6].ti_Data = TRUE;
    windowTags[7].ti_Tag = WA_PubScreen;
    windowTags[7].ti_Data = (ULONG)screen;
    windowTags[8].ti_Tag = WA_SizeBRight;
    windowTags[8].ti_Data = TRUE;
    windowTags[9].ti_Tag = WA_SizeBBottom;
    windowTags[9].ti_Data = TRUE;
    windowTags[10].ti_Tag = TAG_DONE;
    windowTags[10].ti_Data = 0;
    
    /* Open window */
    session->window = openWindowTagList(windowTags);
    UnlockPubScreen(session->windowState.pubScreenName ? session->windowState.pubScreenName : (STRPTR)"Workbench", screen);
    
    if (!session->window) {
        Printf("[WINDOW] TTX_RestoreWindow: FAIL (openWindow failed)\n");
        return FALSE;
    }
    
    /* Set window limits */
    WindowLimits(session->window, session->windowState.minWidth, session->windowState.minHeight,
                 session->windowState.maxWidth, session->windowState.maxHeight);
    
    /* Restore window position if it was set */
    if (session->windowState.leftEdge != 50 || session->windowState.topEdge != 50) {
        MoveWindow(session->window, session->windowState.leftEdge, session->windowState.topEdge);
    }
    
    /* Recreate menu strip */
    if (!TTX_CreateMenuStrip(session)) {
        Printf("[WINDOW] TTX_RestoreWindow: WARN (CreateMenuStrip failed)\n");
    }
    
    /* Recreate scroll bar gadgets */
    {
        struct Screen *winScreen = session->window->WScreen;
        struct DrawInfo *drawInfo = NULL;
        struct Gadget *gadgetList = NULL;
        static LONG icamap[4] = {GA_ID, ICSPECIAL_CODE, 0, 0};
        ULONG initialTotal = 100;
        ULONG initialVisible = 50;
        ULONG initialTop = 0;
        
        /* Get screen draw info for gadget creation */
        drawInfo = GetScreenDrawInfo(winScreen);
        if (drawInfo) {
            /* Calculate initial scroll values for prop gadgets */
            if (session->buffer) {
                /* Use buffer dimensions if available */
                initialVisible = session->buffer->pageH;
                initialTotal = (session->buffer->maxScrollY > session->buffer->pageH) ? 
                              session->buffer->maxScrollY : session->buffer->pageH;
                if (initialTotal < initialVisible) {
                    initialTotal = initialVisible;
                }
                initialTop = session->buffer->scrollY;
            }
            
            /* Create vertical scroll bar prop gadget */
            session->vertPropGadget = (struct Gadget *)NewObject(NULL, PROPGCLASS,
                GA_RelRight, -session->window->BorderRight + 5,
                GA_Width, session->window->BorderRight - 8,
                GA_Top, session->window->BorderTop + 2,
                GA_RelHeight, -(session->window->BorderTop + session->window->BorderBottom + 4),
                GA_Next, gadgetList,
                ICA_TARGET, ICTARGET_IDCMP,
                ICA_MAP, icamap,
                PGA_Freedom, FREEVERT,
                PGA_NewLook, TRUE,
                PGA_Borderless, TRUE,
                PGA_VertBody, MAXBODY,
                PGA_Total, initialTotal,
                PGA_Visible, initialVisible,
                PGA_Top, initialTop,
                GA_ID, GID_VERT_PROP,
                TAG_DONE);
            
            if (session->vertPropGadget) {
                session->vertPropGadget->Flags |= GFLG_RELRIGHT | GFLG_RELHEIGHT;
                gadgetList = session->vertPropGadget;
            }
            
            /* Calculate horizontal scroll values */
            initialTotal = 100;
            initialVisible = 50;
            initialTop = 0;
            if (session->buffer) {
                initialVisible = session->buffer->pageW;
                initialTotal = (session->buffer->maxScrollX > session->buffer->pageW) ? 
                              session->buffer->maxScrollX : session->buffer->pageW;
                if (initialTotal < initialVisible) {
                    initialTotal = initialVisible;
                }
                initialTop = session->buffer->scrollX;
            }
            
            /* Create horizontal scroll bar prop gadget */
            session->horizPropGadget = (struct Gadget *)NewObject(NULL, PROPGCLASS,
                GA_Left, session->window->BorderLeft,
                GA_RelBottom, -session->window->BorderBottom + 3,
                GA_RelWidth, -(session->window->BorderLeft + session->window->BorderRight + 2),
                GA_Height, session->window->BorderBottom - 4,
                GA_Next, gadgetList,
                ICA_TARGET, ICTARGET_IDCMP,
                ICA_MAP, icamap,
                PGA_Freedom, FREEHORIZ,
                PGA_NewLook, TRUE,
                PGA_Borderless, TRUE,
                PGA_HorizBody, MAXBODY,
                PGA_Total, initialTotal,
                PGA_Visible, initialVisible,
                PGA_Top, initialTop,
                GA_ID, GID_HORIZ_PROP,
                TAG_DONE);
            
            if (session->horizPropGadget) {
                session->horizPropGadget->Flags |= GFLG_RELBOTTOM | GFLG_RELWIDTH;
                gadgetList = session->horizPropGadget;
            }
            
            /* Add gadgets to window */
            if (gadgetList) {
                AddGList(session->window, gadgetList, (UWORD)-1, (UWORD)-1, NULL);
                RefreshGList(gadgetList, session->window, NULL, (UWORD)-1);
            }
            
            FreeScreenDrawInfo(winScreen, drawInfo);
        }
    }
    
    /* Update scroll bars with current buffer state */
    if (session->buffer) {
        CalculateMaxScroll(session->buffer, session->window);
        UpdateScrollBars(session);
    }
    
    session->windowState.windowOpen = TRUE;
    
    Printf("[WINDOW] TTX_RestoreWindow: SUCCESS (window=%lx)\n", (ULONG)session->window);
    return TRUE;
}

/* Perform actual iconification/uniconification */
VOID TTX_DoIconify(struct TTXApplication *app, BOOL iconify)
{
    struct Session *session = NULL;
    STRPTR programName = NULL;
    STRPTR iconName = NULL;
    
    if (!app) {
        return;
    }
    
    Printf("[ICONIFY] TTX_DoIconify: START (iconify=%s, currently iconified=%s)\n", 
           iconify ? "TRUE" : "FALSE", app->iconified ? "TRUE" : "FALSE");
    
    if (iconify && !app->iconified) {
        /* Iconify: Close all windows, create app icon */
        Printf("[ICONIFY] TTX_DoIconify: iconifying application\n");
        
        /* Save window state and close all windows but keep sessions alive */
        session = app->sessions;
        while (session) {
            if (session->window) {
                Printf("[ICONIFY] TTX_DoIconify: saving state and closing window for session %lu\n", session->sessionID);
                TTX_SaveWindowState(session);
                CloseWindow(session->window);
                session->window = NULL;  /* Keep session, just close window */
                session->windowState.windowOpen = FALSE;
                /* Free menu strip and gadgets - they'll be recreated on restore */
                TTX_FreeMenuStrip(session);
                session->vertPropGadget = NULL;
                session->horizPropGadget = NULL;
            }
            session = session->next;
        }
        
        /* Create message port for app icon */
        if (!app->appIconPort) {
            app->appIconPort = CreateMsgPort();
            if (!app->appIconPort) {
                Printf("[ICONIFY] TTX_DoIconify: FAIL (CreateMsgPort failed)\n");
                /* Reopen windows on failure */
                session = app->sessions;
                while (session) {
                    if (!session->window) {
                        /* TODO: Reopen window - for now just mark as needing reopen */
                    }
                    session = session->next;
                }
                return;
            }
            Printf("[ICONIFY] TTX_DoIconify: created appIconPort=%lx\n", (ULONG)app->appIconPort);
        }
        
        /* Get program name for icon */
        {
            struct Task *task = NULL;
            task = FindTask(NULL);
            if (task && task->tc_Node.ln_Name) {
                programName = task->tc_Node.ln_Name;
            } else {
                programName = "TTX";
            }
        }
        
        /* Get disk object for app icon */
        if (!app->appIconDO && IconBase) {
            /* Try GetIconTags first (V44+) */
            if (IconBase->lib_Version >= 44) {
                app->appIconDO = GetIconTags(programName,
                    ICONGETA_FailIfUnavailable, FALSE,
                    TAG_END);
            } else {
                /* Fall back to GetDiskObjectNew for older versions */
                app->appIconDO = GetDiskObjectNew(programName);
            }
            
            if (!app->appIconDO) {
                Printf("[ICONIFY] TTX_DoIconify: WARN (could not get disk object, using default)\n");
                /* Continue anyway - AddAppIcon will use default icon */
            } else {
                /* Set icon position to NO_ICON_POSITION to let user position it */
                app->appIconDO->do_CurrentX = NO_ICON_POSITION;
                app->appIconDO->do_CurrentY = NO_ICON_POSITION;
            }
        }
        
        /* Add app icon to Workbench */
        if (!WorkbenchBase) {
            Printf("[ICONIFY] TTX_DoIconify: FAIL (WorkbenchBase not available)\n");
            /* Reopen windows on failure */
            session = app->sessions;
            while (session) {
                if (!session->window) {
                    TTX_RestoreWindow(app, session);
                }
                session = session->next;
            }
            return;
        }
        
        if (!app->appIconPort) {
            Printf("[ICONIFY] TTX_DoIconify: FAIL (appIconPort not created)\n");
            /* Reopen windows on failure */
            session = app->sessions;
            while (session) {
                if (!session->window) {
                    TTX_RestoreWindow(app, session);
                }
                session = session->next;
            }
            return;
        }
        
        iconName = FilePart(programName);
        if (!iconName || iconName[0] == '\0') {
            iconName = programName;
        }
        
        Printf("[ICONIFY] TTX_DoIconify: calling AddAppIcon (name='%s', port=%lx, diskObj=%lx)\n",
               iconName, (ULONG)app->appIconPort, (ULONG)app->appIconDO);
        app->appIcon = AddAppIcon(0, 0, iconName, app->appIconPort, NULL, app->appIconDO, TAG_END);
        if (!app->appIcon) {
            LONG errorCode = 0;
            errorCode = IoErr();
            Printf("[ICONIFY] TTX_DoIconify: FAIL (AddAppIcon failed, IoErr=%ld)\n", errorCode);
            /* Cleanup and reopen windows */
            if (app->appIconDO) {
                FreeDiskObject(app->appIconDO);
                app->appIconDO = NULL;
            }
            DeleteMsgPort(app->appIconPort);
            app->appIconPort = NULL;
            /* Reopen windows */
            session = app->sessions;
            while (session) {
                if (!session->window) {
                    TTX_RestoreWindow(app, session);
                }
                session = session->next;
            }
            return;
        }
        Printf("[ICONIFY] TTX_DoIconify: created appIcon=%lx\n", (ULONG)app->appIcon);
        
        app->iconified = TRUE;
        Printf("[ICONIFY] TTX_DoIconify: SUCCESS (iconified)\n");
        
    } else if (!iconify && app->iconified) {
        /* Uniconify: Remove app icon, reopen windows */
        Printf("[ICONIFY] TTX_DoIconify: uniconifying application\n");
        
        /* Remove app icon */
        if (app->appIcon && WorkbenchBase) {
            RemoveAppIcon(app->appIcon);
            app->appIcon = NULL;
        }
        
        /* Free disk object */
        if (app->appIconDO && IconBase) {
            FreeDiskObject(app->appIconDO);
            app->appIconDO = NULL;
        }
        
        /* Delete message port */
        if (app->appIconPort) {
            struct Message *msg = NULL;
            /* Clean up any pending messages */
            Forbid();
            while ((msg = GetMsg(app->appIconPort)) != NULL) {
                ReplyMsg(msg);
            }
            Permit();
            DeleteMsgPort(app->appIconPort);
            app->appIconPort = NULL;
        }
        
        /* Reopen all windows */
        session = app->sessions;
        while (session) {
            if (!session->window) {
                Printf("[ICONIFY] TTX_DoIconify: restoring window for session %lu\n", session->sessionID);
                if (!TTX_RestoreWindow(app, session)) {
                    Printf("[ICONIFY] TTX_DoIconify: WARN (failed to restore window for session %lu)\n", session->sessionID);
                } else {
                    /* Refresh display after restoring window */
                    if (session->buffer) {
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        UpdateScrollBars(session);
                    }
                }
            }
            session = session->next;
        }
        
        app->iconified = FALSE;
        Printf("[ICONIFY] TTX_DoIconify: SUCCESS (uniconified)\n");
    } else {
        Printf("[ICONIFY] TTX_DoIconify: no change needed (iconify=%s, iconified=%s)\n",
               iconify ? "TRUE" : "FALSE", app->iconified ? "TRUE" : "FALSE");
    }
}

/* Process app icon messages (double-click, file drops) */
VOID TTX_ProcessAppIcon(struct TTXApplication *app)
{
    struct AppMessage *msg = NULL;
    STRPTR fileName = NULL;
    STRPTR fullPath = NULL;
    ULONG i = 0;
    ULONG pathLen = 0;
    BPTR oldDir = 0;
    
    if (!app || !app->appIconPort) {
        return;
    }
    
    while ((msg = (struct AppMessage *)GetMsg(app->appIconPort)) != NULL) {
        Printf("[ICONIFY] TTX_ProcessAppIcon: received message (am_NumArgs=%lu)\n", msg->am_NumArgs);
        
        /* Always uniconify on app icon click */
        TTX_Iconify(app, FALSE);
        
        /* Process dropped files */
        if (msg->am_NumArgs > 0 && msg->am_ArgList) {
            for (i = 0; i < msg->am_NumArgs; i++) {
                /* Convert lock+name to full path */
                fileName = NULL;
                fullPath = NULL;
                pathLen = 0;
                
                /* Build path from lock and name */
                if (msg->am_ArgList[i].wa_Lock && msg->am_ArgList[i].wa_Name) {
                    /* Get current directory to restore later */
                    oldDir = CurrentDir(msg->am_ArgList[i].wa_Lock);
                    
                    /* Build full path - allocate buffer */
                    pathLen = 256; /* Reasonable max path length */
                    fullPath = (STRPTR)allocVec(pathLen, MEMF_CLEAR);
                    if (fullPath) {
                        /* Get path from lock */
                        if (NameFromLock(msg->am_ArgList[i].wa_Lock, fullPath, pathLen)) {
                            /* Add filename */
                            if (AddPart(fullPath, msg->am_ArgList[i].wa_Name, pathLen)) {
                                Printf("[ICONIFY] TTX_ProcessAppIcon: opening file '%s'\n", fullPath);
                                TTX_CreateSession(app, fullPath);
                            } else {
                                Printf("[ICONIFY] TTX_ProcessAppIcon: WARN (AddPart failed)\n");
                            }
                        } else {
                            Printf("[ICONIFY] TTX_ProcessAppIcon: WARN (NameFromLock failed)\n");
                        }
                    }
                    
                    /* Restore original directory */
                    if (oldDir) {
                        CurrentDir(oldDir);
                    }
                } else if (msg->am_ArgList[i].wa_Name) {
                    /* Just a name, no lock - use as-is */
                    Printf("[ICONIFY] TTX_ProcessAppIcon: opening file '%s'\n", msg->am_ArgList[i].wa_Name);
                    TTX_CreateSession(app, msg->am_ArgList[i].wa_Name);
                }
            }
        } else {
            /* Double-click with no files - just uniconify (already done above) */
            Printf("[ICONIFY] TTX_ProcessAppIcon: double-click (no files)\n");
        }
        
        /* Reply to message */
        ReplyMsg((struct Message *)msg);
    }
}

/* Create a new session (window) */
BOOL TTX_CreateSession(struct TTXApplication *app, STRPTR fileName)
{
    struct Session *session = NULL;
    struct Screen *screen = NULL;
    STRPTR titleText = NULL;
    ULONG titleLen = 0;
    BOOL result = FALSE;
    
    Printf("[INIT] TTX_CreateSession: START (fileName=%s)\n", fileName ? fileName : (STRPTR)"(null)");
    if (!app || !app->cleanupStack) {
        Printf("[INIT] TTX_CreateSession: FAIL (app=%lx, stack=%lx)\n", (ULONG)app, app ? (ULONG)app->cleanupStack : 0);
        return FALSE;
    }
    
    /* Allocate session structure on global cleanup stack */
    session = (struct Session *)allocVec(sizeof(struct Session), MEMF_CLEAR);
    if (!session) {
        Printf("[INIT] TTX_CreateSession: FAIL (allocVec session failed)\n");
        return FALSE;
    }
    Printf("[INIT] TTX_CreateSession: session=%lx\n", (ULONG)session);
    
    /* Initialize session */
    session->sessionID = app->nextSessionID++;
    session->cleanupStack = app->cleanupStack;  /* Store pointer to global cleanup stack */
    session->window = NULL;
    session->menuStrip = NULL;  /* Will be created after window is opened */
    session->vertPropGadget = NULL;  /* Vertical scroll bar prop gadget */
    session->horizPropGadget = NULL;  /* Horizontal scroll bar prop gadget */
    session->buffer = NULL;
    session->next = NULL;  /* Initialize list pointers */
    session->prev = NULL;
    
    /* Initialize window state with defaults */
    session->windowState.leftEdge = 50;  /* Default position */
    session->windowState.topEdge = 50;
    session->windowState.innerWidth = 600;  /* Default size */
    session->windowState.innerHeight = 400;
    session->windowState.flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SIZEGADGET | WFLG_CLOSEGADGET | WFLG_ACTIVATE | WFLG_SMART_REFRESH | WFLG_NEWLOOKMENUS | WFLG_REPORTMOUSE;
    session->windowState.idcmpFlags = IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY | IDCMP_RAWKEY | IDCMP_REFRESHWINDOW | IDCMP_NEWSIZE | IDCMP_MOUSEBUTTONS | IDCMP_MENUPICK | IDCMP_IDCMPUPDATE;
    session->windowState.title = NULL;
    session->windowState.screenTitle = NULL;
    session->windowState.pubScreenName = NULL;
    session->windowState.minWidth = 200;
    session->windowState.minHeight = 100;
    session->windowState.maxWidth = 32767;
    session->windowState.maxHeight = 32767;
    session->windowState.windowOpen = FALSE;
    
    /* Initialize document state */
    session->docState.fileName = NULL;
    session->docState.modified = FALSE;
    session->docState.readOnly = FALSE;
    session->docState.loadTime = 0;
    session->docState.fileSize = 0;
    session->docState.fileExists = FALSE;
    
    /* Allocate and initialize text buffer using global cleanup stack */
    session->buffer = (struct TextBuffer *)allocVec(sizeof(struct TextBuffer), MEMF_CLEAR);
    if (!session->buffer) {
        Printf("[INIT] TTX_CreateSession: FAIL (allocVec buffer failed)\n");
        freeVec(session);
        return FALSE;
    }
    Printf("[INIT] TTX_CreateSession: buffer=%lx\n", (ULONG)session->buffer);
    
    if (!InitTextBuffer(session->buffer, app->cleanupStack)) {
        Printf("[INIT] TTX_CreateSession: FAIL (InitTextBuffer failed)\n");
        freeVec(session);
        return FALSE;
    }
    
    /* Copy filename if provided */
    if (fileName) {
        titleLen = 0;
        while (fileName[titleLen] != '\0') {
            titleLen++;
        }
        if (titleLen > 0) {
            session->docState.fileName = (STRPTR)allocVec(titleLen + 1, MEMF_CLEAR);
            if (session->docState.fileName) {
                Printf("[INIT] TTX_CreateSession: allocated fileName=%lx\n", (ULONG)session->docState.fileName);
                CopyMem(fileName, session->docState.fileName, titleLen);
                session->docState.fileName[titleLen] = '\0';
                /* Check if file exists and get its size */
                {
                    BPTR fileLock = 0;
                    struct FileInfoBlock *fib = NULL;
                    BPTR oldDir = 0;
                    
                    fileLock = Lock(fileName, SHARED_LOCK);
                    if (fileLock) {
                        oldDir = CurrentDir(fileLock);
                        fib = (struct FileInfoBlock *)allocVec(sizeof(struct FileInfoBlock), MEMF_CLEAR);
                        if (fib && Examine(fileLock, fib)) {
                            session->docState.fileExists = TRUE;
                            session->docState.fileSize = fib->fib_Size;
                            session->docState.loadTime = fib->fib_Date.ds_Days * 86400L + fib->fib_Date.ds_Minute * 60L + fib->fib_Date.ds_Tick / 50L;
                        }
                        if (fib) {
                            freeVec(fib);
                        }
                        if (oldDir) {
                            CurrentDir(oldDir);
                        }
                        UnLock(fileLock);
                    }
                }
            }
        }
    }
    
    /* Create window title */
    if (session->docState.fileName) {
        titleLen = 0;
        while (session->docState.fileName[titleLen] != '\0') {
            titleLen++;
        }
    } else {
        titleLen = 8; /* "Untitled" */
    }
    
    titleText = (STRPTR)allocVec(titleLen + 20, MEMF_CLEAR);
    if (titleText) {
        Printf("[INIT] TTX_CreateSession: allocated titleText=%lx\n", (ULONG)titleText);
        if (session->docState.fileName) {
            CopyMem(session->docState.fileName, titleText, titleLen);
            titleText[titleLen] = '\0';
        } else {
            CopyMem("Untitled", titleText, 8);
            titleText[8] = '\0';
        }
        /* Store title in window state */
        session->windowState.title = titleText;
    }
    
    /* Store screen title */
    session->windowState.screenTitle = (STRPTR)allocVec(4, MEMF_CLEAR);
    if (session->windowState.screenTitle) {
        CopyMem("TTX", session->windowState.screenTitle, 3);
        session->windowState.screenTitle[3] = '\0';
    }
    
    /* Lock public screen (temporary, doesn't need tracking) */
    screen = LockPubScreen((STRPTR)"Workbench");
    if (!screen) {
        Printf("[INIT] TTX_CreateSession: FAIL (LockPubScreen failed)\n");
        freeVec(session);
        return FALSE;
    }
    
    /* Open window using global cleanup stack */
    Printf("[INIT] TTX_CreateSession: opening window\n");
    {
        struct TagItem windowTags[17];
        /* Set base flags first - includes drag bar, depth, size, close gadgets */
        windowTags[0].ti_Tag = WA_Flags;
        windowTags[0].ti_Data = session->windowState.flags;
        windowTags[1].ti_Tag = WA_InnerWidth;
        windowTags[1].ti_Data = session->windowState.innerWidth;
        windowTags[2].ti_Tag = WA_InnerHeight;
        windowTags[2].ti_Data = session->windowState.innerHeight;
        windowTags[3].ti_Tag = WA_Title;
        windowTags[3].ti_Data = (ULONG)(session->windowState.title ? session->windowState.title : (STRPTR)"Untitled");
        windowTags[4].ti_Tag = WA_IDCMP;
        windowTags[4].ti_Data = session->windowState.idcmpFlags;
        windowTags[5].ti_Tag = WA_ScreenTitle;
        windowTags[5].ti_Data = (ULONG)(session->windowState.screenTitle ? session->windowState.screenTitle : (STRPTR)"TTX");
        windowTags[6].ti_Tag = WA_AutoAdjust;
        windowTags[6].ti_Data = TRUE;
        windowTags[7].ti_Tag = WA_PubScreen;
        windowTags[7].ti_Data = (ULONG)screen;
        /* Size gadget positioning - must specify which border for sizing gadget */
        windowTags[8].ti_Tag = WA_SizeBRight;
        windowTags[8].ti_Data = TRUE;
        windowTags[9].ti_Tag = WA_SizeBBottom;
        windowTags[9].ti_Data = TRUE;
        /* Super bitmap will be set after window creation if supported */
        windowTags[10].ti_Tag = TAG_DONE;
        windowTags[10].ti_Data = 0;
        
        session->window = openWindowTagList(windowTags);
    }
    
    UnlockPubScreen((STRPTR)"Workbench", screen);
    
    if (!session->window) {
        Printf("[INIT] TTX_CreateSession: FAIL (openWindow failed)\n");
        freeVec(session);
        return FALSE;
    }
    Printf("[INIT] TTX_CreateSession: window=%lx\n", (ULONG)session->window);
    Printf("[INIT] TTX_CreateSession: window flags=%lx (WFLG_DRAGBAR=%s)\n", 
           (ULONG)session->window->Flags, 
           (session->window->Flags & WFLG_DRAGBAR) ? "YES" : "NO");
    
    /* Set window limits */
    WindowLimits(session->window, session->windowState.minWidth, session->windowState.minHeight, 
                 session->windowState.maxWidth, session->windowState.maxHeight);
    
    /* Save window position and size from actual window */
    if (session->window) {
        session->windowState.leftEdge = session->window->LeftEdge;
        session->windowState.topEdge = session->window->TopEdge;
        session->windowState.innerWidth = session->window->Width - session->window->BorderLeft - session->window->BorderRight;
        session->windowState.innerHeight = session->window->Height - session->window->BorderTop - session->window->BorderBottom;
        session->windowState.windowOpen = TRUE;
    }
    
    /* Create scroll bar prop gadgets */
    /* Note: Must be created AFTER window is opened so we can use window border values */
    /* When using WA_InnerWidth/WA_InnerHeight, border gadgets must use relative positioning */
    {
        struct Screen *winScreen = session->window->WScreen;
        struct DrawInfo *drawInfo = NULL;
        struct Gadget *gadgetList = NULL;
        static LONG icamap[4] = {GA_ID, ICSPECIAL_CODE, 0, 0};
        
        /* Get screen draw info for gadget creation */
        drawInfo = GetScreenDrawInfo(winScreen);
        if (drawInfo) {
            ULONG initialTotal = 100;
            ULONG initialVisible = 50;
            ULONG initialTop = 0;
            /* Calculate max scroll values BEFORE creating scroll bars */
            /* This ensures pageW, pageH, maxScrollX, and maxScrollY are correct */
            if (session->buffer) {
                CalculateMaxScroll(session->buffer, session->window);
            }
            
            /* Calculate initial scroll values for prop gadgets */
            /* These values are now accurate because CalculateMaxScroll was called above */
            
            if (session->buffer) {
                /* Use buffer dimensions if available */
                initialVisible = session->buffer->pageH;
                initialTotal = (session->buffer->maxScrollY > session->buffer->pageH) ? 
                              session->buffer->maxScrollY : session->buffer->pageH;
                if (initialTotal < initialVisible) {
                    initialTotal = initialVisible;
                }
                initialTop = session->buffer->scrollY;
            }
            
            /* Create vertical scroll bar prop gadget (right border) */
            /* Position in window using relative positioning, not as border gadget */
            /* GA_RelRight with negative value positions from right edge of window */
            /* GA_Width sets explicit width (border size minus padding) */
            /* GA_Top sets absolute top position (border top plus padding) */
            /* GA_RelHeight with negative value makes it span full height accounting for top/bottom */
            /* Important: Must stop before horizontal scroll bar at bottom */
            /* The horizontal scroll bar is at the bottom border, so vertical scroll bar should stop there */
            /* Height = window height - top border - bottom border (where horizontal scroll bar is) */
            session->vertPropGadget = (struct Gadget *)NewObject(NULL, PROPGCLASS,
                GA_RelRight, -session->window->BorderRight + 5,  /* Position from right edge */
                GA_Width, session->window->BorderRight - 8,        /* Width of slider */
                GA_Top, session->window->BorderTop + 2,           /* Top position */
                GA_RelHeight, -(session->window->BorderTop + session->window->BorderBottom + 4),  /* Height - stops before horizontal scroll bar */
                GA_Next, gadgetList,
                ICA_TARGET, ICTARGET_IDCMP,
                ICA_MAP, icamap,
                PGA_Freedom, FREEVERT,
                PGA_NewLook, TRUE,
                PGA_Borderless, TRUE,
                PGA_VertBody, MAXBODY,
                PGA_Total, initialTotal,
                PGA_Visible, initialVisible,
                PGA_Top, initialTop,
                GA_ID, GID_VERT_PROP,
                TAG_DONE);
            
            if (session->vertPropGadget) {
                /* Set relative positioning flags for vertical scrollbar */
                session->vertPropGadget->Flags |= GFLG_RELRIGHT | GFLG_RELHEIGHT;
                gadgetList = session->vertPropGadget;
                Printf("[INIT] TTX_CreateSession: vertical prop gadget created\n");
            }
            
            /* Calculate initial horizontal scroll values */
            /* These values are now accurate because CalculateMaxScroll was called above */
            initialTotal = 100;
            initialVisible = 50;
            initialTop = 0;
            if (session->buffer) {
                /* Calculate maximum line length for horizontal scrolling */
                ULONG maxLineLen = 0;
                ULONG i = 0;
                
                if (session->buffer->lines && session->buffer->lineCount > 0) {
                    for (i = 0; i < session->buffer->lineCount; i++) {
                        if (session->buffer->lines[i].length > maxLineLen) {
                            maxLineLen = session->buffer->lines[i].length;
                        }
                    }
                }
                
                /* Use calculated values from CalculateMaxScroll */
                initialVisible = session->buffer->pageW;
                /* Total should be max line length, not maxScrollX (which is maxLineLen - pageW) */
                initialTotal = maxLineLen;
                if (initialTotal < initialVisible) {
                    initialTotal = initialVisible;
                }
                initialTop = session->buffer->scrollX;
            }
            
            /* Create horizontal scroll bar prop gadget (bottom border) */
            /* Position in window using relative positioning, not as border gadget */
            /* GA_Left sets absolute left position (border left) */
            /* GA_RelBottom with negative value positions from bottom edge */
            /* GA_RelWidth with negative value makes it span full width accounting for left/right */
            /* GA_Height sets explicit height (border size minus padding) */
            /* Prop gadgets require PGA_Total, PGA_Visible, and PGA_Top to be set at creation */
            /* Note: Horizontal scroll bar must be added BEFORE vertical scroll bar in the chain */
            /* so that it appears below the text area and doesn't interfere with vertical scrolling */
            session->horizPropGadget = (struct Gadget *)NewObject(NULL, PROPGCLASS,
                GA_Left, session->window->BorderLeft,              /* Start at left border */
                GA_RelBottom, -session->window->BorderBottom + 3,  /* Position from bottom */
                GA_RelWidth, -(session->window->BorderLeft + session->window->BorderRight + 2),  /* Width */
                GA_Height, session->window->BorderBottom - 4,      /* Height of slider */
                GA_Next, gadgetList,  /* Chain to vertical scroll bar (if it exists) */
                ICA_TARGET, ICTARGET_IDCMP,
                ICA_MAP, icamap,
                PGA_Freedom, FREEHORIZ,
                PGA_NewLook, TRUE,
                PGA_Borderless, TRUE,
                PGA_HorizBody, MAXBODY,
                PGA_Total, initialTotal,
                PGA_Visible, initialVisible,
                PGA_Top, initialTop,
                GA_ID, GID_HORIZ_PROP,
                TAG_DONE);
            
            if (session->horizPropGadget) {
                /* Set relative positioning flags for horizontal scrollbar */
                session->horizPropGadget->Flags |= GFLG_RELBOTTOM | GFLG_RELWIDTH;
                /* Make horizontal scroll bar the head of the gadget list */
                /* This ensures it's processed first and appears at the bottom */
                gadgetList = session->horizPropGadget;
                Printf("[INIT] TTX_CreateSession: horizontal prop gadget created\n");
            }
            
            /* Add gadgets to window */
            if (gadgetList) {
                AddGList(session->window, gadgetList, (UWORD)-1, (UWORD)-1, NULL);
                RefreshGList(gadgetList, session->window, NULL, (UWORD)-1);
            }
            
            FreeScreenDrawInfo(winScreen, drawInfo);
        }
    }
    
    /* Create menu strip for this window */
    if (!TTX_CreateMenuStrip(session)) {
        Printf("[INIT] TTX_CreateSession: WARN (menu creation failed, continuing without menu)\n");
    }
    
    /* Create super bitmap for optimized scrolling (Graphics v39+) */
    if (session->buffer && GfxBase && ((struct Library *)GfxBase)->lib_Version >= 39) {
        if (CreateSuperBitMap(session->buffer, session->window)) {
            /* Set super bitmap on window for ScrollLayer support */
            /* Note: This requires the window to be created with WA_SmartRefresh */
            /* The super bitmap is already allocated, but we need to tell the window about it */
            /* However, WA_SuperBitMap must be set at window creation time */
            /* So we'll use ScrollLayer without WA_SuperBitMap for now */
            /* ScrollLayer works with regular windows too, just not as efficiently */
            Printf("[INIT] TTX_CreateSession: super bitmap created\n");
        } else {
            Printf("[INIT] TTX_CreateSession: super bitmap creation failed (continuing without it)\n");
        }
    }
    
    /* Load file if filename provided */
    if (session->docState.fileName && session->buffer) {
        if (!LoadFile(session->docState.fileName, session->buffer, app->cleanupStack)) {
            /* File load failed, but keep empty buffer */
        }
    }
    
    /* Calculate max scroll values and update scroll bars */
    if (session->buffer) {
        CalculateMaxScroll(session->buffer, session->window);
        UpdateScrollBars(session);
    }
    
    /* Initial render */
    if (session->buffer) {
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
    }
    
    /* Add to session list */
    session->prev = NULL;  /* New session is always first in list */
    if (app->sessions) {
        session->next = app->sessions;
        app->sessions->prev = session;
    } else {
        session->next = NULL;  /* Explicitly set if first session */
    }
    app->sessions = session;
    app->sessionCount++;
    app->activeSession = session;
    
    result = TRUE;
    Printf("[INIT] TTX_CreateSession: SUCCESS (sessionID=%lu, window=%lx)\n", session->sessionID, (ULONG)session->window);
    return result;
}

/* Destroy a session */
VOID TTX_DestroySession(struct TTXApplication *app, struct Session *session)
{
    struct IntuiMessage *imsg = NULL;
    
    Printf("[CLEANUP] TTX_DestroySession: START (session=%lx, sessionID=%lu)\n", (ULONG)session, session ? session->sessionID : 0);
    if (!app || !session) {
        Printf("[CLEANUP] TTX_DestroySession: DONE (app=%lx, session=%lx)\n", (ULONG)app, (ULONG)session);
        return;
    }
    
    /* Remove from session list */
    if (session->prev) {
        session->prev->next = session->next;
    } else {
        app->sessions = session->next;
    }
    
    if (session->next) {
        session->next->prev = session->prev;
    }
    
    /* Clean up any pending messages before closing window */
    /* Note: We check for INVALID_RESOURCE to avoid accessing freed windows */
    if (session->window && session->window != INVALID_RESOURCE && session->window->UserPort) {
        while ((imsg = (struct IntuiMessage *)GetMsg(session->window->UserPort)) != NULL) {
            ReplyMsg((struct Message *)imsg);
        }
    }
    
    /* Update active session BEFORE freeing session structure */
    if (app->activeSession == session) {
        app->activeSession = app->sessions;
    }
    
    /* Decrement session count BEFORE freeing */
    app->sessionCount--;
    
    /* Free menu strip BEFORE window closes (window must still be valid for ClearMenuStrip) */
    /* The menu is tracked on the global cleanup stack, but we need to free it manually */
    if (session) {
        TTX_FreeMenuStrip(session);
    }
    
    /* Free text buffer - all resources are tracked on global cleanup stack */
    if (session->buffer && app->cleanupStack) {
        FreeTextBuffer(session->buffer, app->cleanupStack);
    }
    
    /* Mark window as invalid after cleanup stack handles it */
    if (session->window && session->window != INVALID_RESOURCE) {
        session->window = INVALID_RESOURCE;
    }
    
    /* Free session structure from global cleanup stack */
    Printf("[CLEANUP] TTX_DestroySession: freeing session=%lx\n", (ULONG)session);
    if (app && app->cleanupStack && session) {
        freeVec(session);
    }
    Printf("[CLEANUP] TTX_DestroySession: DONE (remaining sessions=%lu)\n", app->sessionCount);
}

/* Handle commodity message (from Exchange or other instances) */
BOOL TTX_HandleCommodityMessage(struct TTXApplication *app, struct Message *msg)
{
    struct CxMsg *cxMsg = NULL;
    ULONG cxMsgID = 0;
    ULONG cxMsgType = 0;
    BOOL result = FALSE;
    struct TTXMessage *ttxMsg = NULL;
    struct Session *session = NULL;
    
    if (!app || !msg) {
        return FALSE;
    }
    
    cxMsg = (struct CxMsg *)msg;
    cxMsgID = CxMsgID((const CxMsg *)cxMsg);
    cxMsgType = CxMsgType((const CxMsg *)cxMsg);
    
    /* Reply to message FIRST (required by commodities.library) */
    ReplyMsg((struct Message *)cxMsg);
    
    /* Process the message */
    switch (cxMsgType) {
        case CXM_IEVENT:
            /* Input event - check if it's our inter-instance message (ID=1) */
            if (cxMsgID == 1L) {
                /* This is an inter-instance message from CxSender */
                /* The actual message data is in CxMsgData */
                ttxMsg = (struct TTXMessage *)CxMsgData((const CxMsg *)cxMsg);
                if (ttxMsg) {
                    switch (ttxMsg->type) {
                        case TTX_MSG_OPEN_FILE:
                            if (ttxMsg->fileName) {
                                TTX_CreateSession(app, ttxMsg->fileName);
                            }
                            break;
                            
                        case TTX_MSG_OPEN_NEW:
                            TTX_CreateSession(app, NULL);
                            break;
                            
                        case TTX_MSG_QUIT:
                            app->running = FALSE;
                            break;
                            
                        default:
                            break;
                    }
                    
                    /* According to Exec message docs: ALL messages must be replied to with ReplyMsg() */
                    /* For one-way messages (mn_ReplyPort=NULL), ReplyMsg() does nothing */
                    /* But we still call it, then free the message since no reply will come back */
                    ReplyMsg(msg);
                    /* Messages from other instances were allocated by THEIR cleanup stack, not ours */
                    /* So we must free them directly with FreeVec, not through our cleanup stack */
                    if (ttxMsg->fileName) {
                        FreeVec(ttxMsg->fileName);
                    }
                    FreeVec(ttxMsg);
                }
            }
            result = TRUE;
            break;
            
        case CXM_COMMAND:
            /* Command from Exchange */
            switch (cxMsgID) {
                case CXCMD_DISABLE:
                    ActivateCxObj(app->broker, FALSE);
                    result = TRUE;
                    break;
                case CXCMD_ENABLE:
                    ActivateCxObj(app->broker, TRUE);
                    result = TRUE;
                    break;
                case CXCMD_APPEAR:
                    /* Show/uniconify application */
                    TTX_Iconify(app, FALSE);
                    /* Bring all windows to front */
                    if (app->sessions) {
                        session = app->sessions;
                        while (session) {
                            if (session->window) {
                                WindowToFront(session->window);
                            }
                            session = session->next;
                        }
                    }
                    result = TRUE;
                    break;
                case CXCMD_DISAPPEAR:
                    /* Hide/iconify application */
                    TTX_Iconify(app, TRUE);
                    result = TRUE;
                    break;
                case CXCMD_KILL:
                    app->running = FALSE;
                    result = TRUE;
                    break;
                case CXCMD_UNIQUE:
                    /* Another instance tried to start - show ourselves */
                    TTX_Iconify(app, FALSE);
                    /* Bring all windows to front */
                    if (app->sessions) {
                        session = app->sessions;
                        while (session) {
                            if (session->window) {
                                WindowToFront(session->window);
                            }
                            session = session->next;
                        }
                    }
                    result = TRUE;
                    break;
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
    
    return result;
}

/* Handle Intuition message */
BOOL TTX_HandleIntuitionMessage(struct TTXApplication *app, struct IntuiMessage *imsg)
{
    struct Session *session = NULL;
    BOOL result = FALSE;
    
    if (!app || !imsg) {
        Printf("[EVENT] TTX_HandleIntuitionMessage: FAIL (app=%lx, imsg=%lx)\n", (ULONG)app, (ULONG)imsg);
        return FALSE;
    }
    
    Printf("[EVENT] TTX_HandleIntuitionMessage: Class=0x%08lx, Code=0x%04x\n",
           (ULONG)imsg->Class, (unsigned int)imsg->Code);
    
    /* Find session for this window */
    session = app->sessions;
    while (session) {
        if (session->window == imsg->IDCMPWindow) {
            Printf("[EVENT] Found session for window %lx\n", (ULONG)imsg->IDCMPWindow);
            break;
        }
        session = session->next;
    }
    
    if (!session) {
        Printf("[EVENT] No session found for window %lx\n", (ULONG)imsg->IDCMPWindow);
        return FALSE;
    }
    
            switch (imsg->Class) {
                case IDCMP_MENUPICK:
                    /* Handle menu selection - process NextSelect chain for submenus */
                    /* According to Intuition guide: menu code is in msg->Code field */
                    /* Menu number format: 16-bit with 5 bits menu, 6 bits item, 5 bits sub-item */
                    {
                        /* Use Code field as per Intuition guide, but if it's 0, try Qualifier (for gadtools) */
                        UWORD menuCode = (UWORD)imsg->Code;
                        if (menuCode == 0 && imsg->Qualifier != 0) {
                            /* For gadtools menus, code might be in Qualifier */
                            menuCode = (UWORD)imsg->Qualifier;
                            Printf("[EVENT] IDCMP_MENUPICK: Code was 0, using Qualifier=0x%04x\n", (unsigned int)menuCode);
                        } else {
                            Printf("[EVENT] IDCMP_MENUPICK: Code=0x%04x, Qualifier=0x%04x\n", 
                                   (unsigned int)imsg->Code, (unsigned int)imsg->Qualifier);
                        }
                        
                        /* Check for MENUNULL first */
                        if (menuCode == MENUNULL || menuCode == 0xFFFF) {
                            Printf("[EVENT] IDCMP_MENUPICK: MENUNULL (0x%04x), ignoring\n", (unsigned int)menuCode);
                            result = TRUE;
                            break;
                        }
                        
                        /* Process menu selection chain as per Intuition guide */
                        while (menuCode != MENUNULL && menuCode != 0xFFFF) {
                            UWORD menuNumber = 0;
                            UWORD itemNumber = 0;
                            struct MenuItem *item = NULL;
                            
                            /* Get the menu item using ItemAddress (as per Intuition guide) */
                            if (session->window && session->window->MenuStrip) {
                                item = ItemAddress(session->window->MenuStrip, menuCode);
                                if (!item) {
                                    Printf("[EVENT] IDCMP_MENUPICK: ItemAddress returned NULL for code=0x%04x\n", (unsigned int)menuCode);
                                    break;
                                }
                                
                                /* Get menu/item numbers from UserData if available (our custom storage) */
                                if (item && GTMENUITEM_USERDATA(item)) {
                                    ULONG userData = (ULONG)GTMENUITEM_USERDATA(item);
                                    menuNumber = (userData >> 8) & 0xFF;
                                    itemNumber = userData & 0xFF;
                                    Printf("[EVENT] IDCMP_MENUPICK: menuCode=0x%04x, got from UserData: menu=%u, item=%u\n", 
                                           (unsigned int)menuCode, menuNumber, itemNumber);
                                } else {
                                    /* Fallback: Use Intuition macros if available, otherwise extract manually */
                                    /* Menu number format: 5 bits menu (bits 11-15), 6 bits item (bits 5-10), 5 bits sub (bits 0-4) */
                                    /* But we only care about menu and item for now */
                                    menuNumber = (menuCode >> 11) & 0x1F;  /* 5 bits for menu */
                                    itemNumber = (menuCode >> 5) & 0x3F;   /* 6 bits for item */
                                    Printf("[EVENT] IDCMP_MENUPICK: menuCode=0x%04x, extracted menu=%u, item=%u (no UserData)\n", 
                                           (unsigned int)menuCode, menuNumber, itemNumber);
                                }
                            } else {
                                Printf("[EVENT] IDCMP_MENUPICK: no window or MenuStrip\n");
                                break;
                            }
                            
                            /* Handle this menu item */
                            if (!TTX_HandleMenuPick(app, session, menuNumber, itemNumber)) {
                                /* Command failed or was cancelled - stop processing chain */
                                Printf("[EVENT] IDCMP_MENUPICK: command failed, stopping chain\n");
                                break;
                            }
                            
                            /* Get next item in chain (for submenus) - as per Intuition guide */
                            if (item) {
                                menuCode = item->NextSelect;
                                Printf("[EVENT] IDCMP_MENUPICK: next in chain=0x%04x\n", (unsigned int)menuCode);
                            } else {
                                menuCode = MENUNULL;
                            }
                        }
                    }
                    result = TRUE;
                    break;
                    
                case IDCMP_CLOSEWINDOW:
            TTX_DestroySession(app, session);
            result = TRUE;
                    break;
                    
                case IDCMP_MOUSEBUTTONS:
            /* Handle mouse clicks to position cursor */
            if (imsg->Code == IECODE_LBUTTON && !(imsg->Code & 0x80)) {
                /* Left mouse button press (not release) */
                ULONG newCursorX = 0;
                ULONG newCursorY = 0;
                
                MouseToCursor(session->buffer, session->window, imsg->MouseX, imsg->MouseY, &newCursorX, &newCursorY);
                
                /* Update cursor position */
                if (session->buffer && session->buffer->lines && newCursorY < session->buffer->lineCount) {
                    session->buffer->cursorY = newCursorY;
                    if (newCursorX <= session->buffer->lines[newCursorY].length) {
                        session->buffer->cursorX = newCursorX;
                    } else {
                        session->buffer->cursorX = session->buffer->lines[newCursorY].length;
                    }
                }
                
                CalculateMaxScroll(session->buffer, session->window);
                ScrollToCursor(session->buffer, session->window);
                UpdateScrollBars(session);
                RenderText(session->window, session->buffer);
                UpdateCursor(session->window, session->buffer);
                result = TRUE;
            }
                    break;
                    
                case IDCMP_VANILLAKEY:
                case IDCMP_RAWKEY:
            if (session->buffer && !session->docState.readOnly) {
                UBYTE keyCode = imsg->Code;
                ULONG qualifiers = imsg->Qualifier;
                struct InputEvent ievent;
                UBYTE charBuffer[10];
                WORD chars = 0;
                struct KeyMap *keymap = NULL;
                BOOL processed = FALSE;
                
                /* Handle VANILLAKEY first - these are already converted by Intuition */
                if (imsg->Class == IDCMP_VANILLAKEY) {
                    /* Filter out arrow keys that might come as VANILLAKEY */
                    /* Arrow keys should be handled as RAWKEY, but some systems might send them as VANILLAKEY */
                    if (keyCode == 0x1C || keyCode == 0x1D || keyCode == 0x1E || keyCode == 0x1F) {
                        /* Arrow keys as VANILLAKEY - ignore, they should come as RAWKEY */
                        processed = TRUE;
                    } else if ((keyCode >= 27 && keyCode <= 126) || (keyCode >= 128 && keyCode <= 255)) {
                        /* Printable character - insert directly */
                        InsertChar(session->buffer, keyCode, session->cleanupStack);
                        CalculateMaxScroll(session->buffer, session->window);
                        ScrollToCursor(session->buffer, session->window);
                        UpdateScrollBars(session);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x08) {
                        /* Backspace */
                        DeleteChar(session->buffer, session->cleanupStack);
                        CalculateMaxScroll(session->buffer, session->window);
                        ScrollToCursor(session->buffer, session->window);
                        UpdateScrollBars(session);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x7F) {
                        /* Delete key (forward delete) */
                        DeleteForward(session->buffer, session->cleanupStack);
                        CalculateMaxScroll(session->buffer, session->window);
                        ScrollToCursor(session->buffer, session->window);
                        UpdateScrollBars(session);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x0A || keyCode == 0x0D) {
                        /* Enter/Return */
                        InsertNewline(session->buffer, session->cleanupStack);
                        CalculateMaxScroll(session->buffer, session->window);
                        ScrollToCursor(session->buffer, session->window);
                        UpdateScrollBars(session);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x1B) {
                        /* Escape = Quit (close window) */
                        TTX_DestroySession(app, session);
                        result = TRUE;
                        break;
                    } else if (keyCode == 0x45 && (qualifiers & IEQUALIFIER_CONTROL)) {
                        /* Ctrl+E = Save */
                        if (session->docState.fileName && session->buffer && app->cleanupStack) {
                            if (SaveFile(session->docState.fileName, session->buffer, app->cleanupStack)) {
                                session->docState.modified = FALSE;
                                session->buffer->modified = FALSE;
                            }
                        }
                        processed = TRUE;
                    }
                } else if (imsg->Class == IDCMP_RAWKEY) {
                    /* RAWKEY needs conversion via keymap */
                    /* Filter out modifier keys and mouse buttons */
                    /* Check for key release (bit 7 set) */
                    if (keyCode & 0x80) {
                        /* Key release - ignore */
                        result = TRUE;
                        break;
                    }
                    
                    /* Filter out modifier keys */
                    if (keyCode >= 0x60 && keyCode <= 0x67) {
                        /* Shift, Ctrl, Alt, etc. - ignore */
                        result = TRUE;
                        break;
                    }
                    
                    /* Filter out mouse buttons */
                    if (keyCode >= 0x68 && keyCode <= 0x6A) {
                        /* Mouse buttons - ignore */
                        result = TRUE;
                        break;
                    }
                    
                    /* Handle special keys first (before keymap conversion) */
                    /* Arrow keys: 0x4F=Left, 0x4E=Right, 0x4C=Up, 0x4D=Down (Amiga raw key codes) */
                    if (keyCode == 0x4F) {
                        /* Left arrow */
                        if (session->buffer && session->buffer->lines) {
                            if (session->buffer->cursorX > 0) {
                                session->buffer->cursorX--;
                            } else if (session->buffer->cursorY > 0 && session->buffer->cursorY - 1 < session->buffer->lineCount) {
                                session->buffer->cursorY--;
                                session->buffer->cursorX = session->buffer->lines[session->buffer->cursorY].length;
                            }
                        }
                        ScrollToCursor(session->buffer, session->window);
                        UpdateScrollBars(session);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x4E) {
                        /* Right arrow */
                        if (session->buffer && session->buffer->lines && session->buffer->cursorY < session->buffer->lineCount) {
                            if (session->buffer->cursorX < session->buffer->lines[session->buffer->cursorY].length) {
                                session->buffer->cursorX++;
                            } else if (session->buffer->cursorY < session->buffer->lineCount - 1) {
                                session->buffer->cursorY++;
                                session->buffer->cursorX = 0;
                            }
                        }
                        ScrollToCursor(session->buffer, session->window);
                        UpdateScrollBars(session);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x4C) {
                        /* Up arrow */
                        if (session->buffer && session->buffer->lines && session->buffer->cursorY > 0) {
                            session->buffer->cursorY--;
                            if (session->buffer->cursorY < session->buffer->lineCount && session->buffer->cursorX > session->buffer->lines[session->buffer->cursorY].length) {
                                session->buffer->cursorX = session->buffer->lines[session->buffer->cursorY].length;
                            }
                        }
                        ScrollToCursor(session->buffer, session->window);
                        UpdateScrollBars(session);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x4D) {
                        /* Down arrow */
                        if (session->buffer && session->buffer->lines && session->buffer->cursorY < session->buffer->lineCount - 1) {
                            session->buffer->cursorY++;
                            if (session->buffer->cursorY < session->buffer->lineCount && session->buffer->cursorX > session->buffer->lines[session->buffer->cursorY].length) {
                                session->buffer->cursorX = session->buffer->lines[session->buffer->cursorY].length;
                            }
                        }
                        ScrollToCursor(session->buffer, session->window);
                        UpdateScrollBars(session);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x46) {
                        /* Delete key (raw key code) */
                        DeleteForward(session->buffer, session->cleanupStack);
                        ScrollToCursor(session->buffer, session->window);
                        UpdateScrollBars(session);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else {
                        /* Convert raw key to character using keymap */
                        /* Get default keymap for conversion */
                        if (KeymapBase) {
                            keymap = AskKeyMapDefault();
                        }
                        
                        if (keymap) {
                            /* MapRawKey requires an InputEvent structure */
                            /* Follow the API doc example from keymap.doc */
                            ievent.ie_Class = IECLASS_RAWKEY;
                            ievent.ie_SubClass = 0;
                            ievent.ie_Code = keyCode;
                            ievent.ie_Qualifier = qualifiers & ~(IEQUALIFIER_CAPSLOCK | IEQUALIFIER_RELATIVEMOUSE);
                            /* Recover dead key codes & qualifiers from IAddress */
                            /* As per API doc: ie.ie_EventAddress = (APTR *) *((ULONG *)im->IAddress); */
                            if (imsg->IAddress) {
                                ievent.ie_EventAddress = (APTR) *((ULONG *)imsg->IAddress);
                            } else {
                                ievent.ie_EventAddress = NULL;
                            }
                            
                            /* Use MapRawKey from keymap.library */
                            /* MapRawKey returns WORD: number of characters, or -1 for buffer overflow */
                            chars = MapRawKey(&ievent, charBuffer, sizeof(charBuffer) - 1, keymap);
                            if (chars > 0 && chars < (WORD)(sizeof(charBuffer) - 1)) {
                                /* Successfully converted - insert characters */
                                charBuffer[chars] = '\0';
                                /* Insert each character from the conversion */
                                {
                                    ULONG i = 0;
                                    for (i = 0; i < (ULONG)chars; i++) {
                                        if (charBuffer[i] >= 0x20 && charBuffer[i] < 0x7F) {
                                            /* Printable ASCII character */
                                            InsertChar(session->buffer, charBuffer[i], session->cleanupStack);
                                        } else if (charBuffer[i] == 0x0A || charBuffer[i] == 0x0D) {
                                            /* Newline */
                                            InsertNewline(session->buffer, session->cleanupStack);
                                        }
                                    }
                                }
                                CalculateMaxScroll(session->buffer, session->window);
                                ScrollToCursor(session->buffer, session->window);
                                UpdateScrollBars(session);
                                RenderText(session->window, session->buffer);
                                UpdateCursor(session->window, session->buffer);
                                processed = TRUE;
                            }
                            /* If chars == -1, buffer overflow occurred - ignore this key */
                            /* If chars == 0, no characters generated - ignore this key */
                        }
                    }
                }
                
                if (processed) {
                    session->docState.modified = session->buffer->modified;
                }
            }
            result = TRUE;
                    break;
                    
                case IDCMP_REFRESHWINDOW:
            if (session->buffer) {
                BeginRefresh(session->window);
                RenderText(session->window, session->buffer);
                UpdateCursor(session->window, session->buffer);
                EndRefresh(session->window, TRUE);
            }
            result = TRUE;
                    break;
                    
                case IDCMP_NEWSIZE:
                case IDCMP_CHANGEWINDOW:
                    /* Window was resized or moved - refresh super bitmap if using ScrollLayer */
                    if (session->buffer && session->buffer->superBitMap && session->window->RPort->Layer) {
                        /* Copy super bitmap to refresh display after window move/resize */
                        /* This is required when using super bitmaps with ScrollLayer */
                        if (GfxBase && ((struct Library *)GfxBase)->lib_Version >= 39) {
                            LockLayerRom(session->window->RPort->Layer);
                            CopySBitMap(session->window->RPort->Layer);
                            UnlockLayerRom(session->window->RPort->Layer);
                        }
                        /* Force full redraw on next render */
                        session->buffer->needsFullRedraw = TRUE;
                    }
                    /* Recreate super bitmap if window size changed significantly */
                    if (session->buffer && session->window) {
                        ULONG newSuperWidth = session->window->Width * 2;
                        ULONG newSuperHeight = session->window->Height * 2;
                        if (newSuperWidth != session->buffer->superWidth || 
                            newSuperHeight != session->buffer->superHeight) {
                            FreeSuperBitMap(session->buffer);
                            CreateSuperBitMap(session->buffer, session->window);
                        }
                    }
                    /* Recalculate max scroll values and update scroll bars */
                    if (session->buffer) {
                        CalculateMaxScroll(session->buffer, session->window);
                        ScrollToCursor(session->buffer, session->window);  /* ScrollToCursor may change scroll position */
                        UpdateScrollBars(session);  /* Update scroll bars after scroll position may have changed */
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                    }
                    result = TRUE;
                    break;
                    
                case IDCMP_IDCMPUPDATE:
                    /* Handle prop gadget updates (scroll bar movement) */
                    {
                        ULONG gadgetID = (ULONG)imsg->Code;
                        ULONG newScrollY = 0;
                        ULONG newScrollX = 0;
                        ULONG scaledValue = 0;
                        struct Gadget *gadget = NULL;
                        
                        if (gadgetID == GID_VERT_PROP) {
                            /* Vertical scroll bar moved */
                            gadget = session->vertPropGadget;
                            if (gadget && session->buffer) {
                                GetAttr(PGA_Top, gadget, &scaledValue);
                                /* Convert scaled value back to unscaled */
                                newScrollY = scaledValue;
                                if (session->buffer->scrollYShift > 0) {
                                    newScrollY <<= session->buffer->scrollYShift;
                                }
                                /* Clamp to valid range */
                                if (newScrollY > session->buffer->maxScrollY) {
                                    newScrollY = session->buffer->maxScrollY;
                                }
                                if (newScrollY != session->buffer->scrollY) {
                                    session->buffer->scrollY = newScrollY;
                                    /* Recalculate max scroll in case it changed */
                                    CalculateMaxScroll(session->buffer, session->window);
                                    RenderText(session->window, session->buffer);
                                    UpdateCursor(session->window, session->buffer);
                                }
                            }
                            result = TRUE;
                        } else if (gadgetID == GID_HORIZ_PROP) {
                            /* Horizontal scroll bar moved */
                            Printf("[EVENT] IDCMP_IDCMPUPDATE: horizontal scroll bar (gadgetID=%lu)\n", gadgetID);
                            gadget = session->horizPropGadget;
                            if (gadget && session->buffer) {
                                GetAttr(PGA_Top, gadget, &scaledValue);
                                Printf("[EVENT] IDCMP_IDCMPUPDATE: horizontal scroll scaledValue=%lu, scrollXShift=%d\n", 
                                       scaledValue, session->buffer->scrollXShift);
                                /* Convert scaled value back to unscaled */
                                newScrollX = scaledValue;
                                if (session->buffer->scrollXShift > 0) {
                                    newScrollX <<= session->buffer->scrollXShift;
                                }
                                Printf("[EVENT] IDCMP_IDCMPUPDATE: horizontal scroll newScrollX=%lu, current scrollX=%lu, maxScrollX=%lu\n",
                                       newScrollX, session->buffer->scrollX, session->buffer->maxScrollX);
                                /* Clamp to valid range */
                                if (newScrollX > session->buffer->maxScrollX) {
                                    newScrollX = session->buffer->maxScrollX;
                                }
                                if (newScrollX != session->buffer->scrollX) {
                                    session->buffer->scrollX = newScrollX;
                                    Printf("[EVENT] IDCMP_IDCMPUPDATE: horizontal scroll updated to %lu\n", newScrollX);
                                    /* Recalculate max scroll in case it changed */
                                    CalculateMaxScroll(session->buffer, session->window);
                                    RenderText(session->window, session->buffer);
                                    UpdateCursor(session->window, session->buffer);
                                } else {
                                    Printf("[EVENT] IDCMP_IDCMPUPDATE: horizontal scroll no change needed\n");
                                }
                            } else {
                                Printf("[EVENT] IDCMP_IDCMPUPDATE: horizontal scroll FAIL (gadget=%lx, buffer=%lx)\n",
                                       (ULONG)gadget, (ULONG)(session ? session->buffer : NULL));
                            }
                            result = TRUE;
                        } else {
                            Printf("[EVENT] IDCMP_IDCMPUPDATE: unknown gadgetID=%lu\n", gadgetID);
                        }
                    }
                    break;
                    
                default:
                    break;
            }
            
    return result;
}

/* Main event loop */
VOID TTX_EventLoop(struct TTXApplication *app)
{
    struct Message *msg = NULL;
    struct IntuiMessage *imsg = NULL;
    ULONG signals = 0;
    struct Session *session = NULL;
    struct Session *nextSession = NULL;
    struct TTXMessage *ttxMsg = NULL;
    
    if (!app) {
        return;
    }
    
    /* Build signal mask */
    app->sigmask = (1UL << app->appPort->mp_SigBit);
    if (app->brokerPort) {
        app->sigmask |= (1UL << app->brokerPort->mp_SigBit);
    }
    if (app->appIconPort) {
        app->sigmask |= (1UL << app->appIconPort->mp_SigBit);
    }
    if (app->sessions) {
        session = app->sessions;
        while (session) {
            if (session->window) {
                app->sigmask |= (1UL << session->window->UserPort->mp_SigBit);
            }
            session = session->next;
        }
    }
    app->sigmask |= SIGBREAKF_CTRL_C;
    
    app->running = TRUE;
    
    while (app->running) {
        /* Process deferred iconification first */
        if (app->iconifyDeferred) {
            app->iconifyDeferred = FALSE;
            TTX_DoIconify(app, app->iconifyState);
            /* Rebuild signal mask after iconification (windows may have changed) */
            app->sigmask = (1UL << app->appPort->mp_SigBit);
            if (app->brokerPort) {
                app->sigmask |= (1UL << app->brokerPort->mp_SigBit);
            }
            if (app->appIconPort) {
                app->sigmask |= (1UL << app->appIconPort->mp_SigBit);
            }
            if (app->sessions) {
                session = app->sessions;
                while (session) {
                    if (session->window) {
                        app->sigmask |= (1UL << session->window->UserPort->mp_SigBit);
                    }
                    session = session->next;
                }
            }
            app->sigmask |= SIGBREAKF_CTRL_C;
        }
        
        Printf("[EVENT] Waiting for signals (mask=0x%08lx)\n", app->sigmask);
        signals = Wait(app->sigmask);
        Printf("[EVENT] Wait returned: signals=0x%08lx\n", signals);
        
        /* Check for break signal */
        if (signals & SIGBREAKF_CTRL_C) {
            Printf("[EVENT] Break signal received, exiting\n");
            app->running = FALSE;
            break;
        }
        
        /* Check broker port (commodity messages from Exchange) */
        if (app->brokerPort && (signals & (1UL << app->brokerPort->mp_SigBit))) {
            while ((msg = GetMsg(app->brokerPort)) != NULL) {
                TTX_HandleCommodityMessage(app, msg);
            }
        }
        
        /* Check app icon port (app icon messages) */
        if (app->appIconPort && (signals & (1UL << app->appIconPort->mp_SigBit))) {
            TTX_ProcessAppIcon(app);
        }
        
        /* Check application port (inter-instance messages) */
        if (signals & (1UL << app->appPort->mp_SigBit)) {
            while ((msg = GetMsg(app->appPort)) != NULL) {
                ttxMsg = (struct TTXMessage *)msg;
                switch (ttxMsg->type) {
                    case TTX_MSG_OPEN_FILE:
                        if (ttxMsg->fileName) {
                            TTX_CreateSession(app, ttxMsg->fileName);
                        }
                        break;
                    case TTX_MSG_OPEN_NEW:
                        TTX_CreateSession(app, NULL);
                        break;
                    case TTX_MSG_QUIT:
                        app->running = FALSE;
                        break;
                    default:
                        break;
                }
                /* According to Exec message docs: ALL messages must be replied to with ReplyMsg() */
                /* For one-way messages (mn_ReplyPort=NULL), ReplyMsg() does nothing */
                /* But we still call it, then free the message since no reply will come back */
                ReplyMsg(msg);
                /* Messages from other instances were allocated by THEIR cleanup stack, not ours */
                /* So we must free them directly with FreeVec, not through our cleanup stack */
                if (ttxMsg->fileName) {
                    FreeVec(ttxMsg->fileName);
                }
                FreeVec(ttxMsg);
            }
        }
        
        /* Check session windows */
        if (app->sessions) {
            session = app->sessions;
            while (session) {
                /* Save next pointer in case session is destroyed */
                nextSession = session->next;
                if (session->window) {
                    ULONG windowSignal = (1UL << session->window->UserPort->mp_SigBit);
                    if (signals & windowSignal) {
                        Printf("[EVENT] Window signal received: sigbit=%lu, window=%lx\n", 
                               session->window->UserPort->mp_SigBit, (ULONG)session->window);
                    while ((imsg = (struct IntuiMessage *)GetMsg(session->window->UserPort)) != NULL) {
                            Printf("[EVENT] Got IntuiMessage: Class=0x%08lx, Code=0x%04x, Qualifier=0x%04x\n",
                                   (ULONG)imsg->Class, (unsigned int)imsg->Code, (unsigned int)imsg->Qualifier);
                        TTX_HandleIntuitionMessage(app, imsg);
                        ReplyMsg((struct Message *)imsg);
                        }
                    }
                }
                session = nextSession;
            }
        } else {
            Printf("[EVENT] No sessions to check\n");
        }
        
        /* Exit if no sessions left (unless in background mode) */
        if (app->sessionCount == 0 && !app->backgroundMode) {
            app->running = FALSE;
        }
    }
}

/* Initialize application */
BOOL TTX_Init(struct TTXApplication *app)
{
    ULONG i = 0;
    
    Printf("[INIT] TTX_Init: START\n");
    if (!app) {
        Printf("[INIT] TTX_Init: FAIL (app=NULL)\n");
        return FALSE;
    }
    
    /* Clear application structure */
    for (i = 0; i < sizeof(struct TTXApplication); i++) {
        ((UBYTE *)app)[i] = 0;
    }
    
    /* Use default cleanup stack created by Seiso auto-init */
    app->cleanupStack = GetCleanupStack();
    if (!app->cleanupStack) {
        Printf("[INIT] TTX_Init: FAIL (GetDefaultStack failed)\n");
        return FALSE;
    }
    g_ttxStack = app->cleanupStack;
    Printf("[INIT] TTX_Init: using default cleanup stack\n");
    
    /* Initialize libraries */
    if (!TTX_InitLibraries(app->cleanupStack)) {
        /* Don't delete default stack - it will be cleaned up by _STD_SeisoCleanup() */
        app->cleanupStack = NULL;
        g_ttxStack = NULL;
        return FALSE;
    }
    
    /* Setup message port */
    if (!TTX_SetupMessagePort(app)) {
        /* Don't delete default stack - it will be cleaned up by _STD_SeisoCleanup() */
        app->cleanupStack = NULL;
        g_ttxStack = NULL;
        return FALSE;
    }
    
    /* Setup commodity if available */
    if (CxBase) {
        if (!TTX_SetupCommodity(app)) {
            /* Commodity setup failed, but continue anyway */
            /* The app can still work without commodities */
        }
    }
    
    /* Setup app icon support (for iconification) */
    if (WorkbenchBase && IconBase) {
        if (!TTX_SetupAppIcon(app)) {
            /* App icon setup failed, but continue anyway */
            /* The app can still work without iconification */
        }
    }
    
    Printf("[INIT] TTX_Init: SUCCESS\n");
    return TRUE;
}

/* Cleanup application */
VOID TTX_Cleanup(struct TTXApplication *app)
{
    struct Message *msg = NULL;
    
    Printf("[CLEANUP] TTX_Cleanup: START\n");
    if (!app) {
        Printf("[CLEANUP] TTX_Cleanup: DONE (app=NULL)\n");
        return;
    }
    
    /* Remove app icon before destroying sessions */
    TTX_RemoveAppIcon(app);
    
    /* Destroy all sessions */
    Printf("[CLEANUP] TTX_Cleanup: destroying %lu sessions\n", app->sessionCount);
    while (app->sessions) {
        TTX_DestroySession(app, app->sessions);
    }
    
    /* Clean up any pending messages from app port before stack cleanup */
    /* Note: The port itself is tracked on cleanup stack and will be cleaned up automatically */
    /* According to Exec message docs: ALL messages received via GetMsg() must be replied to with ReplyMsg() */
    /* The sender is responsible for freeing the message after receiving the reply */
    if (app->appPort) {
        Printf("[CLEANUP] TTX_Cleanup: cleaning pending messages from appPort\n");
        while ((msg = GetMsg(app->appPort)) != NULL) {
            /* Reply to all messages - sender will free them after receiving reply */
            ReplyMsg(msg);
        }
        Printf("[CLEANUP] TTX_Cleanup: appPort messages cleaned\n");
        /* Port cleanup (RemPort + DeleteMsgPort) is handled by cleanup stack via cleanupPort() */
        /* We just clear the pointer so we don't try to use it after cleanup */
        app->appPort = NULL;
    }
    
    /* Broker is tracked by Seiso cleanup stack and will be cleaned up automatically */
    /* Just clear our reference */
    
    /* Clean up any pending messages from broker port before stack cleanup */
    /* This must be done AFTER the broker is deleted */
    /* According to commodities.library docs, messages sent to a commodity program
     * from a sender object must be sent back using ReplyMsg() */
    if (app->brokerPort) {
        Printf("[CLEANUP] TTX_Cleanup: cleaning pending messages from brokerPort\n");
        /* Get and reply to all pending messages */
        while ((msg = GetMsg(app->brokerPort)) != NULL) {
            /* Reply to commodity messages (required by commodities.library) */
            ReplyMsg(msg);
        }
        Printf("[CLEANUP] TTX_Cleanup: brokerPort messages cleaned\n");
    }
    
    /* Free ReadArgs BEFORE cleanup stack deletion */
    /* ReadArgs must be freed after we're done using the parsed arguments */
    /* This should have been done earlier, but ensure it's done here as a safety net */
    /* Note: ReadArgs is tracked by the cleanup stack, so it will be freed automatically,
     * but we should free it explicitly before cleanup to avoid issues */
    
    /* DO NOT clear broker/brokerPort pointers yet - cleanup stack needs them */
    /* The cleanup stack will properly clean up the broker and port */
    /* We'll clear the pointers AFTER the stack cleanup is complete */
    
    /* DO NOT flush or delete the default cleanup stack here */
    /* The default stack is owned by Seiso and will be properly cleaned up
     * by _STD_SeisoCleanup() after main() returns. This ensures proper
     * cleanup order and prevents memory corruption. */
    /* Libraries, ports, brokers, etc. are all cleaned up automatically in LIFO order
     * when _STD_SeisoCleanup() calls DeleteCleanupStack() on the default stack */
    /* Note: ReadArgs will be freed by the cleanup stack if still tracked */
    /* According to commodities.library docs, DeleteCxObjAll() will:
     * - Remove the broker from the list if it's linked
     * - Recursively delete all objects attached to it
     * This is the proper way to clean up when an application exits */
    /* Just clear our reference - don't flush or delete */
    app->cleanupStack = NULL;
    g_ttxStack = NULL;
    
    /* Clear application pointers - resources will be cleaned up by _STD_SeisoCleanup() */
    /* DeleteCxObjAll() is synchronous - broker is immediately removed from system */
    app->appPort = NULL;
    app->brokerPort = NULL;
    app->broker = NULL;
    
    /* DO NOT clear global library base pointers here */
    /* The cleanup stack will close the libraries when _STD_SeisoCleanup() runs */
    /* Clearing them now would cause crashes when cleanup functions try to use them */
    /* The libraries must remain valid until the cleanup stack closes them */
    /* After _STD_SeisoCleanup() completes, the libraries will be closed and
     * the pointers will naturally be invalid, but we don't need to explicitly
     * set them to NULL here */
    
    Printf("[CLEANUP] TTX_Cleanup: DONE\n");
}

/* Calculate maximum scroll values based on buffer content and window size */
VOID CalculateMaxScroll(struct TextBuffer *buffer, struct Window *window)
{
    ULONG i = 0;
    ULONG maxLineLen = 0;
    ULONG lineHeight = 0;
    ULONG visibleLines = 0;
    
    if (!buffer || !window || !window->RPort) {
        return;
    }
    
    /* Calculate visible lines (pageH) */
    lineHeight = GetLineHeight(window->RPort);
    if (lineHeight > 0) {
        visibleLines = (window->Height - window->BorderTop - window->BorderBottom) / lineHeight;
        buffer->pageH = visibleLines;
    } else {
        buffer->pageH = 0;
    }
    
    /* Calculate maximum vertical scroll (maxScrollY) */
    if (buffer->lineCount > buffer->pageH) {
        buffer->maxScrollY = buffer->lineCount - buffer->pageH;
    } else {
        buffer->maxScrollY = 0;
    }
    
    /* Calculate visible characters per line (pageW) */
    /* This must be calculated here so scroll bars can use it during initialization */
    {
        ULONG charWidth = 0;
        ULONG textStartX = 0;
        ULONG textEndX = 0;
        ULONG textWidth = 0;
        ULONG maxChars = 0;
        
        charWidth = GetCharWidth(window->RPort, 'M');
        textStartX = window->BorderLeft + buffer->leftMargin + 1;
        textEndX = window->Width - (window->BorderRight + 1);
        
        if (charWidth > 0) {
            /* Calculate available text width in pixels */
            if (textEndX >= textStartX) {
                textWidth = textEndX - textStartX + 1;
            } else {
                textWidth = 0;
            }
            /* Convert to characters, subtract 1 for safety */
            maxChars = textWidth / charWidth;
            if (maxChars > 0) {
                maxChars--;
            }
            buffer->pageW = maxChars;
        } else {
            buffer->pageW = 0;
        }
    }
    
    /* Calculate maximum line length for horizontal scrolling */
    maxLineLen = 0;
    if (buffer->lines && buffer->lineCount > 0) {
        for (i = 0; i < buffer->lineCount; i++) {
            if (buffer->lines[i].length > maxLineLen) {
                maxLineLen = buffer->lines[i].length;
            }
        }
    }
    
    /* Calculate maximum horizontal scroll (maxScrollX) */
    if (maxLineLen > buffer->pageW) {
        buffer->maxScrollX = maxLineLen - buffer->pageW;
    } else {
        buffer->maxScrollX = 0;
    }
    
    /* Clamp current scroll positions to valid ranges */
    if (buffer->scrollY > buffer->maxScrollY) {
        buffer->scrollY = buffer->maxScrollY;
    }
    if (buffer->scrollX > buffer->maxScrollX) {
        buffer->scrollX = buffer->maxScrollX;
    }
}

/* Update scroll bar prop gadgets to reflect current scroll position */
/* Uses scroller pattern: total = total content size, visible = viewport size, top = scroll position */
VOID UpdateScrollBars(struct Session *session)
{
    struct Gadget *gadget = NULL;
    ULONG total = 0;
    ULONG visible = 0;
    ULONG top = 0;
    ULONG maxValue = 0xFFFF;  /* Maximum value for propgclass (16-bit) */
    ULONG scaledTotal = 0;
    ULONG scaledVisible = 0;
    ULONG scaledTop = 0;
    SHORT shift = 0;
    
    if (!session || !session->buffer || !session->window) {
        return;
    }
    
    /* Update vertical scroll bar */
    gadget = session->vertPropGadget;
    if (gadget) {
        /* For scroller: total = total lines, visible = visible lines, top = scroll position */
        total = session->buffer->lineCount;
        visible = session->buffer->pageH;
        top = session->buffer->scrollY;
        
        /* Scale down if total exceeds propgclass limit (0xFFFF) */
        scaledTotal = total;
        scaledVisible = visible;
        scaledTop = top;
        shift = 0;
        
        while (scaledTotal > maxValue) {
            scaledTotal >>= 1;
            shift++;
        }
        
        if (shift > 0) {
            scaledVisible >>= shift;
            scaledTop >>= shift;
        }
        
        /* Store shift factor for converting back when reading scroll position */
        session->buffer->scrollYShift = shift;
        
        /* Ensure visible doesn't exceed total */
        if (scaledVisible > scaledTotal) {
            scaledVisible = scaledTotal;
        }
        
        /* Ensure top is within valid range */
        /* For scroller: top can be at most (total - visible) */
        if (scaledTotal > scaledVisible) {
            ULONG maxTop = scaledTotal - scaledVisible;
            if (scaledTop > maxTop) {
                scaledTop = maxTop;
            }
        } else {
            /* All content is visible, top should be 0 */
            scaledTop = 0;
        }
        
        SetGadgetAttrs(gadget, session->window, NULL,
            PGA_Total, scaledTotal,
            PGA_Visible, scaledVisible,
            PGA_Top, scaledTop,
            TAG_DONE);
    }
    
    /* Update horizontal scroll bar */
    gadget = session->horizPropGadget;
    if (gadget) {
        /* Calculate maximum line length for horizontal scrolling */
        ULONG maxLineLen = 0;
        ULONG i = 0;
        
        if (session->buffer->lines && session->buffer->lineCount > 0) {
            for (i = 0; i < session->buffer->lineCount; i++) {
                if (session->buffer->lines[i].length > maxLineLen) {
                    maxLineLen = session->buffer->lines[i].length;
                }
            }
        }
        
        /* For scroller: total = max line length, visible = visible characters, top = scroll position */
        total = maxLineLen;
        visible = session->buffer->pageW;
        top = session->buffer->scrollX;
        
        /* Scale down if total exceeds propgclass limit (0xFFFF) */
        scaledTotal = total;
        scaledVisible = visible;
        scaledTop = top;
        shift = 0;
        
        while (scaledTotal > maxValue) {
            scaledTotal >>= 1;
            shift++;
        }
        
        if (shift > 0) {
            scaledVisible >>= shift;
            scaledTop >>= shift;
        }
        
        /* Store shift factor for converting back when reading scroll position */
        session->buffer->scrollXShift = shift;
        
        /* Ensure visible doesn't exceed total */
        if (scaledVisible > scaledTotal) {
            scaledVisible = scaledTotal;
        }
        
        /* Ensure top is within valid range */
        /* For scroller: top can be at most (total - visible) */
        if (scaledTotal > scaledVisible) {
            ULONG maxTop = scaledTotal - scaledVisible;
            if (scaledTop > maxTop) {
                scaledTop = maxTop;
            }
        } else {
            /* All content is visible, top should be 0 */
            scaledTop = 0;
        }
        
        SetGadgetAttrs(gadget, session->window, NULL,
            PGA_Total, scaledTotal,
            PGA_Visible, scaledVisible,
            PGA_Top, scaledTop,
            TAG_DONE);
    }
}

/* Show usage information */
VOID TTX_ShowUsage(VOID)
{
    Printf("Usage: TTX {files} [STARTUP=<macro>] [WINDOW=<desc>] [PUBSCREEN=<name>]\n");
    Printf("            [SETTINGS=<file>] [DEFINITIONS=<file>] [NOWINDOW] [WAIT] [BACKGROUND] [UNLOAD]\n");
    Printf("\n");
    Printf("Options:\n");
    Printf("  FILES          Files to open (multiple allowed, supports patterns)\n");
    Printf("  STARTUP        ARexx macro to run for each document\n");
    Printf("  WINDOW         Window description: left/top/width/height/iconified left/iconified top/ICONIFIED/CLOSED\n");
    Printf("  PUBSCREEN      Public screen name to open on\n");
    Printf("  SETTINGS       Preferences file\n");
    Printf("  DEFINITIONS    Definition file\n");
    Printf("  NOWINDOW       Don't open default window\n");
    Printf("  WAIT           Wait for documents to close\n");
    Printf("  BACKGROUND     Stay resident in background\n");
    Printf("  UNLOAD         Unload from background mode\n");
    Printf("\n");
    Printf("Examples:\n");
    Printf("  TTX readme.txt\n");
    Printf("  TTX file1.c file2.c\n");
    Printf("  TTX #?.c\n");
    Printf("  TTX\n");
}

/* Main entry point */
int main(int argc, char *argv[])
{
    struct TTXApplication app;
    struct WBStartup *wbMsg = NULL;
    struct TTXArgs ttxArgs;
    struct RDArgs *rda = NULL;
    STRPTR *files = NULL;
    BOOL parseResult = FALSE;
    LONG result = RETURN_OK;
    
    /* Initialize application */
    if (!TTX_Init(&app)) {
        LONG errorCode = IoErr();
        if (errorCode != 0) {
            PrintFault(errorCode, "TTX");
        } else {
            PrintFault(ERROR_OBJECT_NOT_FOUND, "TTX");
        }
        return RETURN_FAIL;
    }
    
    /* Initialize args structure to zero */
    {
        ULONG j;
        for (j = 0; j < sizeof(struct TTXArgs); j++) {
            ((UBYTE *)&ttxArgs)[j] = 0;
        }
    }
    
    /* Parse arguments (command line or tooltypes) */
    if (argc > 0) {
        /* CLI launch - TurboText style */
        parseResult = TTX_ParseArguments(&ttxArgs, app.cleanupStack);
        rda = ttxArgs.rda;
    } else {
        /* Workbench launch - argv points to WBStartup structure */
        wbMsg = (struct WBStartup *)argv;
        /* For now, just check for files in WBStartup */
        if (wbMsg && wbMsg->sm_NumArgs > 1) {
            /* Files were dropped on icon */
            STRPTR fileName = NULL;
            struct WBArg *wbarg = &wbMsg->sm_ArgList[1];
            ULONG len = 0;
            char fullPath[512];
            
            /* Build full path */
            /* Clear IoErr() before dos.library path operations to ensure clean state */
            SetIoErr(0);
            fullPath[0] = '\0';
            if (wbarg->wa_Lock) {
                /* NameFromLock can fail - check result and clear error on failure */
                if (!NameFromLock(wbarg->wa_Lock, fullPath, sizeof(fullPath))) {
                    /* NameFromLock failed - clear error and use empty path */
                    /* This prevents dos.library from being left in undefined state */
                    SetIoErr(0);
                    fullPath[0] = '\0';
                } else {
                    /* NameFromLock succeeded - clear any error code that may have been set */
                    SetIoErr(0);
                }
            }
            /* AddPart can fail - check result and clear error on failure */
            if (fullPath[0] != '\0' || wbarg->wa_Lock == NULL) {
                /* Clear IoErr() before AddPart to ensure clean state */
                SetIoErr(0);
                if (!AddPart(fullPath, wbarg->wa_Name, sizeof(fullPath))) {
                    /* AddPart failed - clear error to prevent dos.library corruption */
                    SetIoErr(0);
                    fullPath[0] = '\0';
                } else {
                    /* AddPart succeeded - clear any error code that may have been set */
                    SetIoErr(0);
                }
            }
            
            len = 0;
            while (fullPath[len] != '\0' && len < sizeof(fullPath) - 1) {
                len++;
            }
            if (len > 0 && app.cleanupStack) {
                fileName = (STRPTR)allocVec(len + 1, MEMF_CLEAR);
                if (fileName) {
                    Printf("[INIT] main: allocated fileName=%lx\n", (ULONG)fileName);
                    CopyMem(fullPath, fileName, len);
                    fileName[len] = '\0';
                    ttxArgs.files = &fileName;
                    ttxArgs.files[1] = NULL;
                    parseResult = TRUE;
                }
            }
        } else {
            /* No files, check tooltypes */
            STRPTR fileName = NULL;
            parseResult = TTX_ParseToolTypes(&fileName, wbMsg, app.cleanupStack);
            if (parseResult && fileName) {
                ttxArgs.files = &fileName;
                ttxArgs.files[1] = NULL;
            } else {
                /* No tooltypes either - will create default window */
                parseResult = TRUE;
            }
        }
    }
    
    /* Handle UNLOAD first */
    if (parseResult && ttxArgs.unload) {
        /* TODO: Implement unload from background */
        if (rda && app.cleanupStack) {
            freeArgs(rda);
            rda = NULL; /* Prevent double-free */
        }
        TTX_Cleanup(&app);
        return RETURN_OK;
    }
    
    /* If BACKGROUND is set, don't open any sessions immediately - stay in background */
    if (parseResult && ttxArgs.background) {
        /* Background mode - don't open sessions, just stay loaded */
        app.backgroundMode = TRUE;
        /* Free parsed arguments */
        if (rda && app.cleanupStack) {
            freeArgs(rda);
            rda = NULL; /* Prevent double-free */
        }
        /* Run event loop even with no sessions (background mode) */
        TTX_EventLoop(&app);
        TTX_Cleanup(&app);
    return result;
}

    /* Not in background mode */
    app.backgroundMode = FALSE;
    
    /* Check if another instance is running - but only if we have files or NOWINDOW not set */
    /* NOTE: This check happens BEFORE we add our port to the system, so we won't find ourselves */
    if (parseResult) {
        if (ttxArgs.files && ttxArgs.files[0]) {
            /* We have files - check for existing instance */
            if (TTX_CheckExistingInstance(ttxArgs.files[0])) {
                /* Message sent to existing instance, exit */
                if (rda && app.cleanupStack) {
                    freeArgs(rda);
                    rda = NULL; /* Prevent double-free */
                }
                TTX_Cleanup(&app);
                return RETURN_OK;
            }
        } else if (!ttxArgs.noWindow) {
            /* No files but NOWINDOW not set - check for existing instance */
            if (TTX_CheckExistingInstance(NULL)) {
                if (rda && app.cleanupStack) {
                    freeArgs(rda);
                    rda = NULL; /* Prevent double-free */
                }
                TTX_Cleanup(&app);
                return RETURN_OK;
            }
        }
    } else {
        /* No arguments parsed - check for existing instance */
        if (TTX_CheckExistingInstance(NULL)) {
            TTX_Cleanup(&app);
            return RETURN_OK;
        }
    }
    
    /* No existing instance found - add our port to the system so others can find us */
    if (!TTX_AddMessagePort(&app)) {
        Printf("[INIT] main: WARN (TTX_AddMessagePort failed, continuing anyway)\n");
    }
    
    /* Create sessions for files (only if BACKGROUND not set) */
    if (parseResult && ttxArgs.files) {
        files = ttxArgs.files;
        while (*files) {
            if (!TTX_CreateSession(&app, *files)) {
                LONG errorCode = IoErr();
                if (errorCode != 0) {
                    PrintFault(errorCode, "TTX");
                    /* Clear error code after displaying */
                    SetIoErr(0);
                }
                result = RETURN_FAIL;
            }
            files++;
        }
    }
    
    /* Create default session if no files and NOWINDOW not specified (only if BACKGROUND not set) */
    if ((!parseResult || (!ttxArgs.files && !ttxArgs.noWindow)) && app.sessionCount == 0) {
        if (!TTX_CreateSession(&app, NULL)) {
            LONG errorCode = IoErr();
            if (errorCode != 0) {
                PrintFault(errorCode, "TTX");
                /* Clear error code after displaying */
                SetIoErr(0);
            }
            result = RETURN_FAIL;
        }
    }
    
    /* Free parsed arguments */
    if (rda && app.cleanupStack) {
        freeArgs(rda);
        rda = NULL; /* Prevent double-free */
    }
    
    /* Run event loop if we have sessions */
    if (app.sessionCount > 0) {
        TTX_EventLoop(&app);
    }
    
    /* Cleanup */
    TTX_Cleanup(&app);
    
    return result;
}
