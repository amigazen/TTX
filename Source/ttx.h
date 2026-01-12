/*
 * TTX - Text Editor for AmigaOS
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TTX_H
#define TTX_H

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <intuition/classusr.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
#include <graphics/rastport.h>
#include <graphics/gfx.h>
#include <graphics/text.h>
#include <graphics/regions.h>
#include <graphics/layers.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <libraries/commodities.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <devices/inputevent.h>
#include <devices/keymap.h>
#include <libraries/keymap.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/icon.h>
#include <proto/locale.h>
#include <proto/rexxsyslib.h>
#include <proto/wb.h>
#include <proto/asl.h>
#include <proto/commodities.h>
#include <proto/keymap.h>
#include <proto/gadtools.h>
#include "seiso.h"

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
extern struct Library *AslBase;

/* Message port name for single-instance communication */
#define TTX_MESSAGE_PORT_NAME "TTX.1"

/* Message types for inter-instance communication */
#define TTX_MSG_OPEN_FILE 1
#define TTX_MSG_OPEN_NEW   2
#define TTX_MSG_QUIT       3

/* Message structure for inter-instance communication */
struct TTXMessage {
    struct Message msg;
    ULONG type;
    STRPTR fileName;
    ULONG fileNameLen;
};

/* Text buffer structures */
#define MAX_LINES 10000
#define MAX_LINE_LENGTH 4096

struct TextLine {
    STRPTR text;
    ULONG length;
    ULONG allocated;
};

/* Text selection/marking structure */
struct TextMarking {
    BOOL enabled;                /* Boolean that indicates whether block is on/off */
    ULONG startY;                /* Line where marking starts */
    ULONG startX;                /* X position of start */
    ULONG stopY;                 /* Line where marking ends */
    ULONG stopX;                 /* X position of stop */
};

struct TextBuffer {
    struct TextLine *lines;
    ULONG lineCount;
    ULONG maxLines;
    ULONG cursorX;
    ULONG cursorY;
    ULONG scrollX;
    ULONG scrollY;
    ULONG leftMargin;  /* Left margin for line numbers, fold markers, etc. (in pixels) */
    ULONG pageW;       /* Maximum characters per line (calculated from window width) */
    ULONG pageH;       /* Maximum visible lines (calculated from window height) */
    ULONG maxScrollX;  /* Maximum horizontal scroll position (in characters) */
    ULONG maxScrollY;  /* Maximum vertical scroll position (in lines) */
    SHORT scrollXShift;  /* Scaling shift factor for horizontal scroll (for values > 0xFFFF) */
    SHORT scrollYShift;  /* Scaling shift factor for vertical scroll (for values > 0xFFFF) */
    BOOL modified;
    struct TextMarking marking;  /* Text selection/marking */
    /* Graphics v39+ features for optimized rendering */
    struct BitMap *superBitMap;  /* Super bitmap for off-screen rendering (larger than window) */
    ULONG superWidth;             /* Width of super bitmap in pixels */
    ULONG superHeight;            /* Height of super bitmap in pixels */
    ULONG lastScrollX;            /* Last scroll X position for delta scrolling */
    ULONG lastScrollY;            /* Last scroll Y position for delta scrolling */
    BOOL needsFullRedraw;         /* Flag to force full redraw (e.g., after resize) */
};

/* Forward declarations */
struct TTXArgs {
    STRPTR *files;
    STRPTR startup;
    STRPTR window;
    STRPTR pubscreen;
    STRPTR settings;
    STRPTR definitions;
    BOOL noWindow;
    BOOL wait;
    BOOL background;
    BOOL unload;
    struct RDArgs *rda;
};

/* Window state structure - stores window creation parameters for restoration */
struct WindowState {
    LONG leftEdge;          /* Window left position */
    LONG topEdge;           /* Window top position */
    ULONG innerWidth;       /* Window inner width */
    ULONG innerHeight;      /* Window inner height */
    ULONG flags;            /* Window flags (WFLG_*) */
    ULONG idcmpFlags;       /* IDCMP flags */
    STRPTR title;           /* Window title */
    STRPTR screenTitle;     /* Screen title */
    STRPTR pubScreenName;   /* Public screen name (or NULL for default) */
    ULONG minWidth;         /* Minimum window width */
    ULONG minHeight;        /* Minimum window height */
    ULONG maxWidth;         /* Maximum window width */
    ULONG maxHeight;        /* Maximum window height */
    BOOL windowOpen;        /* TRUE if window is currently open */
};

