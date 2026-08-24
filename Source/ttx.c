/*
 * TTX - Text Editor for AmigaOS
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_driver.h"
#include "ttx_boopsi.h"
#include "ttx_intui.h"
#include "ttx_texteditor.h"
#include "ttx_prefs.h"
#include "ttx_clipboard.h"
#include "ttx.h"

#include <exec/tasks.h>
#include <exec/libraries.h>

static const char *verstag = "$VER: TTX 3.0 (12/1/2026)\n";
static const char *stack_cookie = "$STACK: 4096\n";

/* Library base for gadtools.lib pragmas (CreateMenus, LayoutMenus, …). */
struct Library *GadToolsBase = NULL;

/* Initialize required libraries */
BOOL TTX_InitLibraries(VOID) {
  Printf("[INIT] TTX_InitLibraries: START\n");

  IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39L);
  if (!IntuitionBase) {
    Printf("[INIT] TTX_InitLibraries: FAIL (intuition.library)\n");
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return FALSE;
  }
  Printf("[INIT] TTX_InitLibraries: intuition.library=%lx\n",
         (ULONG)IntuitionBase);

  UtilityBase = OpenLibrary("utility.library", 39L);
  if (!UtilityBase) {
    Printf("[INIT] TTX_InitLibraries: FAIL (utility.library)\n");
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return FALSE;
  }
  Printf("[INIT] TTX_InitLibraries: utility.library=%lx\n", (ULONG)UtilityBase);

  GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39L);
  if (!GfxBase) {
    Printf("[INIT] TTX_InitLibraries: FAIL (graphics.library)\n");
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return FALSE;
  }
  Printf("[INIT] TTX_InitLibraries: graphics.library=%lx\n", (ULONG)GfxBase);

  IconBase = OpenLibrary("icon.library", 39L);
  if (!IconBase) {
    Printf("[INIT] TTX_InitLibraries: FAIL (icon.library)\n");
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return FALSE;
  }
  Printf("[INIT] TTX_InitLibraries: icon.library=%lx\n", (ULONG)IconBase);

  WorkbenchBase = OpenLibrary("workbench.library", 36L);
  if (!WorkbenchBase) {
    /* Workbench library is optional - app icon support won't work without it */
    Printf("[INIT] TTX_InitLibraries: WARN (workbench.library optional, not "
           "found)\n");
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
  } else {
    Printf("[INIT] TTX_InitLibraries: workbench.library=%lx\n",
           (ULONG)WorkbenchBase);
  }

  /* COMMENTED OUT: Commodities disabled
  CxBase = OpenLibrary("commodities.library", 0L);
  if (!CxBase) {
      // Commodities is optional for single-instance, but preferred
      Printf("[INIT] TTX_InitLibraries: WARN (commodities.library optional, not
  found)\n"); SetIoErr(ERROR_OBJECT_NOT_FOUND); } else { Printf("[INIT]
  TTX_InitLibraries: commodities.library=%lx\n", (ULONG)CxBase);
  }
  */

  /* Open keymap.library for MapRawKey */
  KeymapBase = OpenLibrary("keymap.library", 0L);
  if (!KeymapBase) {
    /* Keymap.library is required for keyboard input conversion */
    Printf("[INIT] TTX_InitLibraries: FAIL (keymap.library)\n");
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return FALSE;
  }
  Printf("[INIT] TTX_InitLibraries: keymap.library=%lx\n", (ULONG)KeymapBase);

  /* Required for CreateMenus / LayoutMenus / GetVisualInfo (menu strip). */
  GadToolsBase = OpenLibrary("gadtools.library", 37L);
  if (!GadToolsBase) {
    Printf("[INIT] TTX_InitLibraries: FAIL (gadtools.library)\n");
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return FALSE;
  }
  Printf("[INIT] TTX_InitLibraries: gadtools.library=%lx\n",
         (ULONG)GadToolsBase);

  /* Open asl.library for file requesters */
  AslBase = OpenLibrary("asl.library", 36L);
  if (!AslBase) {
    /* ASL library is optional - file requesters won't work without it */
    Printf(
        "[INIT] TTX_InitLibraries: WARN (asl.library optional, not found)\n");
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
  } else {
    Printf("[INIT] TTX_InitLibraries: asl.library=%lx\n", (ULONG)AslBase);
  }

  Printf("[INIT] TTX_InitLibraries: SUCCESS\n");
  return TRUE;
}

/* Cleanup libraries - now handled automatically by Seiso cleanup stack */
VOID TTX_CleanupLibraries(VOID) {
  /* Libraries are automatically closed by Seiso cleanup stack */
  /* This function is kept for compatibility but does nothing */
  Printf("[CLEANUP] TTX_CleanupLibraries: (handled by Seiso cleanup stack)\n");
}

/* TurboText-style command line template */
static const char *ttxArgTemplate =
    "FILES/M,STARTUP/K,WINDOW/K,PUBSCREEN/K,SETTINGS/K,DEFINITIONS/K,NOWINDOW/"
    "S,WAIT/S,BACKGROUND/S,UNLOAD/S";

