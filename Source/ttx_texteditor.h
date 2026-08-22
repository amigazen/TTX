/*
 * TTX driver - multiline text editor BOOPSI class
 *
 * Registered at application init as a foundational gadget class
 * (textentry.gadget-style multiline editor for TTX).
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TTX_TEXTEDITOR_H
#define TTX_TEXTEDITOR_H

#include "ttx_driver.h"

/****************************************************************************/

#define TTXTEXTEDITORCLASS "ttxtexteditorclass"

#define TEA_Dummy		(TAG_USER + 0x54000)
#define TEA_Session		(TEA_Dummy + 1)
#define TEA_Application		(TEA_Dummy + 2)
#define TEA_ReadOnly		(TEA_Dummy + 3)

#define GID_TEXT_EDITOR		10

/****************************************************************************/

BOOL TTX_TextEditor_InitClass(VOID);
VOID TTX_TextEditor_FreeClass(VOID);

BOOL TTX_TextEditor_CreateGadget(
	struct TTXApplication *app,
	struct Session *session);

VOID TTX_TextEditor_DestroyGadget(struct Session *session);

VOID TTX_TextEditor_Activate(struct Session *session);

VOID TTX_TextEditor_Refresh(struct Session *session);

/****************************************************************************/

#endif /* TTX_TEXTEDITOR_H */
