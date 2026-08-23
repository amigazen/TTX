#ifndef TR_INTERNAL_H
#define TR_INTERNAL_H

#include "compiler.h"
#include "include/libraries/ttxreqs.h"

#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/text.h>

struct TRFontMetrics {
	struct TextFont *font;
	struct TextAttr attr;
	WORD fontY;
	WORD fontX;
	WORD rowH;
	WORD gap;
	WORD margin;
};

#ifndef GRAPHICS_GFXBASE_H
#include <graphics/gfxbase.h>
#endif

extern struct GfxBase *GfxBase;
extern struct Library *GadToolsBase;
extern struct Library *AslBase;

APTR TR_Alloc(ULONG size, ULONG flags);
VOID TR_Free(APTR ptr);

BOOL TR_GetFontMetrics(struct Window *parent, struct TRFontMetrics *m);
VOID TR_FreeFontMetrics(struct TRFontMetrics *m);
LONG TR_TextWidth(struct TRFontMetrics *m, STRPTR text);

#endif /* TR_INTERNAL_H */
