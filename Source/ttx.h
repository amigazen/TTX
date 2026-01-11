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
#include <utility/tagitem.h>
#include <graphics/rastport.h>
#include <graphics/gfx.h>
#include <graphics/text.h>
#include <graphics/regions.h>
#include <workbench/startup.h>
#include <libraries/commodities.h>
#include <devices/inputevent.h>
#include <devices/keymap.h>
#include <libraries/keymap.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/icon.h>
#include <proto/locale.h>
#include <proto/rexxsyslib.h>
#include <proto/wb.h>
#include <proto/commodities.h>
#include <proto/keymap.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <intuition/classusr.h>
#include "seiso.h"

/* Library base pointers */
extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;
extern struct Library *UtilityBase;
extern struct GfxBase *GfxBase;
extern struct Library *CxBase;
extern struct Library *KeymapBase;

/* Version string */
static const char *verstag = "$VER: TTX 3.0 (7/1/2026)\n";
static const char *stack_cookie = "$STACK: 4096\n";

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

/* Session structure - one per open file/window */
struct Session {
    struct Session *next;
    struct Session *prev;
    ULONG sessionID;
    struct CleanupStack *cleanupStack;  /* Pointer to global cleanup stack (app->cleanupStack) */
    struct Window *window;
    struct Menu *menuStrip;  /* Menu strip for this window */
    struct Gadget *vertPropGadget;  /* Vertical scroll bar prop gadget */
    struct Gadget *horizPropGadget;  /* Horizontal scroll bar prop gadget */
    STRPTR fileName;
    struct TextBuffer *buffer;
    BOOL modified;
    BOOL readOnly;
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

