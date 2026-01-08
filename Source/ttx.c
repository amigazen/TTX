/*
 * TTX - Text Editor for AmigaOS
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx.h"

/* Library base pointers */
extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;
extern struct Library *UtilityBase;
extern struct GfxBase *GfxBase;
extern struct Library *IconBase;
extern struct LocaleBase *LocaleBase;
extern struct RxsLib *RexxSysBase;
extern struct Library *WorkbenchBase;
extern struct Library *CxBase;
extern struct Library *KeymapBase;

/* Initialize required libraries */
BOOL TTX_InitLibraries(VOID)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39L);
    if (!IntuitionBase) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }
    
    UtilityBase = OpenLibrary("utility.library", 39L);
    if (!UtilityBase) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
        return FALSE;
    }
    
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39L);
    if (!GfxBase) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        CloseLibrary(UtilityBase);
        UtilityBase = NULL;
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
        return FALSE;
    }
    
    IconBase = OpenLibrary("icon.library", 39L);
    if (!IconBase) {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
        CloseLibrary(UtilityBase);
        UtilityBase = NULL;
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
        return FALSE;
    }
    
    CxBase = OpenLibrary("commodities.library", 0L);
    if (!CxBase) {
        /* Commodities is optional for single-instance, but preferred */
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
    }
    
    /* Open keymap.library for MapRawKey */
    KeymapBase = OpenLibrary("keymap.library", 0L);
    if (!KeymapBase) {
        /* Keymap.library is required for keyboard input conversion */
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        if (CxBase) {
            CloseLibrary(CxBase);
            CxBase = NULL;
        }
        CloseLibrary(IconBase);
        IconBase = NULL;
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
        CloseLibrary(UtilityBase);
        UtilityBase = NULL;
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
        return FALSE;
    }
    
    return TRUE;
}

/* Cleanup libraries */
VOID TTX_CleanupLibraries(VOID)
{
    if (KeymapBase) {
        CloseLibrary(KeymapBase);
        KeymapBase = NULL;
    }
    
    if (CxBase) {
        CloseLibrary(CxBase);
        CxBase = NULL;
    }
    
    if (IconBase) {
        CloseLibrary(IconBase);
        IconBase = NULL;
    }
    
    if (GfxBase) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
    }
    
    if (UtilityBase) {
        CloseLibrary(UtilityBase);
        UtilityBase = NULL;
    }
    
    if (IntuitionBase) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
    }
}

/* TurboText-style command line template */
static const char *ttxArgTemplate = 
    "FILES/M,"
    "STARTUP/K,"
    "WINDOW/K,"
    "PUBSCREEN/K,"
    "SETTINGS/K,"
    "DEFINITIONS/K,"
    "NOWINDOW/S,"
    "WAIT/S,"
    "BACKGROUND/S,"
    "UNLOAD/S";

/* Parse command-line arguments - TurboText style */
BOOL TTX_ParseArguments(struct TTXArgs *args)
{
    LONG argArray[10];
    ULONG i = 0;
    BOOL result = FALSE;
    
    if (!args) {
        return FALSE;
    }
    
    /* Clear args structure */
    for (i = 0; i < sizeof(struct TTXArgs); i++) {
        ((UBYTE *)args)[i] = 0;
    }
    
    /* Initialize arg array */
    for (i = 0; i < 10; i++) {
        argArray[i] = 0;
    }
    
    args->rda = ReadArgs(ttxArgTemplate, argArray, NULL);
    if (args->rda) {
        args->files = (STRPTR *)argArray[0];
        args->startup = (STRPTR)argArray[1];
        args->window = (STRPTR)argArray[2];
        args->pubscreen = (STRPTR)argArray[3];
        args->settings = (STRPTR)argArray[4];
        args->definitions = (STRPTR)argArray[5];
        args->noWindow = (BOOL)argArray[6];
        args->wait = (BOOL)argArray[7];
        args->background = (BOOL)argArray[8];
        args->unload = (BOOL)argArray[9];
        result = TRUE;
    } else {
        LONG errorCode = IoErr();
        if (errorCode != 0 && errorCode != ERROR_REQUIRED_ARG_MISSING) {
            PrintFault(errorCode, "TTX");
        }
        /* No arguments is OK - we'll open default window */
        result = TRUE;
    }
    
    return result;
}

