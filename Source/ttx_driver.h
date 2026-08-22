/*
 * TTX driver - UI layer types and declarations
 *
 * The driver owns Intuition windows, gadgets, menus, and the event loop.
 * Document editing is delegated to turbotext.library.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TTX_DRIVER_H
#define TTX_DRIVER_H

#include <exec/types.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <workbench/startup.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/icon.h>
#include <proto/wb.h>
#include <proto/asl.h>
#include <proto/gadtools.h>
#include <proto/keymap.h>
#include <workbench/icon.h>

#include "ttx_mem.h"
#include "libraries/turbotext.h"
#include "proto/turbotext.h"

/****************************************************************************/

#define TTX_MESSAGE_PORT_NAME "TTX.1"
#define TTX_PROGASSIGN        "TurboText:"
#define TTX_LIBS_PREFIX       "TurboText:Libs/"

/* Sentinel for closed window handles (was seiso INVALID_RESOURCE) */
#define INVALID_RESOURCE ((APTR)0xFFFFFFFF)
#define TTX_MSG_OPEN_FILE 1
#define TTX_MSG_OPEN_NEW   2
#define TTX_MSG_QUIT       3

#define GID_VERT_PROP 1
#define GID_HORIZ_PROP 2

/* Menu UserData: 0x8000 tag allows menu 0 / item 0 (Open) without a NULL pointer */
#define TTX_MENU_UD_FLAG 0x8000UL

/****************************************************************************/

/* BOOPSI scroll gadget handles (TurboText: bar + sysiclass arrow buttons) */
struct TTXScrollGadgets {
	struct Gadget *vertProp;
	struct Gadget *horizProp;
	struct Gadget *vertUp;
	struct Gadget *vertDown;
	struct Gadget *horizLeft;
	struct Gadget *horizRight;
	/* Head gadget passed to AddGList (must match RemoveGList on destroy) */
	struct Gadget *gadgetHead;
	BOOL gadgetsOnWindow;
	/* SYSICLASS arrow images hold DrawInfo pointers — keep until gadgets die */
	struct DrawInfo *drawInfo;
	struct Screen *screen;
	/* Suppress ICA -> IDCMP_IDCMPUPDATE while SetGadgetAttrs updates PGA_Top */
	BOOL suppressIcmp;
};

/****************************************************************************/

/* Driver-side render bitmap cache (not part of engine document model) */
struct TTXRenderState {
	struct BitMap *superBitMap;
	ULONG superWidth;
	ULONG superHeight;
	ULONG lastScrollX;
	ULONG lastScrollY;
	BOOL needsFullRedraw;
};

struct TTXMessage {
	struct Message msg;
	ULONG type;
	STRPTR fileName;
	ULONG fileNameLen;
};

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

struct WindowState {
	LONG leftEdge;
	LONG topEdge;
	ULONG innerWidth;
	ULONG innerHeight;
	ULONG flags;
	ULONG idcmpFlags;
	STRPTR title;
	STRPTR screenTitle;
	STRPTR pubScreenName;
	ULONG minWidth;
	ULONG minHeight;
	ULONG maxWidth;
	ULONG maxHeight;
	BOOL windowOpen;
};

/*
 * Session: driver window binding for one document.
 * Document data lives in turbotext.library (struct TTDocument).
 */
struct Session {
	struct Session *next;
	struct Session *prev;
	ULONG sessionID;
	struct TTDocument *document;
	struct TTXRenderState render;
	struct Window *window;
	struct Menu *menuStrip;
	APTR menuVisualInfo;
	struct NewMenu *menuNewMenuBacking;
	struct DFNFile *menuDFNBacking;
	struct TTXScrollGadgets scroll;
	struct Gadget *textEditorGadget;
	struct WindowState windowState;
	BOOL mouseSelecting;
	ULONG selectStartX;
	ULONG selectStartY;
};

#define TTX_DEFER_NONE            0
#define TTX_DEFER_OPENDOC_NEW     1
#define TTX_DEFER_OPENDOC_FILEREQ 2
#define TTX_DEFER_OPENFILE_FILEREQ 3

struct TTXApplication {
	struct MsgPort *appPort;
	/* OpenLibrary base; libcalls use global TurboTextBase from proto/turbotext.h */
	struct Session *sessions;
	ULONG sessionCount;
	ULONG nextSessionID;
	struct Session *activeSession;
	BOOL running;
	BOOL backgroundMode;
	ULONG signals;
	ULONG sigmask;
	struct MsgPort *appIconPort;
	struct AppIcon *appIcon;
	struct DiskObject *appIconDO;
	BOOL iconified;
	BOOL iconifyDeferred;
	BOOL iconifyState;
	/* Defer Intuition-heavy work until after ReplyMsg on IDCMP messages */
	ULONG intuiHandlerDepth;
	ULONG deferredAction;
	struct Session *deferredCloseSession;
	struct Session *deferredOpenSession;
	TEXT lastAslDrawer[512];
};

