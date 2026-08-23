/*
 * Driver shim: requester LVOs are in ttxreqs.library.
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 */

#ifndef TTX_REQSUI_H
#define TTX_REQSUI_H

#include <libraries/ttxreqs.h>
#include <proto/ttxreqs.h>

#define TTXFindOptions TRFindOptions

#define TTX_RequestBool       TR_RequestBool
#define TTX_RequestChoice     TR_RequestChoice
#define TTX_RequestStr        TR_RequestStr
#define TTX_RequestNum        TR_RequestNum
#define TTX_RequestFile       TR_RequestFile
#define TTX_RequestFind       TR_RequestFind
#define TTX_RequestFindChange TR_RequestFindChange

#endif /* TTX_REQSUI_H */