/* Parse ToolTypes from icon - called when argc == 0 (Workbench launch) */
BOOL TTX_ParseToolTypes(STRPTR *fileName, struct WBStartup *wbMsg)
{
    struct DiskObject *icon = NULL;
    STRPTR *toolTypes = NULL;
    STRPTR fileArg = NULL;
    ULONG i = 0;
    BOOL result = FALSE;
    
    if (!fileName || !wbMsg) {
        return FALSE;
    }
    
    *fileName = NULL;
    
    /* Get icon for this program */
    icon = GetDiskObject(wbMsg->sm_ArgList[0].wa_Name);
    if (!icon) {
        return FALSE;
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
        
        /* Allocate and copy */
        copy = (STRPTR)AllocVec(len + 1, MEMF_CLEAR);
        if (copy) {
            CopyMem(fileArg, copy, len);
            copy[len] = '\0';
            *fileName = copy;
            result = TRUE;
        }
    }
    
    if (icon) {
        FreeDiskObject(icon);
    }
    
    return result;
}

/* Check if another instance is already running */
BOOL TTX_CheckExistingInstance(STRPTR fileName)
{
    struct MsgPort *existingPort = NULL;
    BOOL result = FALSE;
    
    /* Try to find existing message port */
    Forbid();
    existingPort = FindPort(TTX_MESSAGE_PORT_NAME);
    Permit();
    
    if (existingPort) {
        /* Another instance is running, send message to it */
        if (fileName) {
            result = TTX_SendToExistingInstance(TTX_MSG_OPEN_FILE, fileName);
        } else {
            result = TTX_SendToExistingInstance(TTX_MSG_OPEN_NEW, NULL);
        }
    }
    
    return result;
}

/* Send message to existing instance */
BOOL TTX_SendToExistingInstance(ULONG msgType, STRPTR fileName)
{
    struct MsgPort *existingPort = NULL;
    struct TTXMessage *msg = NULL;
    ULONG fileNameLen = 0;
    STRPTR fileNameCopy = NULL;
    BOOL result = FALSE;
    
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
    
    /* Allocate message */
    msg = (struct TTXMessage *)AllocVec(sizeof(struct TTXMessage), MEMF_CLEAR);
    if (!msg) {
                return FALSE;
            }
    
    /* Fill in message */
    msg->msg.mn_Node.ln_Type = NT_MESSAGE;
    msg->msg.mn_Length = sizeof(struct TTXMessage);
    msg->msg.mn_ReplyPort = NULL;
    msg->type = msgType;
    msg->fileName = NULL;
    msg->fileNameLen = fileNameLen;
    
    /* Allocate and copy filename if provided */
    if (fileName && fileNameLen > 0) {
        fileNameCopy = (STRPTR)AllocVec(fileNameLen + 1, MEMF_CLEAR);
        if (fileNameCopy) {
            CopyMem(fileName, fileNameCopy, fileNameLen);
            fileNameCopy[fileNameLen] = '\0';
            msg->fileName = fileNameCopy;
        } else {
            FreeVec(msg);
                return FALSE;
        }
    }
    
    /* Send message */
    PutMsg(existingPort, (struct Message *)msg);
    result = TRUE;
    
    return result;
}

/* Setup message port for single-instance operation */
BOOL TTX_SetupMessagePort(struct TTXApplication *app)
{
    if (!app) {
        return FALSE;
    }
    
    /* Create application message port */
    app->appPort = CreateMsgPort();
    if (!app->appPort) {
            return FALSE;
    }
    
    /* Set port name for finding by other instances */
    Forbid();
    app->appPort->mp_Node.ln_Name = TTX_MESSAGE_PORT_NAME;
    Permit();
    
    return TRUE;
}