/* Document state structure - stores document metadata */
struct DocumentState {
    STRPTR fileName;        /* File name (or NULL for untitled) */
    BOOL modified;          /* TRUE if document has been modified since load */
    BOOL readOnly;           /* TRUE if document is read-only */
    ULONG loadTime;          /* Time when file was loaded (for "modified since" checks) */
    ULONG fileSize;          /* File size in bytes (0 if untitled) */
    BOOL fileExists;         /* TRUE if file exists on disk */
};

/* Session structure - one per open file/window */
/* Implements Session-Window-Document model: Session contains Window and Document state */
struct Session {
    struct Session *next;
    struct Session *prev;
    ULONG sessionID;
    struct CleanupStack *cleanupStack;  /* Pointer to global cleanup stack (app->cleanupStack) */
    /* Window state - Intuition window and UI elements */
    struct Window *window;              /* Intuition window (NULL if closed/iconified) */
    struct Menu *menuStrip;             /* Menu strip for this window */
    struct Gadget *vertPropGadget;      /* Vertical scroll bar prop gadget */
    struct Gadget *horizPropGadget;     /* Horizontal scroll bar prop gadget */
    struct WindowState windowState;     /* Window creation parameters (for restoration) */
    /* Document state - file and buffer */
    struct DocumentState docState;      /* Document metadata */
    struct TextBuffer *buffer;          /* Text buffer (always present, even when window closed) */
    /* Mouse selection state */
    BOOL mouseSelecting;                /* TRUE if mouse button is down and we're selecting */
    ULONG selectStartX;                 /* Selection start X position */
    ULONG selectStartY;                 /* Selection start Y position */
};

/* Prop gadget IDs */
#define GID_VERT_PROP 1
#define GID_HORIZ_PROP 2

/* Application structure - single instance */
struct TTXApplication {
    struct CleanupStack *cleanupStack;  /* Resource tracking cleanup stack */
    struct MsgPort *appPort;
    struct MsgPort *brokerPort;  /* Message port for commodity broker */
    CxObj *broker;  /* Commodity broker object */
    CxObj *filter;  /* Commodity filter object */
    CxObj *sender;  /* Commodity sender object */
    struct Session *sessions;
    ULONG sessionCount;
    ULONG nextSessionID;
    struct Session *activeSession;
    BOOL running;
    BOOL backgroundMode;  /* TRUE if running in background mode */
    ULONG signals;
    ULONG sigmask;
    /* App icon support for application-level iconification */
    struct MsgPort *appIconPort;  /* Message port for app icon */
    struct AppIcon *appIcon;      /* App icon object */
    struct DiskObject *appIconDO; /* Disk object for app icon */
    BOOL iconified;               /* TRUE if application is iconified */
    BOOL iconifyDeferred;         /* Defer iconification to main loop */
    BOOL iconifyState;            /* Desired iconification state */
};

/* Forward declarations */
BOOL TTX_Init(struct TTXApplication *app);
VOID TTX_Cleanup(struct TTXApplication *app);
BOOL TTX_InitLibraries(struct CleanupStack *stack);
VOID TTX_CleanupLibraries(VOID);
BOOL TTX_SetupMessagePort(struct TTXApplication *app);
BOOL TTX_AddMessagePort(struct TTXApplication *app);
VOID TTX_RemoveMessagePort(struct TTXApplication *app);
BOOL TTX_SetupCommodity(struct TTXApplication *app);
VOID TTX_RemoveCommodity(struct TTXApplication *app);
BOOL TTX_ParseArguments(struct TTXArgs *args, struct CleanupStack *stack);
BOOL TTX_ParseToolTypes(STRPTR *fileName, struct WBStartup *wbMsg, struct CleanupStack *stack);
BOOL TTX_CheckExistingInstance(STRPTR fileName);
BOOL TTX_SendToExistingInstance(struct CleanupStack *stack, ULONG msgType, STRPTR fileName);
BOOL TTX_CreateSession(struct TTXApplication *app, STRPTR fileName);
VOID TTX_DestroySession(struct TTXApplication *app, struct Session *session);
VOID TTX_EventLoop(struct TTXApplication *app);
BOOL TTX_HandleCommodityMessage(struct TTXApplication *app, struct Message *msg);
BOOL TTX_HandleIntuitionMessage(struct TTXApplication *app, struct IntuiMessage *imsg);
BOOL TTX_CreateMenuStrip(struct Session *session);
VOID TTX_FreeMenuStrip(struct Session *session);
BOOL TTX_HandleCommand(struct TTXApplication *app, struct Session *session, STRPTR command, STRPTR *args, ULONG argCount);
BOOL TTX_HandleMenuPick(struct TTXApplication *app, struct Session *session, ULONG menuNumber, ULONG itemNumber);
VOID TTX_ShowUsage(VOID);
VOID TTX_Iconify(struct TTXApplication *app, BOOL iconify);
VOID TTX_DoIconify(struct TTXApplication *app, BOOL iconify);
VOID TTX_ProcessAppIcon(struct TTXApplication *app);
BOOL TTX_SetupAppIcon(struct TTXApplication *app);
VOID TTX_RemoveAppIcon(struct TTXApplication *app);
BOOL TTX_SaveWindowState(struct Session *session);
BOOL TTX_RestoreWindow(struct TTXApplication *app, struct Session *session);

