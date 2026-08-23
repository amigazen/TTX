/*
 * Driver prefs apply (presentation refresh only).
 * Prefs data + requesters live in ttxreqs.library.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 *
 * Include after ttx_driver.h when calling TTX_PrefsApply (needs Session).
 */

#ifndef TTX_PREFS_H
#define TTX_PREFS_H

#include <libraries/ttxreqs.h>
#include <proto/ttxreqs.h>

/*
 * Alias the library tags so "struct TTXPrefs" == "struct TRPrefs".
 * A typedef alone would leave "struct TTXPrefs" as a different incomplete type.
 */
#define TTXPrefs TRPrefs
#define TTXFindOptions TRFindOptions

#define TTX_PrefsSetDefaults TR_PrefsSetDefaults
#define TTX_PrefsGet         TR_PrefsGet
#define TTX_PrefsSet         TR_PrefsSet
#define TTX_PrefsLoad        TR_PrefsLoad
#define TTX_PrefsSave        TR_PrefsSave
#define TTX_PrefsRequester   TR_PrefsRequester

/* Push prefs into ttxreqs singleton and redraw open sessions. */
VOID TTX_PrefsApply(struct TTXApplication *app, struct Session *session,
	struct TRPrefs *p);

#endif /* TTX_PREFS_H */