/* Remove message port */
VOID TTX_RemoveMessagePort(struct TTXApplication *app)
{
    struct Message *msg = NULL;
    
    if (!app) {
        return;
    }
    
    if (app->appPort) {
        /* Clean up any pending messages before deleting port */
        while ((msg = GetMsg(app->appPort)) != NULL) {
            struct TTXMessage *ttxMsg = (struct TTXMessage *)msg;
            /* Free message and filename */
            if (ttxMsg->fileName) {
                FreeVec(ttxMsg->fileName);
            }
            FreeVec(ttxMsg);
        }
        Forbid();
        app->appPort->mp_Node.ln_Name = NULL;
        Permit();
        DeleteMsgPort(app->appPort);
        app->appPort = NULL;
    }
}

/* Setup commodity for single-instance (appears in Exchange) */
BOOL TTX_SetupCommodity(struct TTXApplication *app)
{
    struct NewBroker nb;
    LONG brokerError = 0;
    BOOL result = FALSE;
    
    if (!app || !CxBase) {
        return FALSE;
    }
    
    /* Create message port for commodity broker */
    /* This port receives CXM_COMMAND messages from Exchange */
    app->brokerPort = CreateMsgPort();
    if (!app->brokerPort) {
        return FALSE;
    }
    
    /* Initialize NewBroker structure */
    nb.nb_Version = NB_VERSION;
    nb.nb_Name = (STRPTR)"TTX";
    nb.nb_Title = (STRPTR)"TTX Text Editor";
    nb.nb_Descr = (STRPTR)"ToolKit Text eXtension";
    nb.nb_Unique = NBU_UNIQUE | NBU_NOTIFY;
    nb.nb_Flags = COF_SHOW_HIDE;
    nb.nb_Pri = 0;  /* Default priority */
    nb.nb_Port = app->brokerPort;
    nb.nb_ReservedChannel = 0;
    
    /* Create broker */
    /* For single-instance operation, we only need the broker */
    /* No filter/sender/translator chain needed - broker handles Exchange commands */
    app->broker = CxBroker(&nb, &brokerError);
    if (!app->broker) {
        /* Check if broker already exists (duplicate instance) */
        if (brokerError == CBERR_DUP) {
            /* Another instance is running - this will be handled by TTX_CheckExistingInstance */
            DeleteMsgPort(app->brokerPort);
            app->brokerPort = NULL;
            return FALSE;
        }
        DeleteMsgPort(app->brokerPort);
        app->brokerPort = NULL;
        return FALSE;
    }
    
    /* Activate the broker */
    /* Brokers are created inactive, must activate to receive messages */
    ActivateCxObj(app->broker, TRUE);
    
    /* Verify broker is active */
    if (ActivateCxObj(app->broker, -1L) == 0) {
        /* Broker failed to activate */
        DeleteCxObjAll(app->broker);
        app->broker = NULL;
        DeleteMsgPort(app->brokerPort);
        app->brokerPort = NULL;
        return FALSE;
    }
    
    /* Check for any broker errors */
    if (CxObjError(app->broker) != 0) {
        /* Broker has errors, but continue anyway */
        /* The broker might still be registered */
    }
    
    result = TRUE;
    return result;
}