/* Command handler functions - organized by category */
/* Document commands */
BOOL TTX_Cmd_ActivateLastDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ActivateNextDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ActivatePrevDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_CloseDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_OpenDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* Display/Window commands */
BOOL TTX_Cmd_ActivateWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_BeepScreen(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_CloseRequester(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ControlWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetCursor(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetScreenInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetWindowInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_IconifyWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveSizeWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_OpenRequester(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_RemakeScreen(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Screen2Back(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Screen2Front(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetCursor(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetStatusBar(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SizeWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_UsurpWindow(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Window2Back(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Window2Front(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* View commands */
BOOL TTX_Cmd_CenterView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetViewInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ScrollView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SizeView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SplitView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SwapViews(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SwitchView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_UpdateView(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* Selection block commands */
BOOL TTX_Cmd_CopyBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_CutBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_DeleteBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_EncryptBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetBlkInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MarkBlk(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* Clipboard commands */
BOOL TTX_Cmd_OpenClip(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_PasteClip(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_PrintClip(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SaveClip(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* File commands */
BOOL TTX_Cmd_ClearFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetFileInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetFilePath(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_InsertFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_OpenFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_PrintFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SaveFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SaveFileAs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetFilePath(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* Cursor position commands */
BOOL TTX_Cmd_Find(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetCursorPos(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Move(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveChar(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveDown(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveDownScr(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveEOF(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveEOL(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveLastChange(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveLeft(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveMatchBkt(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveNextTabStop(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveNextWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MovePrevTabStop(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MovePrevWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveRight(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveSOF(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveSOL(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveUp(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveUpScr(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* Bookmark commands */
BOOL TTX_Cmd_ClearBookmark(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveAutomark(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MoveBookmark(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetBookmark(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* Editing commands */
BOOL TTX_Cmd_Delete(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_DeleteEOL(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_DeleteEOW(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_DeleteLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_DeleteSOL(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_DeleteSOW(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_FindChange(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetChar(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Insert(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_InsertLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetChar(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SwapChars(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Text(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ToggleCharCase(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_UndeleteLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_UndoLine(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* Word-level editing commands */
BOOL TTX_Cmd_CompleteTemplate(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_CorrectWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_CorrectWordCase(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ReplaceWord(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* Formatting commands */
BOOL TTX_Cmd_Center(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Conv2Lower(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Conv2Spaces(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Conv2Tabs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Conv2Upper(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_FormatParagraph(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Justify(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ShiftLeft(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ShiftRight(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* Fold commands */
BOOL TTX_Cmd_HideFold(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_MakeFold(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ShowFold(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ToggleFold(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_UnmakeFold(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* Macro commands */
BOOL TTX_Cmd_EndMacro(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ExecARexxMacro(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ExecARexxString(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_FlushARexxCache(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetARexxCache(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetMacroInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_OpenMacro(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_PlayMacro(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_RecordMacro(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SaveMacro(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetARexxCache(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* External tool commands */
BOOL TTX_Cmd_ExecTool(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* Configuration commands */
BOOL TTX_Cmd_GetPrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_OpenDefinitions(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_OpenPrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SaveDefPrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SavePrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetPrefs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* ARexx input commands */
BOOL TTX_Cmd_RequestBool(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_RequestChoice(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_RequestFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_RequestNum(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_RequestStr(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* ARexx control commands */
BOOL TTX_Cmd_GetBackground(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetCurrentDir(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetDocuments(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetErrorInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetLockInfo(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetPort(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetPriority(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetReadOnly(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_GetVersion(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetBackground(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetCurrentDir(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetDisplayLock(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetInputLock(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetMeta(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetMeta2(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetMode(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetMode2(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetPriority(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetQuoteMode(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetReadOnly(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
/* Helper commands */
BOOL TTX_Cmd_Help(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Illegal(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_NOP(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Iconify(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_Quit(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);

/* Text buffer functions */
BOOL InitTextBuffer(struct TextBuffer *buffer, struct CleanupStack *stack);
VOID FreeTextBuffer(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL LoadFile(STRPTR fileName, struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL SaveFile(STRPTR fileName, struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL InsertChar(struct TextBuffer *buffer, UBYTE ch, struct CleanupStack *stack);
BOOL DeleteChar(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL DeleteForward(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL InsertNewline(struct TextBuffer *buffer, struct CleanupStack *stack);
VOID MouseToCursor(struct TextBuffer *buffer, struct Window *window, LONG mouseX, LONG mouseY, ULONG *cursorX, ULONG *cursorY);
BOOL CreateSuperBitMap(struct TextBuffer *buffer, struct Window *window);
VOID FreeSuperBitMap(struct TextBuffer *buffer);
VOID RenderText(struct Window *window, struct TextBuffer *buffer);
VOID UpdateCursor(struct Window *window, struct TextBuffer *buffer);
VOID ScrollToCursor(struct TextBuffer *buffer, struct Window *window);
/* Block operations */
STRPTR GetBlock(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL DeleteBlock(struct TextBuffer *buffer, struct CleanupStack *stack);
VOID MarkAllBlock(struct TextBuffer *buffer);
VOID SetMarking(struct TextBuffer *buffer, ULONG startY, ULONG startX, ULONG stopY, ULONG stopX);
VOID ClearMarking(struct TextBuffer *buffer);
/* Word navigation */
BOOL MoveNextWord(struct TextBuffer *buffer);
BOOL MovePrevWord(struct TextBuffer *buffer);
BOOL MoveEndOfLine(struct TextBuffer *buffer);
BOOL MoveStartOfLine(struct TextBuffer *buffer);
BOOL MoveEndOfWord(struct TextBuffer *buffer);
BOOL MoveStartOfWord(struct TextBuffer *buffer);
ULONG GetCharWidth(struct RastPort *rp, UBYTE ch);
ULONG GetLineHeight(struct RastPort *rp);
VOID UpdateScrollBars(struct Session *session);
VOID CalculateMaxScroll(struct TextBuffer *buffer, struct Window *window);
/* Delete operations */
BOOL DeleteEOL(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL DeleteEOW(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL DeleteSOL(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL DeleteSOW(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL DeleteLine(struct TextBuffer *buffer, struct CleanupStack *stack);
/* Text insertion operations */
BOOL InsertText(struct TextBuffer *buffer, STRPTR text, struct CleanupStack *stack);
UBYTE GetCharAtCursor(struct TextBuffer *buffer);
STRPTR GetCurrentLine(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL SetCharAtCursor(struct TextBuffer *buffer, UBYTE ch, struct CleanupStack *stack);
BOOL SwapChars(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL ToggleCharCase(struct TextBuffer *buffer, struct CleanupStack *stack);
/* Word operations */
STRPTR GetWordAtCursor(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL ReplaceWordAtCursor(struct TextBuffer *buffer, STRPTR newWord, struct CleanupStack *stack);
/* Case conversion operations */
BOOL ConvertToUpper(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL ConvertToLower(struct TextBuffer *buffer, struct CleanupStack *stack);
/* Indentation operations */
BOOL ShiftLeft(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL ShiftRight(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL ConvertTabsToSpaces(struct TextBuffer *buffer, struct CleanupStack *stack);
BOOL ConvertSpacesToTabs(struct TextBuffer *buffer, struct CleanupStack *stack);

/* Definition file parser */
struct DFNFile;
struct DFNFile *ParseDFNFile(STRPTR fileName, struct CleanupStack *stack);
VOID FreeDFNFile(struct DFNFile *dfn);
struct NewMenu *ConvertDFNToNewMenu(struct DFNFile *dfn, ULONG *outCount);

#endif /* TTX_H */