/* Parse command-line arguments - TurboText style */
BOOL TTX_ParseArguments(struct TTXArgs *args) {
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
  args->rda = ReadArgs((STRPTR)ttxArgTemplate, argArray, NULL);
  if (args->rda) {
    /* ReadArgs succeeded - extract arguments */
    /* CRITICAL: Copy file names before freeing RDArgs - they point to memory
     * owned by RDArgs */
    /* ReadArgs allocates memory for /M (multiple) parameters that gets freed
     * with FreeArgs() */
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
        copiedFiles =
            (STRPTR *)TTX_Alloc((fileCount + 1) * sizeof(STRPTR), MEMF_CLEAR);
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
              copy = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
              if (copy) {
                CopyMem(readArgsFiles[i], copy, len);
                copy[len] = '\0';
                copiedFiles[i] = copy;
              } else {
                /* Allocation failed - free what we've allocated so far */
                for (i = 0; i < fileCount && copiedFiles[i]; i++) {
                  TTX_Free(copiedFiles[i]);
                }
                TTX_Free(copiedFiles);
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
        copy = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
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
        copy = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
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
        copy = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
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
        copy = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
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
        copy = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
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
    if (args->rda) {
      FreeArgs(args->rda);
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
    /* Clear error code after handling to prevent interference with subsequent
     * operations */
    SetIoErr(0);
    /* Ensure all args fields remain NULL/0 (already cleared at start) */
    args->rda = NULL;
    /* No arguments is OK - we'll open default window */
    result = TRUE;
  }

  return result;
}

/* Parse ToolTypes from icon - called when argc == 0 (Workbench launch) */
BOOL TTX_ParseToolTypes(STRPTR *fileName, struct WBStartup *wbMsg) {
  struct DiskObject *icon = NULL;
  STRPTR *toolTypes = NULL;
  STRPTR fileArg = NULL;
  ULONG i = 0;
  BOOL result = FALSE;

  if (!fileName || !wbMsg) {
    return FALSE;
  }

  *fileName = NULL;

  /* Get icon for this program using cleanup stack */
  /* Clear IoErr() before GetDiskObject to ensure clean state */
  SetIoErr(0);
  icon = GetDiskObject(wbMsg->sm_ArgList[0].wa_Name);
  if (!icon) {
    /* GetDiskObject failed - check error code and clear it */
    LONG errorCode = IoErr();
    if (errorCode != 0) {
      /* Clear error to prevent icon.library from being left in undefined state
       */
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
      if (toolTypes[i][0] == 'F' && toolTypes[i][1] == 'I' &&
          toolTypes[i][2] == 'L' && toolTypes[i][3] == 'E' &&
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
    copy = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
    if (copy) {
      Printf("[INIT] TTX_ParseToolTypes: allocated copy=%lx\n", (ULONG)copy);
      CopyMem(fileArg, copy, len);
      copy[len] = '\0';
      *fileName = copy;
      result = TRUE;
    }
  }

  /* Remove DiskObject from tracking and free it explicitly */
  FreeDiskObject(icon);

  return result;
}

/* Check if another instance is already running */
BOOL TTX_CheckExistingInstance(STRPTR fileName) {
  struct MsgPort *existingPort = NULL;
  BOOL result = FALSE;
  struct Task *sigTask = NULL;

  /* Try to find existing message port */
  /* Note: FindPort() MUST be called with Forbid()/Permit() protection */
  Forbid();
  existingPort = FindPort(TTX_MESSAGE_PORT_NAME);
  if (existingPort) {
    /* Basic validation: check port structure looks valid */
    if (existingPort->mp_Node.ln_Type != NT_MSGPORT ||
        existingPort->mp_SigBit == (UBYTE)-1) {
      existingPort = NULL;
    } else {
      /*
       * Stale port from a process that exited without RemPort: SigTask
       * points at freed memory. RemPort the zombie so we do not PutMsg it
       * (that hang) and so AddPort can reuse TTX.1.
       */
      sigTask = existingPort->mp_SigTask;
      if (!sigTask ||
          (sigTask->tc_Node.ln_Type != NT_TASK &&
           sigTask->tc_Node.ln_Type != NT_PROCESS)) {
        Printf("[INIT] TTX_CheckExistingInstance: RemPort stale %s\n",
               TTX_MESSAGE_PORT_NAME);
        RemPort(existingPort);
        existingPort = NULL;
      }
    }
  }
  Permit();

  if (existingPort) {
    /* Another instance is running, send message to it */
    if (fileName) {
      result =
          TTX_SendToExistingInstance( TTX_MSG_OPEN_FILE, fileName);
    } else {
      result = TTX_SendToExistingInstance( TTX_MSG_OPEN_NEW, NULL);
    }
  }

  return result;
}

/* Send message to existing instance */
BOOL TTX_SendToExistingInstance(ULONG msgType, STRPTR fileName) {
  struct MsgPort *existingPort = NULL;
  struct TTXMessage *msg = NULL;
  ULONG fileNameLen = 0;
  STRPTR fileNameCopy = NULL;
  BOOL result = FALSE;

  /* Find existing message port */
  Forbid();
  existingPort = FindPort(TTX_MESSAGE_PORT_NAME);
  /* Validate port structure before Permit() - check it looks like a valid
   * MsgPort */
  if (existingPort) {
    /* Basic validation: check port structure looks valid */
    /* NT_MSGPORT is 4, and valid ports should have a signal bit */
    if (existingPort->mp_Node.ln_Type != NT_MSGPORT ||
        existingPort->mp_SigBit == 0) {
      /* Port structure looks invalid - might be stale pointer */
      existingPort = NULL;
    }
  }
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
  /* Note: Once sent, the receiving instance will free it, so we remove it from
   * tracking */
  msg = (struct TTXMessage *)TTX_Alloc(sizeof(struct TTXMessage), MEMF_CLEAR);
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
    fileNameCopy = (STRPTR)TTX_Alloc(fileNameLen + 1, MEMF_CLEAR);
    if (fileNameCopy) {
      Printf("[INIT] TTX_SendToExistingInstance: allocated fileNameCopy=%lx\n",
             (ULONG)fileNameCopy);
      CopyMem(fileName, fileNameCopy, fileNameLen);
      fileNameCopy[fileNameLen] = '\0';
      msg->fileName = fileNameCopy;
    } else {
      Printf("[CLEANUP] TTX_SendToExistingInstance: freeing msg=%lx (error "
             "path)\n",
             (ULONG)msg);
      TTX_Free(msg);
      return FALSE;
    }
  }

  /* Send message - for one-way messages (mn_ReplyPort=NULL), receiver must free
   * after ReplyMsg() */
  /* According to Exec docs: ALL messages must be replied to with ReplyMsg() */
  /* If mn_ReplyPort is NULL, ReplyMsg() does nothing, but receiver still calls
   * it */
  /* Receiver will free the message after calling ReplyMsg() */
  PutMsg(existingPort, (struct Message *)msg);
  result = TRUE;

  return result;
}

/* Setup message port for single-instance operation */
BOOL TTX_SetupMessagePort(struct TTXApplication *app) {
  Printf("[INIT] TTX_SetupMessagePort: START\n");
  if (!app) {
    Printf("[INIT] TTX_SetupMessagePort: FAIL (app=NULL)\n");
    return FALSE;
  }

  app->appPort = CreateMsgPort();
  if (!app->appPort) {
    Printf("[INIT] TTX_SetupMessagePort: FAIL (createMsgPort failed)\n");
    return FALSE;
  }

  /* DON'T set port name yet - we'll set it and add it after checking for
   * existing instances */
  /* This prevents seiso's cleanupPort from trying to remove an unadded port on
   * abnormal exit */

  Printf("[INIT] TTX_SetupMessagePort: SUCCESS (port=%lx, name will be %s, not "
         "yet added)\n",
         (ULONG)app->appPort, TTX_MESSAGE_PORT_NAME);
  return TRUE;
}

/* Add message port to system (call this after checking for existing instances)
 */
BOOL TTX_AddMessagePort(struct TTXApplication *app) {
  if (!app || !app->appPort) {
    return FALSE;
  }

  /* Set name and add port to system port list so FindPort() can find it */
  /* This must be done together under Forbid() */
  Forbid();
  app->appPort->mp_Node.ln_Name = TTX_MESSAGE_PORT_NAME;
  AddPort(app->appPort);
  Permit();

  Printf("[INIT] TTX_AddMessagePort: port named and added to system\n");
  return TRUE;
}

/* Remove named application port from the system before freeing it (RemPort) */
VOID TTX_RemoveMessagePort(struct TTXApplication *app) {
  if (!app || !app->appPort)
    return;

  Forbid();
  /* RemPort by pointer; clear name after unlink so FindPort cannot see us. */
  RemPort(app->appPort);
  app->appPort->mp_Node.ln_Name = NULL;
  Permit();
}

/* COMMENTED OUT: Commodities disabled
/* Setup commodity for single-instance (appears in Exchange) */
BOOL TTX_SetupCommodity(struct TTXApplication *app) {
  struct NewBroker nb;
  LONG brokerError = 0;
  BOOL result = FALSE;

  Printf("[INIT] TTX_SetupCommodity: START\n");
  if (!app || !CxBase) {
    Printf("[INIT] TTX_SetupCommodity: FAIL (app=%lx, CxBase=%lx)\n",
           (ULONG)app, (ULONG)CxBase);
    return FALSE;
  }

  app->brokerPort = CreateMsgPort();
  if (!app->brokerPort) {
    Printf("[INIT] TTX_SetupCommodity: FAIL (createMsgPort failed)\n");
    return FALSE;
  }
  Printf("[INIT] TTX_SetupCommodity: brokerPort=%lx\n", (ULONG)app->brokerPort);

  // Create broker using Seiso - mirrors CxBroker() API
  // COF_SHOW_HIDE enables show/hide commands from Exchange
  // NBU_UNIQUE | NBU_NOTIFY: single instance, notify on duplicate
  {
    struct NewBroker nb;
    LONG brokerError = 0;

    // Initialize NewBroker structure
    nb.nb_Version = NB_VERSION;
    nb.nb_Name = (STRPTR) "TTX";
    nb.nb_Title = (STRPTR) "TTX";
    nb.nb_Descr = (STRPTR) "Text Editor";
    nb.nb_Unique = NBU_UNIQUE | NBU_NOTIFY; // Unique name, notify on duplicate
    nb.nb_Flags = COF_SHOW_HIDE;            // Support show/hide commands
    nb.nb_Pri = 0;                          // Normal priority
    nb.nb_Port = app->brokerPort;
    nb.nb_ReservedChannel = 0;

    Printf("[INIT] TTX_SetupCommodity: creating broker with COF_SHOW_HIDE\n");
    app->broker = cxBroker(&nb, &brokerError);
    if (!app->broker) {
      // Broker creation failed - could be duplicate instance or other error
      Printf("[INIT] TTX_SetupCommodity: FAIL (CxBroker failed, error=%ld)\n",
             brokerError);
      // Duplicate instance detection is handled by TTX_CheckExistingInstance
      DeleteMsgPort(app->brokerPort);
      app->brokerPort = NULL;
      return FALSE;
    }
  }

  // CRITICAL: Verify broker is actually new before activation
  // When NBU_UNIQUE is set, CxBroker() should return NULL with CBERR_DUP if a
  // duplicate exists However, if DeleteCxObjAll() didn't properly remove the
  // broker from the Exchange list, CxBroker() might return the same broker
  // pointer with error=0, and the broker might be inactive but still in the
  // Exchange list. We must detect this case and fail gracefully.
  //
  // The way to detect a stale broker is to check if ActivateCxObj() returns
  // non-zero, indicating the broker was already active. But if the broker is
  // inactive, we can't reliably detect it without activating it first, which
  // would change its state.
  //
  // However, we can check the broker's type and error state to verify it's
  // valid. If the broker has unexpected errors or is in an invalid state, it
  // might be stale.
  {
    LONG brokerError;
    LONG prevActivationState;
    LONG brokerType = 0;

    // First check broker type - should be CX_BROKER for a valid broker
    brokerType = CxObjType(app->broker);
    if (brokerType != CX_BROKER) {
      Printf("[INIT] TTX_SetupCommodity: FAIL (broker has invalid type=%ld, "
             "expected CX_BROKER)\n",
             brokerType);
      Printf("[INIT] TTX_SetupCommodity: This indicates a stale or corrupted "
             "broker from a previous run\n");
      // Untrack the broker from cleanup stack - we didn't create it
      if (app->broker) {
        app->broker = NULL;
      }
      // Port will be cleaned up by cleanup stack on exit
      return FALSE;
    }

    // Check for broker errors before attempting activation
    // This can help detect if the broker is in an invalid state
    brokerError = CxObjError(app->broker);
    if (brokerError != 0) {
      Printf("[INIT] TTX_SetupCommodity: WARN (broker has errors before "
             "activation, error=0x%08lx)\n",
             (ULONG)brokerError);
      // Continue anyway - errors might be non-fatal (e.g., COERR_BADFILTER on
      // attached objects)
    }

    // Activate the broker - should return 0 for a newly created (inactive)
    // broker CRITICAL: Flush output before potentially hanging call
    Flush(Output());
    prevActivationState = ActivateCxObj(app->broker, TRUE);
    Flush(Output());

    if (prevActivationState != 0) {
      // Broker was already active - this means CxBroker() returned an
      // existing/stale broker This should not happen with NBU_UNIQUE, but
      // handle it defensively CRITICAL: We CANNOT use or clean up a broker we
      // didn't create We must fail gracefully because the exclusive name is
      // already taken
      Printf("[INIT] TTX_SetupCommodity: FAIL (broker already active, "
             "prevState=%ld)\n",
             prevActivationState);
      Printf("[INIT] TTX_SetupCommodity: broker name 'TTX' is already in use - "
             "cannot proceed\n");
      Printf("[INIT] TTX_SetupCommodity: This indicates a stale broker from a "
             "previous run was not properly cleaned up\n");
      // Deactivate it back to original state
      Flush(Output());
      ActivateCxObj(app->broker, FALSE);
      Flush(Output());
      // Untrack the broker from cleanup stack - we didn't create it
      if (app->broker) {
        app->broker = NULL;
      }
      // Port will be cleaned up by cleanup stack on exit
      return FALSE;
    }
    // Broker was inactive and is now activated - this is correct for a newly
    // created broker
    Printf("[INIT] TTX_SetupCommodity: broker activated successfully (was "
           "inactive, now active)\n");
  }

  // Check for any broker errors
  if (CxObjError(app->broker) != 0) {
    Printf("[INIT] TTX_SetupCommodity: WARN (broker has errors, continuing)\n");
  }

  result = TRUE;
  Printf("[INIT] TTX_SetupCommodity: SUCCESS (broker=%lx)\n",
         (ULONG)app->broker);
  return result;
}
*/

    /* Remove commodity - now handled automatically by cleanup stack */
    VOID TTX_RemoveCommodity(struct TTXApplication *app) {
  /* Resources are automatically cleaned up by Seiso cleanup stack */
  /* This function is kept for compatibility but does nothing */
  /* Messages are cleaned up in TTX_Cleanup before stack deletion */
  Printf("[CLEANUP] TTX_RemoveCommodity: (handled by Seiso cleanup stack)\n");
}

/* Setup app icon for application-level iconification */
BOOL TTX_SetupAppIcon(struct TTXApplication *app) {
  Printf("[INIT] TTX_SetupAppIcon: START\n");
  if (!app || !WorkbenchBase || !IconBase) {
    Printf("[INIT] TTX_SetupAppIcon: FAIL (app=%lx, WorkbenchBase=%lx, "
           "IconBase=%lx)\n",
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
VOID TTX_RemoveAppIcon(struct TTXApplication *app) {
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
    /* Use deleteMsgPort to untrack from cleanup stack */
    DeleteMsgPort(app->appIconPort);
    app->appIconPort = NULL;
  }

  app->iconified = FALSE;

  Printf("[CLEANUP] TTX_RemoveAppIcon: DONE\n");
}

/* Deferred iconification - sets flag for main loop to process */
VOID TTX_Iconify(struct TTXApplication *app, BOOL iconify) {
  if (!app) {
    return;
  }

  Printf("[ICONIFY] TTX_Iconify: deferring iconify=%s\n",
         iconify ? "TRUE" : "FALSE");
  app->iconifyDeferred = TRUE;
  app->iconifyState = iconify;
}

/* Save window state before closing */
BOOL TTX_SaveWindowState(struct Session *session) {
  if (!session || !session->window) {
    return FALSE;
  }

  Printf("[WINDOW] TTX_SaveWindowState: saving state for session %lu\n",
         session->sessionID);

  /* Save window position and size */
  session->windowState.leftEdge = session->window->LeftEdge;
  session->windowState.topEdge = session->window->TopEdge;
  session->windowState.innerWidth = session->window->Width -
                                    session->window->BorderLeft -
                                    session->window->BorderRight;
  session->windowState.innerHeight = session->window->Height -
                                     session->window->BorderTop -
                                     session->window->BorderBottom;
  session->windowState.flags = session->window->Flags;

  Printf("[WINDOW] TTX_SaveWindowState: saved pos=(%ld,%ld) size=(%lu,%lu) "
         "flags=0x%08lx\n",
         session->windowState.leftEdge, session->windowState.topEdge,
         session->windowState.innerWidth, session->windowState.innerHeight,
         session->windowState.flags);

  return TRUE;
}

/*
 * Restore window from saved state
 */
BOOL TTX_RestoreWindow(struct TTXApplication *app, struct Session *session) {
  struct Screen *screen = NULL;

  if (!app || !session) {
    Printf("[WINDOW] TTX_RestoreWindow: FAIL (app=%lx, session=%lx)\n",
           (ULONG)app, (ULONG)session);
    return FALSE;
  }

  if (session->window) {
    Printf("[WINDOW] TTX_RestoreWindow: window already open for session %lu\n",
           session->sessionID);
    return TRUE;
  }

  Printf("[WINDOW] TTX_RestoreWindow: restoring window for session %lu\n",
         session->sessionID);

  screen = LockPubScreen(session->windowState.pubScreenName
                             ? session->windowState.pubScreenName
                             : (STRPTR) "Workbench");
  if (!screen) {
    Printf("[WINDOW] TTX_RestoreWindow: FAIL (LockPubScreen failed)\n");
    return FALSE;
  }

  if (!TTX_IntuiOpenWindow(session, screen)) {
    UnlockPubScreen(session->windowState.pubScreenName
                        ? session->windowState.pubScreenName
                        : (STRPTR) "Workbench",
                    screen);
    Printf("[WINDOW] TTX_RestoreWindow: FAIL (openWindow failed)\n");
    return FALSE;
  }

  UnlockPubScreen(session->windowState.pubScreenName
                      ? session->windowState.pubScreenName
                      : (STRPTR) "Workbench",
                  screen);

  /* Recreate menu strip */
  if (!TTX_CreateMenuStrip(session)) {
    Printf("[WINDOW] TTX_RestoreWindow: WARN (CreateMenuStrip failed)\n");
  }

  /* Text editor first (client clip), then scroll props chain it into AddGList. */
  {
    struct DrawInfo *drawInfo = NULL;

    if (!TTX_TextEditor_CreateGadget(app, session))
      Printf("[WINDOW] TTX_RestoreWindow: WARN (text editor gadget failed)\n");

    drawInfo = GetScreenDrawInfo(session->window->WScreen);
    if (drawInfo)
    {
      if (!TTX_BoopsiCreateScrollGadgets(session, session->window, drawInfo))
        Printf("[WINDOW] TTX_RestoreWindow: WARN (scroll gadgets failed)\n");
      FreeScreenDrawInfo(session->window->WScreen, drawInfo);
    }
  }

  /* Update scroll bars with current buffer state */
  if (TT_SessionBuffer(session)) {
    CalculateMaxScroll(session, session->window);
    UpdateScrollBars(session);
    TTX_RequestRedraw(session);
  }

  if (session->window) {
    ActivateWindow(session->window);
    /* Keys via IDCMP; ActivateGadget would trap input until click-release. */
  }

  session->windowState.windowOpen = TRUE;

  Printf("[WINDOW] TTX_RestoreWindow: SUCCESS (window=%lx)\n",
         (ULONG)session->window);
  TTX_RebuildSignalMask(app);
  return TRUE;
}

/* Perform actual iconification/uniconification */
VOID TTX_DoIconify(struct TTXApplication *app, BOOL iconify) {
  struct Session *session = NULL;
  STRPTR programName = NULL;
  STRPTR iconName = NULL;

  if (!app) {
    return;
  }

  Printf(
      "[ICONIFY] TTX_DoIconify: START (iconify=%s, currently iconified=%s)\n",
      iconify ? "TRUE" : "FALSE", app->iconified ? "TRUE" : "FALSE");

  if (iconify && !app->iconified) {
    /* Iconify: Close all windows, create app icon */
    Printf("[ICONIFY] TTX_DoIconify: iconifying application\n");

    /* Save window state and close all windows but keep sessions alive */
    session = app->sessions;
    while (session) {
      if (session->window) {
        Printf("[ICONIFY] TTX_DoIconify: saving state and closing window for "
               "session %lu\n",
               session->sessionID);
        TTX_SaveWindowState(session);
        TTX_CloseSessionWindow(app, session, NULL);
        session->windowState.windowOpen = FALSE;
      }
      session = session->next;
    }

    TTX_RebuildSignalMask(app);

    /* Create message port for app icon using cleanup stack */
    if (!app->appIconPort) {
      app->appIconPort = CreateMsgPort();
      if (!app->appIconPort) {
        Printf("[ICONIFY] TTX_DoIconify: FAIL (createMsgPort failed)\n");
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
      Printf("[ICONIFY] TTX_DoIconify: created appIconPort=%lx (tracked on "
             "cleanup stack)\n",
             (ULONG)app->appIconPort);
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
        app->appIconDO = GetIconTags(programName, ICONGETA_FailIfUnavailable,
                                     FALSE, TAG_END);
      } else {
        /* Fall back to GetDiskObjectNew for older versions */
        app->appIconDO = GetDiskObjectNew(programName);
      }

      if (!app->appIconDO) {
        Printf("[ICONIFY] TTX_DoIconify: WARN (could not get disk object, "
               "using default)\n");
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

    Printf("[ICONIFY] TTX_DoIconify: calling AddAppIcon (name='%s', port=%lx, "
           "diskObj=%lx)\n",
           iconName, (ULONG)app->appIconPort, (ULONG)app->appIconDO);
    app->appIcon = AddAppIcon(0, 0, iconName, app->appIconPort, NULL,
                              app->appIconDO, TAG_END);
    if (!app->appIcon) {
      LONG errorCode = 0;
      errorCode = IoErr();
      Printf("[ICONIFY] TTX_DoIconify: FAIL (AddAppIcon failed, IoErr=%ld)\n",
             errorCode);
      /* Cleanup and reopen windows */
      if (app->appIconDO) {
        FreeDiskObject(app->appIconDO);
        app->appIconDO = NULL;
      }
      /* Use deleteMsgPort to untrack from cleanup stack */
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
    Printf("[ICONIFY] TTX_DoIconify: created appIcon=%lx\n",
           (ULONG)app->appIcon);

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

    /* Delete message port (tracked on cleanup stack, use deleteMsgPort) */
    if (app->appIconPort) {
      struct Message *msg = NULL;
      /* Clean up any pending messages */
      Forbid();
      while ((msg = GetMsg(app->appIconPort)) != NULL) {
        ReplyMsg(msg);
      }
      Permit();
      /* Use deleteMsgPort to untrack from cleanup stack */
      DeleteMsgPort(app->appIconPort);
      app->appIconPort = NULL;
    }

    /* Reopen all windows */
    session = app->sessions;
    while (session) {
      if (!session->window) {
        Printf("[ICONIFY] TTX_DoIconify: restoring window for session %lu\n",
               session->sessionID);
        if (!TTX_RestoreWindow(app, session)) {
          Printf("[ICONIFY] TTX_DoIconify: WARN (failed to restore window for "
                 "session %lu)\n",
                 session->sessionID);
        } else {
          /* Refresh display after restoring window */
          if (TT_SessionBuffer(session)) {
            RenderText(session->window, session);
            UpdateCursor(session->window, session);
            UpdateScrollBars(session);
          }
        }
      }
      session = session->next;
    }

    TTX_RebuildSignalMask(app);
    app->iconified = FALSE;
    Printf("[ICONIFY] TTX_DoIconify: SUCCESS (uniconified)\n");
  } else {
    Printf("[ICONIFY] TTX_DoIconify: no change needed (iconify=%s, "
           "iconified=%s)\n",
           iconify ? "TRUE" : "FALSE", app->iconified ? "TRUE" : "FALSE");
  }
}

/* Process app icon messages (double-click, file drops) */
VOID TTX_ProcessAppIcon(struct TTXApplication *app) {
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
    Printf("[ICONIFY] TTX_ProcessAppIcon: received message (am_NumArgs=%lu)\n",
           msg->am_NumArgs);

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
          fullPath = (STRPTR)TTX_Alloc(pathLen, MEMF_CLEAR);
          if (fullPath) {
            /* Get path from lock */
            if (NameFromLock(msg->am_ArgList[i].wa_Lock, fullPath, pathLen)) {
              /* Add filename */
              if (AddPart(fullPath, msg->am_ArgList[i].wa_Name, pathLen)) {
                Printf("[ICONIFY] TTX_ProcessAppIcon: opening file '%s'\n",
                       fullPath);
                if (TTX_CreateSession(app, fullPath)) {
                  /* Rebuild signal mask after creating new session */
                  TTX_RebuildSignalMask(app);
                }
                /* fullPath is copied by TTX_CreateSession, so we can free it
                 * now */
                TTX_Free(fullPath);
                fullPath = NULL;
              } else {
                Printf("[ICONIFY] TTX_ProcessAppIcon: WARN (AddPart failed)\n");
                TTX_Free(fullPath);
                fullPath = NULL;
              }
            } else {
              Printf(
                  "[ICONIFY] TTX_ProcessAppIcon: WARN (NameFromLock failed)\n");
              TTX_Free(fullPath);
              fullPath = NULL;
            }
          }

          /* Free fullPath if it wasn't freed above */
          if (fullPath) {
            TTX_Free(fullPath);
            fullPath = NULL;
          }

          /* Restore original directory */
          if (oldDir) {
            CurrentDir(oldDir);
          }
        } else if (msg->am_ArgList[i].wa_Name) {
          /* Just a name, no lock - use as-is */
          Printf("[ICONIFY] TTX_ProcessAppIcon: opening file '%s'\n",
                 msg->am_ArgList[i].wa_Name);
          if (TTX_CreateSession(app, msg->am_ArgList[i].wa_Name)) {
            /* Rebuild signal mask after creating new session */
            TTX_RebuildSignalMask(app);
          }
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
BOOL TTX_CreateSession(struct TTXApplication *app, STRPTR fileName) {
  struct TTDocument *doc = NULL;

  if (!app || !TurboTextBase)
    return FALSE;

  doc = TT_OpenDocument(fileName);
  if (!doc)
    return FALSE;

  return TTX_CreateSessionForDocument(app, doc, fileName);
}

BOOL TTX_CreateSessionForDocument(struct TTXApplication *app, struct TTDocument *doc, STRPTR fileName) {
  struct Session *session = NULL;
  struct Screen *screen = NULL;
  STRPTR titleText = NULL;
  ULONG titleLen = 0;
  BOOL result = FALSE;

  Printf("[INIT] TTX_CreateSession: START (fileName=%s)\n",
         fileName ? fileName : (STRPTR) "(null)");
  if (!app || !doc) {
    Printf("[INIT] TTX_CreateSession: FAIL (app=%lx, doc=%lx)\n", (ULONG)app,
           (ULONG)doc);
    return FALSE;
  }

  session = (struct Session *)TTX_Alloc(sizeof(struct Session), MEMF_CLEAR);
  if (!session) {
    Printf("[INIT] TTX_CreateSession: FAIL (TTX_Alloc session failed)\n");
    return FALSE;
  }
  Printf("[INIT] TTX_CreateSession: session=%lx\n", (ULONG)session);

  /* Initialize session */
  session->sessionID = app->nextSessionID++;
  session->window = NULL;
  session->menuStrip = NULL;             /* Will be created after window is opened */
  session->menuVisualInfo = NULL;         /* Set when menu created; free after CloseWindow() */
  session->menuNewMenuBacking = NULL;    /* Set when using DFN menu (freed with menu) */
  session->menuDFNBacking = NULL;        /* Set when using DFN menu (freed with menu) */
  session->scroll.vertProp = NULL;
  session->scroll.horizProp = NULL;
  session->scroll.vertUp = NULL;
  session->scroll.vertDown = NULL;
  session->scroll.horizLeft = NULL;
  session->scroll.horizRight = NULL;
  session->scroll.gadgetHead = NULL;
  session->scroll.gadgetsOnWindow = FALSE;
  session->scroll.drawInfo = NULL;
  session->scroll.screen = NULL;
  session->scroll.suppressIcmp = FALSE;
  
  session->mouseSelecting = FALSE; /* Mouse selection state */
  session->selectStartX = 0;
  session->selectStartY = 0;
  session->next = NULL; /* Initialize list pointers */
  session->prev = NULL;
  session->document = doc;
  session->render.superBitMap = NULL;
  session->render.needsFullRedraw = TRUE;
  session->render.cursorVisible = FALSE;
  session->render.cursorPixelX = 0;
  session->render.cursorPixelY = 0;
  session->render.cursorPixelW = 0;
  session->render.cursorPixelH = 0;
  session->splitY = 0;
  session->splitRatio = 0;
  session->paneClipTop = 0;
  session->paneClipBottom = 0;
  session->paneClipActive = FALSE;
  session->arexxPortName[0] = '\0';
  TTX_ArexxBindSession(app, session);
  if (doc->activeView)
    doc->activeView->uiBinding = session;

  /* Initialize window state with defaults */
  session->windowState.leftEdge = 50; /* Default position */
  session->windowState.topEdge = 50;
  session->windowState.innerWidth = 600; /* Default size */
  session->windowState.innerHeight = 400;
  session->windowState.flags =
      WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SIZEGADGET | WFLG_SIZEBRIGHT |
      WFLG_SIZEBBOTTOM | WFLG_CLOSEGADGET | WFLG_SIMPLE_REFRESH |
      WFLG_NEWLOOKMENUS | WFLG_REPORTMOUSE;
  /*
   * OpenWindow autodoc: only the first window should use WFLG_ACTIVATE so later
   * windows do not steal keyboard focus from the user's active task.
   */
  if (app->sessionCount == 0)
    session->windowState.flags |= WFLG_ACTIVATE;
  session->windowState.idcmpFlags =
      IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY | IDCMP_RAWKEY |
      IDCMP_REFRESHWINDOW | IDCMP_NEWSIZE | IDCMP_CHANGEWINDOW |
      IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_MENUPICK |
      IDCMP_GADGETUP | IDCMP_IDCMPUPDATE | IDCMP_ACTIVEWINDOW |
      IDCMP_INACTIVEWINDOW;
  session->windowState.title = NULL;
  session->windowState.screenTitle = NULL;
  session->windowState.pubScreenName = NULL;
  session->windowState.minWidth = 200;
  session->windowState.minHeight = 100;
  session->windowState.maxWidth = 32767;
  session->windowState.maxHeight = 32767;
  session->windowState.windowOpen = FALSE;

  /* Document state owned by turbotext.library */


  /* Create window title */
  if (session->document->state.fileName) {
    titleLen = 0;
    while (session->document->state.fileName[titleLen] != '\0') {
      titleLen++;
    }
  } else {
    titleLen = 8; /* "Untitled" */
  }

  titleText = (STRPTR)TTX_Alloc(titleLen + 20, MEMF_CLEAR);
  if (titleText) {
    Printf("[INIT] TTX_CreateSession: allocated titleText=%lx\n",
           (ULONG)titleText);
    if (session->document->state.fileName) {
      CopyMem(session->document->state.fileName, titleText, titleLen);
      titleText[titleLen] = '\0';
    } else {
      CopyMem("Untitled", titleText, 8);
      titleText[8] = '\0';
    }
    /* Store title in window state */
    session->windowState.title = titleText;
  }

  /* Store screen title */
  session->windowState.screenTitle = (STRPTR)TTX_Alloc(4, MEMF_CLEAR);
  if (session->windowState.screenTitle) {
    CopyMem("TTX", session->windowState.screenTitle, 3);
    session->windowState.screenTitle[3] = '\0';
  }

  /* Lock public screen (temporary, doesn't need tracking) */
  screen = LockPubScreen((STRPTR) "Workbench");
  if (!screen) {
    Printf("[INIT] TTX_CreateSession: FAIL (LockPubScreen failed)\n");
    /* Free session-owned allocations (all tracked on cleanup stack) */
    if (session->windowState.title) {
      TTX_Free(session->windowState.title);
      session->windowState.title = NULL;
    }
    if (session->windowState.screenTitle) {
      TTX_Free(session->windowState.screenTitle);
      session->windowState.screenTitle = NULL;
    }
    if (TT_SessionBuffer(session)) {
      
    }
    TTX_Free(session);
    return FALSE;
  }

  /* Open window using global cleanup stack */
  Printf("[INIT] TTX_CreateSession: opening window\n");
  if (!TTX_IntuiOpenWindow(session, screen)) {
    Printf("[INIT] TTX_CreateSession: FAIL (openWindow failed)\n");
    /* Free session-owned allocations (all tracked on cleanup stack) */
    if (session->windowState.title) {
      TTX_Free(session->windowState.title);
      session->windowState.title = NULL;
    }
    if (session->windowState.screenTitle) {
      TTX_Free(session->windowState.screenTitle);
      session->windowState.screenTitle = NULL;
    }
    if (TT_SessionBuffer(session)) {
      
    }
    TTX_Free(session);
    return FALSE;
  }

  UnlockPubScreen((STRPTR) "Workbench", screen);

  Printf("[INIT] TTX_CreateSession: window=%lx\n", (ULONG)session->window);
  if (session->window->UserPort) {
    Printf("[INIT] TTX_CreateSession: Intuition UserPort=%lx sigbit=%lu\n",
           (ULONG)session->window->UserPort,
           session->window->UserPort->mp_SigBit);
  }

  Printf("[INIT] TTX_CreateSession: window flags=%lx (WFLG_DRAGBAR=%s WFLG_DEPTHGADGET=%s)\n",
         (ULONG)session->window->Flags,
         (session->window->Flags & WFLG_DRAGBAR) ? "YES" : "NO",
         (session->window->Flags & WFLG_DEPTHGADGET) ? "YES" : "NO");

  /* Set window limits already done in TTX_IntuiOpenWindow */

  /*
   * Buffer starts with pageH/pageW = 0. propgclass NewObject() with PGA_Visible
   * 0 can wedge Intuition; compute page metrics before any BOOPSI props.
   */
  if (TT_SessionBuffer(session) && session->window)
    CalculateMaxScroll(session, session->window);

  /* Menu strip before scroll gadgets */
  if (!TTX_CreateMenuStrip(session)) {
    Printf("[INIT] TTX_CreateSession: WARN (menu creation failed, continuing "
           "without menu)\n");
  }

  /* Text editor first (client clip), then scroll props chain it into AddGList. */
  {
    struct DrawInfo *drawInfo = NULL;

    if (!TTX_TextEditor_CreateGadget(app, session))
      Printf("[INIT] TTX_CreateSession: WARN (text editor gadget failed - "
             "using window IDCMP input)\n");

    drawInfo = GetScreenDrawInfo(session->window->WScreen);
    if (drawInfo)
    {
      if (!TTX_BoopsiCreateScrollGadgets(session, session->window, drawInfo))
        Printf("[INIT] TTX_CreateSession: WARN (scroll gadgets failed)\n");
      FreeScreenDrawInfo(session->window->WScreen, drawInfo);
    }
  }

  /* Calculate max scroll values and update scroll bars */
  if (TT_SessionBuffer(session)) {
    CalculateMaxScroll(session, session->window);
    UpdateScrollBars(session);
  }

  /* Initial render */
  if (TT_SessionBuffer(session)) {
    TTX_RequestRedraw(session);
  }

  /* Add to session list */
  session->prev = NULL; /* New session is always first in list */
  if (app->sessions) {
    session->next = app->sessions;
    app->sessions->prev = session;
  } else {
    session->next = NULL; /* Explicitly set if first session */
  }
  app->sessions = session;
  app->sessionCount++;
  app->activeSession = session;

  /* Ensure keyboard focus and cursor on the new window */
  if (session->window) {
    ActivateWindow(session->window);
    /* Do not ActivateGadget here - that leaves the editor sticky-active
     * and blocks menus/close until a click cycle. Keys use window IDCMP.
     */
    if (TT_SessionBuffer(session)) {
      UpdateCursor(session->window, session);
    }
  }

  result = TRUE;
  Printf("[INIT] TTX_CreateSession: SUCCESS (sessionID=%lu, window=%lx)\n",
         session->sessionID, (ULONG)session->window);
  TTX_RebuildSignalMask(app);
  return result;
}

/* Close a session window using the safe Intuition teardown sequence */
VOID TTX_CloseSessionWindow(struct TTXApplication *app, struct Session *session,
                            struct Window *closedMark) {
  if (!session || !session->window || session->window == INVALID_RESOURCE)
    return;

  TTX_IntuiCloseWindow(app, session);
  session->window = closedMark;
}

/* Request session destroy; defer if still inside an IDCMP handler */
VOID
TTX_RequestDestroySession(struct TTXApplication *app, struct Session *session)
{
  if (!app || !session)
    return;

  if (app->intuiHandlerDepth > 0) {
    app->deferredCloseSession = session;
    return;
  }

  TTX_DestroySession(app, session);
  TTX_RebuildSignalMask(app);
}

/* Destroy a session */
VOID TTX_DestroySession(struct TTXApplication *app, struct Session *session) {
  Printf("[CLEANUP] TTX_DestroySession: START (session=%lx, sessionID=%lu)\n",
         (ULONG)session, session ? session->sessionID : 0);
  if (!app || !session) {
    Printf("[CLEANUP] TTX_DestroySession: DONE (app=%lx, session=%lx)\n",
           (ULONG)app, (ULONG)session);
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

  /* Note: each window owns its Intuition UserPort (WA_IDCMP at open time). */

  /* Update active session BEFORE freeing session structure */
  if (app->activeSession == session) {
    app->activeSession = app->sessions;
  }

  /* Decrement session count BEFORE freeing */
  app->sessionCount--;

  /* Close window with safe Intuition teardown (menu, gadgets, IDCMP, port) */
  if (session->window && session->window != INVALID_RESOURCE) {
    Printf("[CLEANUP] TTX_DestroySession: closing window=%lx\n",
           (ULONG)session->window);
    TTX_CloseSessionWindow(app, session, (struct Window *)INVALID_RESOURCE);
    Printf("[CLEANUP] TTX_DestroySession: closeWindow completed\n");
  }

  /* Close document via turbotext.library (frees buffer and metadata) */
  if (session->document && TurboTextBase)
  {
    TT_CloseDocument(session->document);
    session->document = NULL;
  }

  /* Free session string allocations */
  if (session->windowState.title) {
    TTX_Free(session->windowState.title);
    session->windowState.title = NULL;
  }
  if (session->windowState.screenTitle) {
    TTX_Free(session->windowState.screenTitle);
    session->windowState.screenTitle = NULL;
  }

  Printf("[CLEANUP] TTX_DestroySession: freeing session=%lx\n", (ULONG)session);
  TTX_Free(session);
  Printf("[CLEANUP] TTX_DestroySession: DONE (remaining sessions=%lu)\n",
         app->sessionCount);
  TTX_RebuildSignalMask(app);
}

/* COMMENTED OUT: Commodities disabled
/* Handle commodity message (from Exchange or other instances) */
BOOL TTX_HandleCommodityMessage(struct TTXApplication *app,
                                struct Message *msg) {
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

  // Reply to message FIRST (required by commodities.library)
  ReplyMsg((struct Message *)cxMsg);

  // Process the message
  switch (cxMsgType) {
  case CXM_IEVENT:
    // Input event - check if it's our inter-instance message (ID=1)
    if (cxMsgID == 1L) {
      // This is an inter-instance message from CxSender
      // The actual message data is in CxMsgData
      ttxMsg = (struct TTXMessage *)CxMsgData((const CxMsg *)cxMsg);
      if (ttxMsg) {
        switch (ttxMsg->type) {
        case TTX_MSG_OPEN_FILE:
          if (ttxMsg->fileName) {
            if (TTX_CreateSession(app, ttxMsg->fileName)) {
              // Rebuild signal mask after creating new session
              TTX_RebuildSignalMask(app);
            }
          }
          break;

        case TTX_MSG_OPEN_NEW:
          if (TTX_CreateSession(app, NULL)) {
            // Rebuild signal mask after creating new session
            TTX_RebuildSignalMask(app);
          }
          break;

        case TTX_MSG_QUIT:
          app->running = FALSE;
          break;

        default:
          break;
        }

        // According to Exec message docs: ALL messages must be replied to with
        // ReplyMsg() For one-way messages (mn_ReplyPort=NULL), ReplyMsg() does
        // nothing But we still call it, then free the message since no reply
        // will come back
        ReplyMsg(msg);
        // Messages from other instances were allocated by THEIR cleanup stack,
        // not ours So we must free them directly with FreeVec, not through our
        // cleanup stack
        if (ttxMsg->fileName) {
          FreeVec(ttxMsg->fileName);
        }
        FreeVec(ttxMsg);
      }
    }
    result = TRUE;
    break;

  case CXM_COMMAND:
    // Command from Exchange
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
      // Show/uniconify application
      TTX_Iconify(app, FALSE);
      // Bring all windows to front
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
      // Hide/iconify application
      TTX_Iconify(app, TRUE);
      result = TRUE;
      break;
    case CXCMD_KILL:
      app->running = FALSE;
      result = TRUE;
      break;
    case CXCMD_UNIQUE:
      // Another instance tried to start - show ourselves
      TTX_Iconify(app, FALSE);
      // Bring all windows to front
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
*/

BOOL TTX_HandleIntuitionMessage(struct TTXApplication *app,
                                    struct Session *portSession,
                                    struct IntuiMessage *imsg)
{
  return TTX_IntuiHandleMessage(app, portSession, imsg);
}

/* Rebuild signal mask after session list changes */
VOID TTX_RebuildSignalMask(struct TTXApplication *app) {
  ULONG oldMask;

  if (!app)
    return;

  oldMask = app->sigmask;
  TTX_IntuiRebuildSignalMask(app);
  /* COMMENTED OUT: Commodities disabled
  if (app->brokerPort) {
      app->sigmask |= (1UL << app->brokerPort->mp_SigBit);
  }
  */
  if (app->appIconPort) {
    app->sigmask |= (1UL << app->appIconPort->mp_SigBit);
  }
  if (app->arexxPort) {
    app->sigmask |= (1UL << app->arexxPort->mp_SigBit);
  }
  app->sigmask |= SIGBREAKF_CTRL_C;

  Printf("[EVENT] TTX_RebuildSignalMask: old mask=0x%08lx, new mask=0x%08lx\n",
         oldMask, app->sigmask);
}

/* Main event loop */
VOID TTX_EventLoop(struct TTXApplication *app) {
  struct Message *msg = NULL;
  struct IntuiMessage *imsg = NULL;
  ULONG signals = 0;
  struct Session *session = NULL;
  struct TTXMessage *ttxMsg = NULL;

  if (!app) {
    return;
  }

  /* Build initial signal mask (per-window UserPorts + appPort) */
  TTX_RebuildSignalMask(app);

  app->running = TRUE;

  while (app->running) {
    /* Process deferred iconification first */
    if (app->iconifyDeferred) {
      app->iconifyDeferred = FALSE;
      TTX_DoIconify(app, app->iconifyState);
      /* Rebuild signal mask after iconification (windows may have changed) */
      TTX_RebuildSignalMask(app);
    }

    if (!app->running)
      break;

    if (app->sessionCount == 0 && !app->backgroundMode) {
      app->running = FALSE;
      break;
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

    /* COMMENTED OUT: Commodities disabled
    // Check broker port (commodity messages from Exchange)
    if (app->brokerPort && (signals & (1UL << app->brokerPort->mp_SigBit))) {
        while ((msg = GetMsg(app->brokerPort)) != NULL) {
            TTX_HandleCommodityMessage(app, msg);
        }
    }
    */

    /* Check app icon port (app icon messages) */
    if (app->appIconPort && (signals & (1UL << app->appIconPort->mp_SigBit))) {
      TTX_ProcessAppIcon(app);
    }

    /* ARexx host: TurboText command strings from ADDRESS TURBOTEXT */
    if (app->arexxPort &&
        (signals & (1UL << app->arexxPort->mp_SigBit))) {
      TTX_ArexxProcess(app);
    }

    /* Check application port (inter-instance messages) */
    if (signals & (1UL << app->appPort->mp_SigBit)) {
      while ((msg = GetMsg(app->appPort)) != NULL) {
        ttxMsg = (struct TTXMessage *)msg;
        switch (ttxMsg->type) {
        case TTX_MSG_OPEN_FILE:
          if (ttxMsg->fileName) {
            if (TTX_CreateSession(app, ttxMsg->fileName)) {
              /* Rebuild signal mask after creating new session */
              TTX_RebuildSignalMask(app);
            }
          }
          break;
        case TTX_MSG_OPEN_NEW:
          if (TTX_CreateSession(app, NULL)) {
            /* Rebuild signal mask after creating new session */
            TTX_RebuildSignalMask(app);
          }
          break;
        case TTX_MSG_QUIT:
          app->running = FALSE;
          break;
        default:
          break;
        }
        /* According to Exec message docs: ALL messages must be replied to with
         * ReplyMsg() */
        /* For one-way messages (mn_ReplyPort=NULL), ReplyMsg() does nothing */
        /* But we still call it, then free the message since no reply will come
         * back */
        ReplyMsg(msg);
        /* Inter-instance messages: allocated by sender (other process), so we
         * must free with exec.library FreeVec(), not Seiso TTX_Free(). */
        if (ttxMsg->fileName) {
          FreeVec(ttxMsg->fileName);
        }
        FreeVec(ttxMsg);
      }
    }

    /* Each window has its own Intuition UserPort (WA_IDCMP at OpenWindow) */
    session = app->sessions;
    while (session) {
      struct Session *nextSession = session->next;
      struct MsgPort *userPort = NULL;

      if (session->window && session->window != INVALID_RESOURCE)
        userPort = session->window->UserPort;

      if (userPort && (signals & (1UL << userPort->mp_SigBit))) {
        Printf("[EVENT] Window signal received: session=%lu sigbit=%lu\n",
               session->sessionID, userPort->mp_SigBit);
        while ((imsg = (struct IntuiMessage *)GetMsg(userPort)) != NULL) {
          struct IntuiMessage savedMsg;
          ULONG classId;

          /*
           * Copy scalars first. For VANILLAKEY/RAWKEY handle while the
           * IntuiMessage (and dead-key IAddress) is still valid, then Reply.
           * For everything else Reply first so nested OpenWindow/ASL/menu
           * work cannot leave an unreplied message (IBase corruption).
           */
          savedMsg = *imsg;
          classId = savedMsg.Class;

          Printf("[EVENT] Got IntuiMessage: Class=0x%08lx, Code=0x%04lx, "
                 "Qualifier=0x%04lx\n",
                 classId, (ULONG)savedMsg.Code, (ULONG)savedMsg.Qualifier);

          if (classId == IDCMP_VANILLAKEY || classId == IDCMP_RAWKEY ||
              classId == 0x00200000UL || classId == 0x00000400UL) {
            app->intuiHandlerDepth++;
            TTX_HandleIntuitionMessage(app, session, &savedMsg);
            app->intuiHandlerDepth--;
            ReplyMsg((struct Message *)imsg);
          } else {
            ReplyMsg((struct Message *)imsg);
            app->intuiHandlerDepth++;
            TTX_HandleIntuitionMessage(app, session, &savedMsg);
            app->intuiHandlerDepth--;
          }
        }
      }
      session = nextSession;
    }

    /*
     * Run deferred OpenDoc/CloseDoc only after every port in this Wait cycle
     * has been fully drained and all IntuiMessages replied to.
     */
    TTX_ProcessDeferredActions(app);

    /* Exit if no sessions left (unless in background mode) */
    if (app->sessionCount == 0 && !app->backgroundMode) {
      app->running = FALSE;
    }
  }
}

/* Initialize application */
BOOL TTX_Init(struct TTXApplication *app) {
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

  /* Initialize libraries */
  if (!TTX_InitLibraries()) {
    return FALSE;
  }

  /* Open turbotext.library engine */
  if (!TTX_OpenTurboText(app)) {
    Printf("[INIT] TTX_Init: FAIL (turbotext.library)\n");
    return FALSE;
  }

  if (!TTX_TextEditor_InitClass()) {
    Printf("[INIT] TTX_Init: FAIL (ttxtexteditorclass)\n");
    TTX_CloseTurboText(app);
    return FALSE;
  }

  app->lastAslDrawer = TTX_AllocPathBuf();
  if (!app->lastAslDrawer) {
    Printf("[INIT] TTX_Init: FAIL (lastAslDrawer)\n");
    TTX_TextEditor_FreeClass();
    TTX_CloseTurboText(app);
    return FALSE;
  }

  /* Setup message port */
  if (!TTX_SetupMessagePort(app)) {
    TTX_Free(app->lastAslDrawer);
    app->lastAslDrawer = NULL;
    TTX_TextEditor_FreeClass();
    TTX_CloseTurboText(app);
    return FALSE;
  }

  /* Optional ARexx host (ADDRESS TURBOTEXT) for exercise scripts */
  TTX_ArexxInit(app);

  /* Load default prefs if present (Open Prefs / Save As Defaults path). */
  {
    struct TTXPrefs *prefs;

    prefs = TTX_PrefsGet();
    (void)TTX_PrefsLoad(prefs, (STRPTR)"PROGDIR:TTX.prefs");
  }

  /* COMMENTED OUT: Commodities disabled
  // Setup commodity if available
  if (CxBase) {
      if (!TTX_SetupCommodity(app)) {
          // Commodity setup failed, but continue anyway
          // The app can still work without commodities
      }
  }
  */

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
VOID TTX_Cleanup(struct TTXApplication *app) {
  struct Message *msg = NULL;

  Printf("[CLEANUP] TTX_Cleanup: START\n");
  if (!app) {
    Printf("[CLEANUP] TTX_Cleanup: DONE (app=NULL)\n");
    return;
  }

  /* Remove app icon before destroying sessions */
  TTX_RemoveAppIcon(app);

  /* Tear down ARexx host before sessions / ports */
  TTX_ArexxShutdown(app);
  TTX_ClipboardShutdown();

  if (app->lastAslDrawer) {
    TTX_Free(app->lastAslDrawer);
    app->lastAslDrawer = NULL;
  }

  /* Destroy all sessions */
  Printf("[CLEANUP] TTX_Cleanup: destroying %lu sessions\n", app->sessionCount);
  while (app->sessions) {
    TTX_DestroySession(app, app->sessions);
  }

  /* Clean up any pending messages from app port before cleanup */
  /* According to Exec message docs: ALL messages received via GetMsg() must be
   * replied to with ReplyMsg() */
  /* The sender is responsible for freeing the message after receiving the reply
   */
  if (app->appPort) {
    Printf("[CLEANUP] TTX_Cleanup: cleaning pending messages from appPort\n");
    while ((msg = GetMsg(app->appPort)) != NULL)
      ReplyMsg(msg);
    TTX_RemoveMessagePort(app);
    DeleteMsgPort(app->appPort);
    app->appPort = NULL;
    Printf("[CLEANUP] TTX_Cleanup: appPort cleaned up\n");
  }

  /* COMMENTED OUT: Commodities disabled
  // Clean up any pending messages from broker port BEFORE broker deletion
  // Messages must be processed while broker is still valid
  // According to commodities.library docs, messages sent to a commodity program
  // from a sender object must be sent back using ReplyMsg()
  if (app->brokerPort) {
      Printf("[CLEANUP] TTX_Cleanup: cleaning pending messages from
  brokerPort\n");
      // Get and reply to all pending messages
      while ((msg = GetMsg(app->brokerPort)) != NULL) {
          // Reply to commodity messages (required by commodities.library)
          ReplyMsg(msg);
      }
      Printf("[CLEANUP] TTX_Cleanup: brokerPort messages cleaned\n");
  }

  // CRITICAL: Explicitly clean up broker BEFORE cleanup stack flush
  // This ensures the broker is ALWAYS removed from the Exchange list, even if
  // the cleanup stack doesn't run properly (e.g., crash during cleanup).
  // According to commodities.library docs and example code, DeleteCxObjAll()
  // should be called explicitly before closing the library.
  // The broker is also tracked on the cleanup stack, but we clean it up
  // explicitly here to ensure it's ALWAYS removed, preventing stale brokers
  // from persisting and causing crashes on subsequent runs.
  if (app->broker && app->broker != INVALID_RESOURCE) {
      Printf("[CLEANUP] TTX_Cleanup: explicitly cleaning up broker=%lx\n",
  (ULONG)app->broker);
      // Use deleteCxObjAll() which will call cleanupBroker() and untrack from
  stack deleteCxObjAll(app->broker); app->broker = NULL; Printf("[CLEANUP]
  TTX_Cleanup: broker explicitly cleaned up\n");
  }

  // Broker is also tracked by Seiso cleanup stack, but we've already cleaned it
  up above
  // The cleanup stack will see it's already invalid and skip it
  */

  /* Close all libraries explicitly using wrapper functions */
  /* Libraries must be closed in reverse order of opening to avoid dependency
   * issues */
  /* The wrapper functions will close the library and remove it from the cleanup
   * stack */
  if (AslBase) {
    Printf("[CLEANUP] TTX_Cleanup: closing asl.library\n");
    CloseLibrary(AslBase);
    AslBase = NULL;
  }
  if (GadToolsBase) {
    Printf("[CLEANUP] TTX_Cleanup: closing gadtools.library\n");
    CloseLibrary(GadToolsBase);
    GadToolsBase = NULL;
  }
  if (KeymapBase) {
    Printf("[CLEANUP] TTX_Cleanup: closing keymap.library\n");
    CloseLibrary(KeymapBase);
    KeymapBase = NULL;
  }
  if (WorkbenchBase) {
    Printf("[CLEANUP] TTX_Cleanup: closing workbench.library\n");
    CloseLibrary(WorkbenchBase);
    WorkbenchBase = NULL;
  }
  if (IconBase) {
    Printf("[CLEANUP] TTX_Cleanup: closing icon.library\n");
    CloseLibrary(IconBase);
    IconBase = NULL;
  }
  if (GfxBase) {
    Printf("[CLEANUP] TTX_Cleanup: closing graphics.library\n");
    CloseLibrary((struct Library *)GfxBase);
    GfxBase = NULL;
  }
  if (UtilityBase) {
    Printf("[CLEANUP] TTX_Cleanup: closing utility.library\n");
    CloseLibrary(UtilityBase);
    UtilityBase = NULL;
  }

  /* Close turbotext while Intuition is still open (hooks may call Intuition). */
  TTX_CloseTurboText(app);
  TTX_TextEditor_FreeClass();

  if (IntuitionBase) {
    Printf("[CLEANUP] TTX_Cleanup: closing intuition.library\n");
    CloseLibrary((struct Library *)IntuitionBase);
    IntuitionBase = NULL;
  }

  Printf("[CLEANUP] TTX_Cleanup: DONE\n");
}

/* Update scroll bar BOOPSI gadgets — implemented in ttx_boopsi.c */
VOID TTX_ShowUsage(VOID) {
  Printf("Usage: TTX {files} [STARTUP=<macro>] [WINDOW=<desc>] "
         "[PUBSCREEN=<name>]\n");
  Printf("            [SETTINGS=<file>] [DEFINITIONS=<file>] [NOWINDOW] [WAIT] "
         "[BACKGROUND] [UNLOAD]\n");
  Printf("\n");
  Printf("Options:\n");
  Printf(
      "  FILES          Files to open (multiple allowed, supports patterns)\n");
  Printf("  STARTUP        ARexx macro to run for each document\n");
  Printf("  WINDOW         Window description: left/top/width/height/iconified "
         "left/iconified top/ICONIFIED/CLOSED\n");
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
int main(int argc, char *argv[]) {
  struct TTXApplication app;
  struct WBStartup *wbMsg = NULL;
  struct TTXArgs ttxArgs;
  struct RDArgs *rda = NULL;
  BOOL parseResult = FALSE;
  LONG result = RETURN_OK;
  STRPTR fullPath = NULL;

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

      fullPath = TTX_AllocPathBuf();
      if (!fullPath) {
        parseResult = FALSE;
      } else {
      /* Build full path */
      /* Clear IoErr() before dos.library path operations to ensure clean state
       */
      SetIoErr(0);
      fullPath[0] = '\0';
      if (wbarg->wa_Lock) {
        /* NameFromLock can fail - check result and clear error on failure */
        if (!NameFromLock(wbarg->wa_Lock, fullPath, TTX_PATH_BUF_LEN)) {
          /* NameFromLock failed - clear error and use empty path */
          /* This prevents dos.library from being left in undefined state */
          SetIoErr(0);
          fullPath[0] = '\0';
        } else {
          /* NameFromLock succeeded - clear any error code that may have been
           * set */
          SetIoErr(0);
        }
      }
      /* AddPart can fail - check result and clear error on failure */
      if (fullPath[0] != '\0' || wbarg->wa_Lock == NULL) {
        /* Clear IoErr() before AddPart to ensure clean state */
        SetIoErr(0);
        if (!AddPart(fullPath, wbarg->wa_Name, TTX_PATH_BUF_LEN)) {
          /* AddPart failed - clear error to prevent dos.library corruption */
          SetIoErr(0);
          fullPath[0] = '\0';
        } else {
          /* AddPart succeeded - clear any error code that may have been set */
          SetIoErr(0);
        }
      }

      len = 0;
      while (fullPath[len] != '\0' && len < (TTX_PATH_BUF_LEN - 1)) {
        len++;
      }
      if (len > 0 ) {
        fileName = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
        if (fileName) {
          Printf("[INIT] main: allocated fileName=%lx\n", (ULONG)fileName);
          CopyMem(fullPath, fileName, len);
          fileName[len] = '\0';
          ttxArgs.files = &fileName;
          ttxArgs.files[1] = NULL;
          parseResult = TRUE;
        }
      }
      TTX_Free(fullPath);
      fullPath = NULL;
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
    if (rda ) {
      FreeArgs(rda);
      rda = NULL; /* Prevent double-free */
    }
    TTX_Cleanup(&app);
    return RETURN_OK;
  }

  /* If BACKGROUND is set, don't open any sessions immediately - stay in
   * background */
  if (parseResult && ttxArgs.background) {
    /* Background mode - don't open sessions, just stay loaded */
    app.backgroundMode = TRUE;
    /* Free parsed arguments */
    if (rda ) {
      FreeArgs(rda);
      rda = NULL; /* Prevent double-free */
    }
    /* Run event loop even with no sessions (background mode) */
    TTX_EventLoop(&app);
    TTX_Cleanup(&app);
    return result;
  }

  /* Not in background mode */
  app.backgroundMode = FALSE;

  /* Check if another instance is running - but only if we have files or
   * NOWINDOW not set */
  /* NOTE: This check happens BEFORE we add our port to the system, so we won't
   * find ourselves */
  if (parseResult) {
    if (ttxArgs.files && ttxArgs.files[0]) {
      /* We have files - check for existing instance */
      if (TTX_CheckExistingInstance(ttxArgs.files[0])) {
        /* Message sent to existing instance, exit */
        if (rda ) {
          FreeArgs(rda);
          rda = NULL; /* Prevent double-free */
        }
        TTX_Cleanup(&app);
        return RETURN_OK;
      }
    } else if (!ttxArgs.noWindow) {
      /* No files but NOWINDOW not set - check for existing instance */
      if (TTX_CheckExistingInstance(NULL)) {
        if (rda ) {
          FreeArgs(rda);
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

  /* No existing instance found - add our port to the system so others can find
   * us */
  if (!TTX_AddMessagePort(&app)) {
    Printf(
        "[INIT] main: WARN (TTX_AddMessagePort failed, continuing anyway)\n");
  }

  /* Open documents via turbotext.library (original TTX driver model) */
  if (parseResult && !ttxArgs.background) {
    if (ttxArgs.settings) {
      struct TTXPrefs loaded;

      if (TTX_PrefsLoad(&loaded, ttxArgs.settings))
        TTX_PrefsApply(&app, NULL, &loaded);
    }
    if (ttxArgs.definitions)
      TTX_SetDefinitionsPath(ttxArgs.definitions);

    if (TTX_RunWithArgs(&app, &ttxArgs) < 0) {
      LONG errorCode = IoErr();
      if (errorCode != 0) {
        PrintFault(errorCode, "TTX");
        SetIoErr(0);
      }
      result = RETURN_FAIL;
    }
  }

  /* Free parsed arguments */
  if (rda ) {
    FreeArgs(rda);
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