/* Remove commodity */
VOID TTX_RemoveCommodity(struct TTXApplication *app)
{
    struct Message *msg = NULL;
    
    if (!app || !CxBase) {
        return;
    }
    
    if (app->broker) {
        /* Deactivate broker */
        ActivateCxObj(app->broker, FALSE);
        
        /* Remove from input stream */
        RemoveCxObj(app->broker);
        
        /* Delete all commodity objects */
        DeleteCxObjAll(app->broker);
        app->broker = NULL;
    }
    
    if (app->brokerPort) {
        /* Clean up any pending messages before deleting port */
        while ((msg = GetMsg(app->brokerPort)) != NULL) {
            /* Reply to commodity messages (required by commodities.library) */
            ReplyMsg(msg);
        }
        DeleteMsgPort(app->brokerPort);
        app->brokerPort = NULL;
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
    
    if (!app) {
        return FALSE;
    }
    
    /* Allocate session structure */
    session = (struct Session *)AllocVec(sizeof(struct Session), MEMF_CLEAR);
    if (!session) {
        return FALSE;
    }
    
    /* Initialize session */
    session->sessionID = app->nextSessionID++;
    session->modified = FALSE;
    session->readOnly = FALSE;
    session->window = NULL;
    session->fileName = NULL;
    session->buffer = NULL;
    
    /* Allocate and initialize text buffer */
    session->buffer = (struct TextBuffer *)AllocVec(sizeof(struct TextBuffer), MEMF_CLEAR);
    if (!session->buffer) {
        if (session->fileName) {
            FreeVec(session->fileName);
        }
        FreeVec(session);
        return FALSE;
    }
    
    if (!InitTextBuffer(session->buffer)) {
        FreeVec(session->buffer);
        if (session->fileName) {
            FreeVec(session->fileName);
        }
        FreeVec(session);
        return FALSE;
    }
    
    /* Copy filename if provided */
    if (fileName) {
        titleLen = 0;
        while (fileName[titleLen] != '\0') {
            titleLen++;
        }
        if (titleLen > 0) {
            session->fileName = (STRPTR)AllocVec(titleLen + 1, MEMF_CLEAR);
            if (session->fileName) {
                CopyMem(fileName, session->fileName, titleLen);
                session->fileName[titleLen] = '\0';
            }
        }
    }
    
    /* Create window title */
    if (session->fileName) {
        titleLen = 0;
        while (session->fileName[titleLen] != '\0') {
            titleLen++;
        }
    } else {
        titleLen = 8; /* "Untitled" */
    }
    
    titleText = (STRPTR)AllocVec(titleLen + 20, MEMF_CLEAR);
    if (titleText) {
        if (session->fileName) {
            CopyMem(session->fileName, titleText, titleLen);
            titleText[titleLen] = '\0';
        } else {
            CopyMem("Untitled", titleText, 8);
            titleText[8] = '\0';
        }
    }
    
    /* Lock public screen */
    screen = LockPubScreen((STRPTR)"Workbench");
    if (!screen) {
        if (titleText) {
            FreeVec(titleText);
        }
        if (session->fileName) {
            FreeVec(session->fileName);
        }
        FreeVec(session);
        return FALSE;
    }
    
    /* Open window */
    /* Use proper colors: On Workbench screen, pen 1 = black, pen 2 = grey */
    /* We'll set the RastPort pens directly in RenderText for content area */
    session->window = OpenWindowTags(NULL,
                                      WA_InnerWidth, 600,
                                      WA_InnerHeight, 400,
                                      WA_Title, titleText ? titleText : (session->fileName ? session->fileName : (STRPTR)"Untitled"),
                                      WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY | IDCMP_RAWKEY | IDCMP_REFRESHWINDOW | IDCMP_NEWSIZE,
                                      WA_DragBar, TRUE,
                                      WA_ScreenTitle, (STRPTR)"TTX",
                                      WA_DepthGadget, TRUE,
                                      WA_CloseGadget, TRUE,
                                      WA_SizeGadget, TRUE,
                                      WA_AutoAdjust, TRUE,
                                      WA_SimpleRefresh, TRUE,
                                      WA_Activate, TRUE,
                                      WA_PubScreen, screen,
                                      TAG_DONE);
    
    UnlockPubScreen((STRPTR)"Workbench", screen);
    
    if (titleText) {
        FreeVec(titleText);
    }
    
    if (!session->window) {
        if (session->fileName) {
            FreeVec(session->fileName);
        }
        FreeVec(session);
        return FALSE;
    }
    
    /* Set window limits */
    WindowLimits(session->window, 200, 100, 32767, 32767);
    
    /* Load file if filename provided */
    if (session->fileName && session->buffer) {
        if (!LoadFile(session->fileName, session->buffer)) {
            /* File load failed, but keep empty buffer */
        }
    }
    
    /* Initial render */
    if (session->buffer) {
        RenderText(session->window, session->buffer);
        UpdateCursor(session->window, session->buffer);
    }
    
    /* Add to session list */
    if (app->sessions) {
        session->next = app->sessions;
        app->sessions->prev = session;
    }
    app->sessions = session;
    app->sessionCount++;
    app->activeSession = session;
    
    result = TRUE;
    return result;
}

/* Destroy a session */
VOID TTX_DestroySession(struct TTXApplication *app, struct Session *session)
{
    if (!app || !session) {
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
    
    /* Close window */
    if (session->window) {
        CloseWindow(session->window);
        session->window = NULL;
    }
    
    /* Free text buffer */
    if (session->buffer) {
        FreeTextBuffer(session->buffer);
        FreeVec(session->buffer);
        session->buffer = NULL;
    }
    
    /* Free filename */
    if (session->fileName) {
        FreeVec(session->fileName);
        session->fileName = NULL;
    }
    
    /* Free session structure */
    FreeVec(session);
    app->sessionCount--;
    
    /* Update active session */
    if (app->activeSession == session) {
        app->activeSession = app->sessions;
    }
}

/* Handle commodity message (from Exchange or other instances) */
BOOL TTX_HandleCommodityMessage(struct TTXApplication *app, struct Message *msg)
{
    struct CxMsg *cxMsg = NULL;
    ULONG cxMsgID = 0;
    ULONG cxMsgType = 0;
    BOOL result = FALSE;
    
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
                struct TTXMessage *ttxMsg = (struct TTXMessage *)CxMsgData((const CxMsg *)cxMsg);
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
                    
                    /* Free message and filename */
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
                    /* Could show window here if hidden */
                    result = TRUE;
                    break;
                case CXCMD_DISAPPEAR:
                    /* Could hide window here */
                    result = TRUE;
                    break;
                case CXCMD_KILL:
                    app->running = FALSE;
                    result = TRUE;
                    break;
                case CXCMD_UNIQUE:
                    /* Another instance tried to start */
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
        return FALSE;
    }
    
    /* Find session for this window */
    session = app->sessions;
    while (session) {
        if (session->window == imsg->IDCMPWindow) {
            break;
        }
        session = session->next;
    }
    
    if (!session) {
        return FALSE;
    }
    
            switch (imsg->Class) {
                case IDCMP_CLOSEWINDOW:
            TTX_DestroySession(app, session);
            result = TRUE;
                    break;
                    
                case IDCMP_VANILLAKEY:
                case IDCMP_RAWKEY:
            if (session->buffer && !session->readOnly) {
                UBYTE keyCode = imsg->Code;
                ULONG qualifiers = imsg->Qualifier;
                struct InputEvent ievent;
                UBYTE charBuffer[10];
                WORD chars = 0;
                struct KeyMap *keymap = NULL;
                BOOL processed = FALSE;
                
                /* Handle VANILLAKEY first - these are already converted by Intuition */
                /* Annotate handles VANILLAKEY directly for printable characters */
                if (imsg->Class == IDCMP_VANILLAKEY) {
                    /* VANILLAKEY already converted - handle printable characters directly */
                    /* Annotate checks: (code >= 27 && code <= 126) || (code >= 128 && code <= 255) */
                    if ((keyCode >= 27 && keyCode <= 126) || (keyCode >= 128 && keyCode <= 255)) {
                        /* Printable character - insert directly */
                        InsertChar(session->buffer, keyCode);
                        ScrollToCursor(session->buffer, session->window);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x08) {
                        /* Backspace */
                        DeleteChar(session->buffer);
                        ScrollToCursor(session->buffer, session->window);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x0A || keyCode == 0x0D) {
                        /* Enter/Return */
                        InsertNewline(session->buffer);
                        ScrollToCursor(session->buffer, session->window);
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
                        if (session->fileName && session->buffer) {
                            if (SaveFile(session->fileName, session->buffer)) {
                                session->modified = FALSE;
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
                    if (keyCode == 0x1C) {
                        /* Left arrow */
                        if (session->buffer->cursorX > 0) {
                            session->buffer->cursorX--;
                        } else if (session->buffer->cursorY > 0) {
                            session->buffer->cursorY--;
                            session->buffer->cursorX = session->buffer->lines[session->buffer->cursorY].length;
                        }
                        ScrollToCursor(session->buffer, session->window);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x1D) {
                        /* Right arrow */
                        if (session->buffer->cursorX < session->buffer->lines[session->buffer->cursorY].length) {
                            session->buffer->cursorX++;
                        } else if (session->buffer->cursorY < session->buffer->lineCount - 1) {
                            session->buffer->cursorY++;
                            session->buffer->cursorX = 0;
                        }
                        ScrollToCursor(session->buffer, session->window);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x1E) {
                        /* Up arrow */
                        if (session->buffer->cursorY > 0) {
                            session->buffer->cursorY--;
                            if (session->buffer->cursorX > session->buffer->lines[session->buffer->cursorY].length) {
                                session->buffer->cursorX = session->buffer->lines[session->buffer->cursorY].length;
                            }
                        }
                        ScrollToCursor(session->buffer, session->window);
                        RenderText(session->window, session->buffer);
                        UpdateCursor(session->window, session->buffer);
                        processed = TRUE;
                    } else if (keyCode == 0x1F) {
                        /* Down arrow */
                        if (session->buffer->cursorY < session->buffer->lineCount - 1) {
                            session->buffer->cursorY++;
                            if (session->buffer->cursorX > session->buffer->lines[session->buffer->cursorY].length) {
                                session->buffer->cursorX = session->buffer->lines[session->buffer->cursorY].length;
                            }
                        }
                        ScrollToCursor(session->buffer, session->window);
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
                                            InsertChar(session->buffer, charBuffer[i]);
                                        } else if (charBuffer[i] == 0x0A || charBuffer[i] == 0x0D) {
                                            /* Newline */
                                            InsertNewline(session->buffer);
                                        }
                                    }
                                }
                                ScrollToCursor(session->buffer, session->window);
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
                    session->modified = session->buffer->modified;
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
            if (session->buffer) {
                RenderText(session->window, session->buffer);
                UpdateCursor(session->window, session->buffer);
            }
            result = TRUE;
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
    
    if (!app) {
        return;
    }
    
    /* Build signal mask */
    app->sigmask = (1UL << app->appPort->mp_SigBit);
    if (app->brokerPort) {
        app->sigmask |= (1UL << app->brokerPort->mp_SigBit);
    }
    if (app->sessions) {
        struct Session *session = app->sessions;
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
        signals = Wait(app->sigmask);
        
        /* Check for break signal */
        if (signals & SIGBREAKF_CTRL_C) {
            app->running = FALSE;
            break;
        }
        
        /* Check broker port (commodity messages from Exchange) */
        if (app->brokerPort && (signals & (1UL << app->brokerPort->mp_SigBit))) {
            while ((msg = GetMsg(app->brokerPort)) != NULL) {
                TTX_HandleCommodityMessage(app, msg);
            }
        }
        
        /* Check application port (inter-instance messages) */
        if (signals & (1UL << app->appPort->mp_SigBit)) {
            while ((msg = GetMsg(app->appPort)) != NULL) {
                struct TTXMessage *ttxMsg = (struct TTXMessage *)msg;
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
                /* Free message and filename */
                if (ttxMsg->fileName) {
                    FreeVec(ttxMsg->fileName);
                }
                FreeVec(ttxMsg);
            }
        }
        
        /* Check session windows */
        if (app->sessions) {
            struct Session *session = app->sessions;
            while (session) {
                if (session->window && 
                    (signals & (1UL << session->window->UserPort->mp_SigBit))) {
                    while ((imsg = (struct IntuiMessage *)GetMsg(session->window->UserPort)) != NULL) {
                        TTX_HandleIntuitionMessage(app, imsg);
            ReplyMsg((struct Message *)imsg);
                    }
                }
                session = session->next;
            }
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
    
    if (!app) {
        return FALSE;
    }
    
    /* Clear application structure */
    for (i = 0; i < sizeof(struct TTXApplication); i++) {
        ((UBYTE *)app)[i] = 0;
    }
    
    /* Initialize libraries */
    if (!TTX_InitLibraries()) {
        return FALSE;
    }
    
    /* Setup message port */
    if (!TTX_SetupMessagePort(app)) {
        TTX_CleanupLibraries();
        return FALSE;
    }
    
    /* Setup commodity if available */
    if (CxBase) {
        if (!TTX_SetupCommodity(app)) {
            /* Commodity setup failed, but continue anyway */
            /* The app can still work without commodities */
        }
    }
    
    return TRUE;
}

/* Cleanup application */
VOID TTX_Cleanup(struct TTXApplication *app)
{
    if (!app) {
        return;
    }
    
    /* Destroy all sessions */
    while (app->sessions) {
        TTX_DestroySession(app, app->sessions);
    }
    
    /* Remove commodity */
    TTX_RemoveCommodity(app);
    
    /* Remove message port */
    TTX_RemoveMessagePort(app);
    
    /* Cleanup libraries */
    TTX_CleanupLibraries();
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
    ULONG i = 0;
    BOOL parseResult = FALSE;
    LONG result = RETURN_OK;
    
    /* Initialize application */
    if (!TTX_Init(&app)) {
        LONG errorCode = IoErr();
        PrintFault(errorCode ? errorCode : ERROR_OBJECT_NOT_FOUND, "TTX");
        return RETURN_FAIL;
    }
    
    /* Clear args structure */
    for (i = 0; i < sizeof(struct TTXArgs); i++) {
        ((UBYTE *)&ttxArgs)[i] = 0;
    }
    
    /* Parse arguments (command line or tooltypes) */
    if (argc > 0) {
        /* CLI launch - TurboText style */
        parseResult = TTX_ParseArguments(&ttxArgs);
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
            fullPath[0] = '\0';
            if (wbarg->wa_Lock) {
                NameFromLock(wbarg->wa_Lock, fullPath, sizeof(fullPath));
            }
            AddPart(fullPath, wbarg->wa_Name, sizeof(fullPath));
            
            len = 0;
            while (fullPath[len] != '\0' && len < sizeof(fullPath) - 1) {
                len++;
            }
            if (len > 0) {
                fileName = (STRPTR)AllocVec(len + 1, MEMF_CLEAR);
                if (fileName) {
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
            parseResult = TTX_ParseToolTypes(&fileName, wbMsg);
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
        if (rda) {
            FreeArgs(rda);
        }
        TTX_Cleanup(&app);
        return RETURN_OK;
    }
    
    /* If BACKGROUND is set, don't open any sessions immediately - stay in background */
    if (parseResult && ttxArgs.background) {
        /* Background mode - don't open sessions, just stay loaded */
        app.backgroundMode = TRUE;
        /* Free parsed arguments */
        if (rda) {
            FreeArgs(rda);
        }
        /* Run event loop even with no sessions (background mode) */
        TTX_EventLoop(&app);
        TTX_Cleanup(&app);
    return result;
}

    /* Not in background mode */
    app.backgroundMode = FALSE;
    
    /* Check if another instance is running - but only if we have files or NOWINDOW not set */
    if (parseResult) {
        if (ttxArgs.files && ttxArgs.files[0]) {
            /* We have files - check for existing instance */
            if (TTX_CheckExistingInstance(ttxArgs.files[0])) {
                /* Message sent to existing instance, exit */
                if (rda) {
                    FreeArgs(rda);
                }
                TTX_Cleanup(&app);
                return RETURN_OK;
            }
        } else if (!ttxArgs.noWindow) {
            /* No files but NOWINDOW not set - check for existing instance */
            if (TTX_CheckExistingInstance(NULL)) {
                if (rda) {
                    FreeArgs(rda);
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
    
    /* Create sessions for files (only if BACKGROUND not set) */
    if (parseResult && ttxArgs.files) {
        files = ttxArgs.files;
        while (*files) {
            if (!TTX_CreateSession(&app, *files)) {
                LONG errorCode = IoErr();
                if (errorCode != 0) {
                    PrintFault(errorCode, "TTX");
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
            }
            result = RETURN_FAIL;
        }
    }
    
    /* Free parsed arguments */
    if (rda) {
        FreeArgs(rda);
    }
    
    /* Run event loop if we have sessions */
    if (app.sessionCount > 0) {
        TTX_EventLoop(&app);
    }
    
    /* Cleanup */
    TTX_Cleanup(&app);
    
    return result;
}
