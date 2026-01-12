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

/* Command handler functions - Project menu */
BOOL TTX_Cmd_OpenFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_OpenDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_InsertFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SaveFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SaveFileAs(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_ClearFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_PrintFile(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_CloseDoc(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
BOOL TTX_Cmd_SetReadOnly(struct TTXApplication *app, struct Session *session, STRPTR *args, ULONG argCount);
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
ULONG GetCharWidth(struct RastPort *rp, UBYTE ch);
ULONG GetLineHeight(struct RastPort *rp);
VOID UpdateScrollBars(struct Session *session);
VOID CalculateMaxScroll(struct TextBuffer *buffer, struct Window *window);

#endif /* TTX_H */