/****************************************************************************/
/* Library base pointers (driver process) */

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;
extern struct Library *UtilityBase;
extern struct GfxBase *GfxBase;
extern struct Library *IconBase;
extern struct Library *WorkbenchBase;
extern struct Library *KeymapBase;
extern struct Library *AslBase;
extern struct Library *GadToolsBase;

/****************************************************************************/
/* Session buffer accessor */

struct TTTextBuffer *TT_SessionBuffer(struct Session *session);

/****************************************************************************/

BOOL TTX_Init(struct TTXApplication *app);
VOID TTX_Cleanup(struct TTXApplication *app);
BOOL TTX_InitLibraries(VOID);
BOOL TTX_OpenTurboText(struct TTXApplication *app);
VOID TTX_CloseTurboText(struct TTXApplication *app);
BOOL TTX_ParseArguments(struct TTXArgs *args);
BOOL TTX_ParseToolTypes(STRPTR *fileName, struct WBStartup *wbMsg);
BOOL TTX_CheckExistingInstance(STRPTR fileName);
BOOL TTX_SendToExistingInstance(ULONG msgType, STRPTR fileName);
BOOL TTX_CreateSession(struct TTXApplication *app, STRPTR fileName);
BOOL TTX_CreateSessionForDocument(struct TTXApplication *app, struct TTDocument *doc, STRPTR fileName);
VOID TTX_DestroySession(struct TTXApplication *app, struct Session *session);
VOID TTX_EventLoop(struct TTXApplication *app);
BOOL TTX_HandleIntuitionMessage(struct TTXApplication *app,
	struct Session *session, struct IntuiMessage *imsg);
BOOL TTX_HandleCommand(struct TTXApplication *app, struct Session *session, STRPTR command, STRPTR *args, ULONG argCount);
BOOL TTX_DoEngineCommand(
	struct TTXApplication *app,
	struct Session *session,
	STRPTR command,
	STRPTR *args,
	ULONG argCount);
BOOL TTX_HandleMenuPick(struct TTXApplication *app, struct Session *session, ULONG menuNumber, ULONG itemNumber);
BOOL TTX_CreateMenuStrip(struct Session *session);
VOID TTX_FreeMenuStrip(struct Session *session);
VOID TTX_ResetMenuStrip(struct Session *session);
VOID TTX_ShowUsage(VOID);
VOID TTX_Iconify(struct TTXApplication *app, BOOL iconify);
VOID TTX_DoIconify(struct TTXApplication *app, BOOL iconify);
VOID TTX_ProcessAppIcon(struct TTXApplication *app);
BOOL TTX_SetupAppIcon(struct TTXApplication *app);
VOID TTX_RemoveAppIcon(struct TTXApplication *app);
BOOL TTX_SaveWindowState(struct Session *session);
VOID TTX_CloseSessionWindow(struct TTXApplication *app, struct Session *session,
	struct Window *closedMark);
VOID TTX_RequestDestroySession(struct TTXApplication *app, struct Session *session);
BOOL TTX_RestoreWindow(struct TTXApplication *app, struct Session *session);
VOID TTX_RebuildSignalMask(struct TTXApplication *app);
VOID TTX_ProcessDeferredActions(struct TTXApplication *app);
LONG TTX_RunWithArgs(struct TTXApplication *app, struct TTXArgs *args);

/****************************************************************************/
/* Rendering (driver UI layer) */

ULONG GetCharWidth(struct RastPort *rp, UBYTE ch);
ULONG GetLineHeight(struct RastPort *rp);
VOID ScrollToCursor(struct TTTextBuffer *buffer, struct Window *window);
BOOL CreateSuperBitMap(struct Session *session, struct Window *window);
VOID FreeSuperBitMap(struct Session *session);
VOID RenderText(struct Window *window, struct Session *session);
VOID UpdateCursor(struct Window *window, struct Session *session);
VOID MouseToCursor(struct TTTextBuffer *buffer, struct Window *window, LONG mouseX, LONG mouseY, ULONG *cursorX, ULONG *cursorY);
VOID CalculateMaxScroll(struct TTTextBuffer *buffer, struct Window *window);
VOID UpdateScrollBars(struct Session *session);

/****************************************************************************/
/* DFN parser */

struct DFNFile;
struct DFNFile *ParseDFNFile(STRPTR fileName);
VOID FreeDFNFile(struct DFNFile *dfn);
struct NewMenu *ConvertDFNToNewMenu(struct DFNFile *dfn, ULONG *outCount);

/****************************************************************************/

#endif /* TTX_DRIVER_H */
