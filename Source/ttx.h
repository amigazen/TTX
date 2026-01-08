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
#define TTX_MESSAGE_PORT_NAME "TTX_MessagePort"

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
    BOOL modified;
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
    struct Window *window;
    STRPTR fileName;
    struct TextBuffer *buffer;
    BOOL modified;
    BOOL readOnly;
};

/* Application structure - single instance */
struct TTXApplication {
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
BOOL TTX_InitLibraries(VOID);
VOID TTX_CleanupLibraries(VOID);
BOOL TTX_SetupMessagePort(struct TTXApplication *app);
VOID TTX_RemoveMessagePort(struct TTXApplication *app);
BOOL TTX_SetupCommodity(struct TTXApplication *app);
VOID TTX_RemoveCommodity(struct TTXApplication *app);
BOOL TTX_ParseArguments(struct TTXArgs *args);
BOOL TTX_ParseToolTypes(STRPTR *fileName, struct WBStartup *wbMsg);
BOOL TTX_CheckExistingInstance(STRPTR fileName);
BOOL TTX_SendToExistingInstance(ULONG msgType, STRPTR fileName);
BOOL TTX_CreateSession(struct TTXApplication *app, STRPTR fileName);
VOID TTX_DestroySession(struct TTXApplication *app, struct Session *session);
VOID TTX_EventLoop(struct TTXApplication *app);
BOOL TTX_HandleCommodityMessage(struct TTXApplication *app, struct Message *msg);
BOOL TTX_HandleIntuitionMessage(struct TTXApplication *app, struct IntuiMessage *imsg);
VOID TTX_ShowUsage(VOID);

/* Text buffer functions */
BOOL InitTextBuffer(struct TextBuffer *buffer);
VOID FreeTextBuffer(struct TextBuffer *buffer);
BOOL LoadFile(STRPTR fileName, struct TextBuffer *buffer);
BOOL SaveFile(STRPTR fileName, struct TextBuffer *buffer);
BOOL InsertChar(struct TextBuffer *buffer, UBYTE ch);
BOOL DeleteChar(struct TextBuffer *buffer);
BOOL DeleteForward(struct TextBuffer *buffer);
BOOL InsertNewline(struct TextBuffer *buffer);
VOID MouseToCursor(struct TextBuffer *buffer, struct Window *window, LONG mouseX, LONG mouseY, ULONG *cursorX, ULONG *cursorY);
VOID RenderText(struct Window *window, struct TextBuffer *buffer);
VOID UpdateCursor(struct Window *window, struct TextBuffer *buffer);
VOID ScrollToCursor(struct TextBuffer *buffer, struct Window *window);
ULONG GetCharWidth(struct RastPort *rp, UBYTE ch);
ULONG GetLineHeight(struct RastPort *rp);

#endif /* TTX_H */

