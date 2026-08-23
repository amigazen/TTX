/*
 * Driver: apply prefs from ttxreqs.library and refresh presentation.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#include "ttx_prefs.h"
#include "ttx_driver.h"

VOID
TTX_PrefsApply(struct TTXApplication *app, struct Session *session,
	struct TRPrefs *p)
{
	struct Session *s;

	if (!p)
		return;
	if (session)
		session->mouseSelecting = FALSE;

	/* Library owns the live prefs singleton. */
	TR_PrefsSet(p);

	if (app) {
		for (s = app->sessions; s; s = s->next) {
			if (s->window)
				RenderText(s->window, s);
		}
	} else if (session && session->window) {
		RenderText(session->window, session);
	}
}
