/*
 * TTX - Definition File Parser
 *
 * Copyright (c) 2025 amigazen project
 * Licensed under BSD 2-Clause License
 *
 * Parses TurboText .dfn files to extract menu definitions, keyboard shortcuts,
 * and other configuration data.
 */

#include "ttx_driver.h"

#ifndef DEVICES_INPUTEVENT_H
#include <devices/inputevent.h>
#endif

/* Menu entry types */
#define DFN_ENTRY_MENU  1
#define DFN_ENTRY_ITEM  2
#define DFN_ENTRY_SUB   3
#define DFN_ENTRY_BAR   4
#define DFN_ENTRY_SBAR  5

/* Menu entry structure */
struct DFNMenuEntry {
    ULONG type;              /* DFN_ENTRY_MENU, DFN_ENTRY_ITEM, etc. */
    STRPTR name;             /* Menu/item name (allocated) */
    STRPTR shortcut;         /* Keyboard shortcut (allocated, may be NULL) */
    STRPTR command;          /* Command name (allocated) */
    STRPTR *args;            /* Command arguments array (allocated, may be NULL) */
    ULONG argCount;          /* Number of arguments */
    struct DFNMenuEntry *next; /* Next entry in list */
};

/* Menu structure */
struct DFNMenu {
    STRPTR name;             /* Menu name (allocated) */
    STRPTR helpNode;         /* AmigaGuide help node (allocated, may be NULL) */
    struct DFNMenuEntry *entries; /* List of menu entries */
    struct DFNMenu *next;    /* Next menu in list */
};

/* KEYBOARD: / HOT_KEYS: binding */
struct DFNKeyBinding {
    STRPTR keySeq;           /* Normalized e.g. CTRL-D, SHIFT-CURSOR_UP */
    STRPTR command;
    STRPTR *args;
    ULONG argCount;
    struct DFNKeyBinding *next;
};

/* Definition file structure */
struct DFNFile {
    struct DFNMenu *menus;   /* List of menus */
    struct DFNKeyBinding *keys; /* KEYBOARD + HOT_KEYS bindings */
};

/* Forward declarations */
static VOID FreeDFNMenuEntry(struct DFNMenuEntry *entry);
static VOID FreeDFNMenu(struct DFNMenu *menu);
static VOID FreeDFNKeyBinding(struct DFNKeyBinding *kb);
static STRPTR SkipWhitespace(STRPTR line);
static STRPTR ExtractQuotedString(STRPTR line, STRPTR *outStr);
static STRPTR ExtractToken(STRPTR line, STRPTR *outStr);
static BOOL ParseMenuLine(STRPTR line, struct DFNMenuEntry *entry);
static BOOL ParseDFNMenus(BPTR fileHandle, struct DFNFile *dfn);
static BOOL ParseDFNKeys(BPTR fileHandle, struct DFNFile *dfn);
static BOOL DFN_IsKeyword(STRPTR p, STRPTR word, ULONG wordLen);

/* Free a menu entry and its allocated strings */
static VOID FreeDFNMenuEntry(struct DFNMenuEntry *entry)
{
    ULONG i;
    
    if (!entry) {
        return;
    }
    
    if (entry->name) {
        TTX_Free(entry->name);
    }
    if (entry->shortcut) {
        TTX_Free(entry->shortcut);
    }
    if (entry->command) {
        TTX_Free(entry->command);
    }
    if (entry->args) {
        for (i = 0; i < entry->argCount; i++) {
            if (entry->args[i]) {
                TTX_Free(entry->args[i]);
            }
        }
        TTX_Free(entry->args);
    }
    TTX_Free(entry);
}

/* Free a menu and all its entries */
static VOID FreeDFNMenu(struct DFNMenu *menu)
{
    struct DFNMenuEntry *entry;
    struct DFNMenuEntry *nextEntry;
    
    if (!menu) {
        return;
    }
    
    if (menu->name) {
        TTX_Free(menu->name);
    }
    if (menu->helpNode) {
        TTX_Free(menu->helpNode);
    }
    
    entry = menu->entries;
    while (entry) {
        nextEntry = entry->next;
        FreeDFNMenuEntry(entry);
        entry = nextEntry;
    }
    
    TTX_Free(menu);
}

static VOID FreeDFNKeyBinding(struct DFNKeyBinding *kb)
{
    ULONG i;

    if (!kb)
        return;
    if (kb->keySeq)
        TTX_Free(kb->keySeq);
    if (kb->command)
        TTX_Free(kb->command);
    if (kb->args) {
        for (i = 0; i < kb->argCount; i++) {
            if (kb->args[i])
                TTX_Free(kb->args[i]);
        }
        TTX_Free(kb->args);
    }
    TTX_Free(kb);
}

/* Skip whitespace at the start of a line */
static STRPTR SkipWhitespace(STRPTR line)
{
    if (!line) {
        return NULL;
    }
    
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    
    return line;
}

/* Extract a quoted string from a line, returning pointer to after the string */
static STRPTR ExtractQuotedString(STRPTR line, STRPTR *outStr)
{
    STRPTR start;
    STRPTR end;
    ULONG len;
    STRPTR result;
    
    if (!line || !outStr) {
        return NULL;
    }
    
    *outStr = NULL;
    
    /* Find opening quote */
    while (*line && *line != '"') {
        line++;
    }
    if (*line != '"') {
        return line; /* No quoted string found */
    }
    
    start = line + 1;
    line = start;
    
    /* Find closing quote (handle escaped quotes?) */
    while (*line && *line != '"') {
        line++;
    }
    if (*line != '"') {
        return start; /* Unterminated string */
    }
    
    end = line;
    len = end - start;
    
    /* Allocate and copy string */
    result = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
    if (!result) {
        return NULL;
    }
    
    CopyMem(start, result, len);
    result[len] = '\0';
    
    *outStr = result;
    
    return end + 1;
}

/* Extract a token (non-quoted string) from a line */
static STRPTR ExtractToken(STRPTR line, STRPTR *outStr)
{
    STRPTR start;
    STRPTR end;
    ULONG len;
    STRPTR result;
    
    if (!line || !outStr) {
        return NULL;
    }
    
    *outStr = NULL;
    
    /* Skip whitespace */
    line = SkipWhitespace(line);
    if (!*line) {
        return line;
    }
    
    start = line;
    
    /* Find end of token (whitespace or end of line) */
    while (*line && *line != ' ' && *line != '\t' && *line != '\n' && *line != '\r') {
        line++;
    }
    
    end = line;
    len = end - start;
    
    if (len == 0) {
        return line;
    }
    
    /* Allocate and copy string */
    result = (STRPTR)TTX_Alloc(len + 1, MEMF_CLEAR);
    if (!result) {
        return NULL;
    }
    
    CopyMem(start, result, len);
    result[len] = '\0';
    
    *outStr = result;
    
    return end;
}

/* Case-insensitive ASCII keyword match at start of line */
static BOOL DFN_IsKeyword(STRPTR p, STRPTR word, ULONG wordLen)
{
    ULONG i;
    UBYTE a;
    UBYTE b;

    for (i = 0; i < wordLen; i++) {
        if (p[i] == '\0')
            return FALSE;
        a = (UBYTE)p[i];
        b = (UBYTE)word[i];
        if (a >= 'a' && a <= 'z')
            a = (UBYTE)(a - ('a' - 'A'));
        if (b >= 'a' && b <= 'z')
            b = (UBYTE)(b - ('a' - 'A'));
        if (a != b)
            return FALSE;
    }
    return TRUE;
}

/* Parse a single menu line (MENU, ITEM, SUB, BAR, SBAR) */
static BOOL ParseMenuLine(STRPTR line, struct DFNMenuEntry *entry)
{
    STRPTR p;
    ULONG argIdx;
    STRPTR *newArgs;
    
    if (!line || !entry) {
        return FALSE;
    }
    
    /* Initialize entry */
    entry->type = 0;
    entry->name = NULL;
    entry->shortcut = NULL;
    entry->command = NULL;
    entry->args = NULL;
    entry->argCount = 0;
    entry->next = NULL;
    
    /* Skip leading whitespace */
    p = SkipWhitespace(line);
    if (!*p) {
        return FALSE; /* Empty line */
    }
    
    /* Determine entry type */
    /* Use StrnCmp from locale.library for string comparison */
    /* SC_ASCII (0) provides case-insensitive ASCII comparison */
    if (DFN_IsKeyword(p, "MENU", 4) && (p[4] == ' ' || p[4] == '\t' || p[4] == '\0')) {
        entry->type = DFN_ENTRY_MENU;
        p += 4;
    } else if (DFN_IsKeyword(p, "ITEM", 4) && (p[4] == ' ' || p[4] == '\t' || p[4] == '\0')) {
        entry->type = DFN_ENTRY_ITEM;
        p += 4;
    } else if (DFN_IsKeyword(p, "SUB", 3) && (p[3] == ' ' || p[3] == '\t' || p[3] == '\0')) {
        entry->type = DFN_ENTRY_SUB;
        p += 3;
    } else if (DFN_IsKeyword(p, "BAR", 3) && (p[3] == ' ' || p[3] == '\t' || p[3] == '\0' || p[3] == '\n' || p[3] == '\r')) {
        entry->type = DFN_ENTRY_BAR;
        return TRUE; /* BAR has no additional fields */
    } else if (DFN_IsKeyword(p, "SBAR", 4) && (p[4] == ' ' || p[4] == '\t' || p[4] == '\0' || p[4] == '\n' || p[4] == '\r')) {
        entry->type = DFN_ENTRY_SBAR;
        return TRUE; /* SBAR has no additional fields */
    } else {
        return FALSE; /* Unknown line type */
    }
    
    /* For MENU, ITEM, SUB: extract name, shortcut, command, and args */
    if (entry->type == DFN_ENTRY_MENU || entry->type == DFN_ENTRY_ITEM || entry->type == DFN_ENTRY_SUB) {
        /* Extract name (quoted string) */
        p = SkipWhitespace(p);
        if (*p == '"') {
            p = ExtractQuotedString(p, &entry->name);
            if (!entry->name) {
                return FALSE;
            }
        } else {
            /* Name not quoted - extract as token */
            p = ExtractToken(p, &entry->name);
            if (!entry->name) {
                return FALSE;
            }
        }
        
        /* For MENU: next field is optional help node (quoted string) */
        if (entry->type == DFN_ENTRY_MENU) {
            p = SkipWhitespace(p);
            if (*p == '"') {
                p = ExtractQuotedString(p, &entry->shortcut);
                /* shortcut holds help node for MENU entries */
            }
            /* MENU entries don't have command or args */
            return TRUE;
        }
        
        /* For ITEM and SUB: extract shortcut, command, and args */
        /* Extract shortcut (quoted string, may be empty) */
        p = SkipWhitespace(p);
        if (*p == '"') {
            p = ExtractQuotedString(p, &entry->shortcut);
            /* shortcut may be NULL if empty string */
        } else if (*p && *p != '\n' && *p != '\r') {
            /* No quotes - extract as token */
            p = ExtractToken(p, &entry->shortcut);
        }
        
        /* Extract command (token) */
        p = SkipWhitespace(p);
        if (*p && *p != '\n' && *p != '\r') {
            p = ExtractToken(p, &entry->command);
            /* command may be NULL if not present */
        }
        
        /* Extract arguments (remaining tokens) */
        argIdx = 0;
        while (*p && *p != '\n' && *p != '\r') {
            p = SkipWhitespace(p);
            if (!*p || *p == '\n' || *p == '\r') {
                break;
            }
            
            /* Expand args array */
            newArgs = (STRPTR *)TTX_Alloc((entry->argCount + 1) * sizeof(STRPTR), MEMF_CLEAR);
            if (!newArgs) {
                return FALSE;
            }
            
            /* Copy existing args */
            if (entry->args) {
                CopyMem(entry->args, newArgs, entry->argCount * sizeof(STRPTR));
                TTX_Free(entry->args);
            }
            
            entry->args = newArgs;
            
            /* Extract next argument */
            p = ExtractToken(p, &entry->args[argIdx]);
            if (!entry->args[argIdx]) {
                break;
            }
            
            argIdx++;
            entry->argCount = argIdx;
        }
    }
    
    return TRUE;
}

/* Parse MENUS section from a .dfn file */
static BOOL ParseDFNMenus(BPTR fileHandle, struct DFNFile *dfn)
{
    STRPTR lineBuffer = NULL;
    STRPTR line;
    ULONG lineLen;
    BOOL inMenusSection = FALSE;
    struct DFNMenu *currentMenu = NULL;
    struct DFNMenu *menuTail = NULL;
    struct DFNMenuEntry *currentEntry = NULL;
    struct DFNMenuEntry *newEntry;
    STRPTR p;
    
    if (!fileHandle || !dfn) {
        return FALSE;
    }

    lineBuffer = TTX_AllocPathBuf();
    if (!lineBuffer)
        return FALSE;
    
    dfn->menus = NULL;
    
    /* Read file line by line */
    SetIoErr(0);
    while (FGets(fileHandle, lineBuffer, TTX_PATH_BUF_LEN - 1) != NULL) {
        line = lineBuffer;
        lineLen = 0;
        
        /* Calculate line length */
        while (line[lineLen] != '\0' && line[lineLen] != '\n' && line[lineLen] != '\r' && lineLen < (TTX_PATH_BUF_LEN - 1)) {
            lineLen++;
        }
        
        /* Remove trailing newline */
        if (lineLen > 0 && (line[lineLen - 1] == '\n' || line[lineLen - 1] == '\r')) {
            lineLen--;
            if (lineLen > 0 && line[lineLen - 1] == '\r') {
                lineLen--;
            }
        }
        line[lineLen] = '\0';
        
        /* Skip comments (C-style /* ... */) */
        /* TODO: Handle comments properly */
        
        /* Check for section markers */
        p = SkipWhitespace(line);
        /* Use StrnCmp from locale.library for string comparison */
        /* SC_ASCII (0) provides case-insensitive ASCII comparison */
        if (DFN_IsKeyword(p, "MENUS:", 6)) {
            inMenusSection = TRUE;
            continue;
        } else if (*p == '#' && inMenusSection) {
            break;
        } else if (DFN_IsKeyword(p, "KEYBOARD:", 9) ||
                   DFN_IsKeyword(p, "HOT_KEYS:", 9) ||
                   DFN_IsKeyword(p, "MOUSE_BUTTONS:", 14) ||
                   DFN_IsKeyword(p, "DICTIONARY:", 11) ||
                   DFN_IsKeyword(p, "TEMPLATES:", 10) ||
                   DFN_IsKeyword(p, "LINKS:", 6)) {
            /* Another section starts - end MENUS section */
            if (inMenusSection) {
                break;
            }
            continue;
        }
        
        if (!inMenusSection) {
            continue;
        }
        
        /* Parse menu line */
        newEntry = (struct DFNMenuEntry *)TTX_Alloc(sizeof(struct DFNMenuEntry), MEMF_CLEAR);
        if (!newEntry) {
            TTX_Free(lineBuffer);
            return FALSE;
        }
        
        if (!ParseMenuLine(line, newEntry)) {
            TTX_Free(newEntry);
            continue; /* Skip invalid lines */
        }
        
        /* Handle MENU entry - start new menu */
        if (newEntry->type == DFN_ENTRY_MENU) {
            currentMenu = (struct DFNMenu *)TTX_Alloc(sizeof(struct DFNMenu), MEMF_CLEAR);
            if (!currentMenu) {
                FreeDFNMenuEntry(newEntry);
                TTX_Free(lineBuffer);
                return FALSE;
            }
            
            currentMenu->name = newEntry->name;
            newEntry->name = NULL; /* Transfer ownership */
            
            /* Extract help node if present (stored in shortcut field for MENU entries) */
            if (newEntry->shortcut) {
                currentMenu->helpNode = newEntry->shortcut;
                newEntry->shortcut = NULL;
            } else {
                currentMenu->helpNode = NULL;
            }
            
            currentMenu->entries = NULL;
            currentMenu->next = NULL;
            /* Append so Project stays first (file order). */
            if (!dfn->menus)
                dfn->menus = currentMenu;
            else
                menuTail->next = currentMenu;
            menuTail = currentMenu;
            
            FreeDFNMenuEntry(newEntry);
            currentEntry = NULL;
            continue;
        }
        
        /* Handle ITEM, SUB, BAR, SBAR entries - add to current menu */
        if (currentMenu) {
            if (!currentMenu->entries) {
                currentMenu->entries = newEntry;
            } else {
                currentEntry->next = newEntry;
            }
            currentEntry = newEntry;
        } else {
            /* Entry without a menu - skip it */
            FreeDFNMenuEntry(newEntry);
        }
    }
    
    TTX_Free(lineBuffer);
    return TRUE;
}

/* Parse a .dfn file and return a DFNFile structure */
struct DFNFile *ParseDFNFile(STRPTR fileName)
{
    BPTR fileHandle;
    struct DFNFile *dfn;
    
    if (!fileName) {
        return NULL;
    }
    
    /* Open file */
    fileHandle = Open(fileName, MODE_OLDFILE);
    if (!fileHandle)
        return NULL;
    
    /* Allocate DFN structure */
    dfn = (struct DFNFile *)TTX_Alloc(sizeof(struct DFNFile), MEMF_CLEAR);
    if (!dfn) {
        Close(fileHandle);
        return NULL;
    }
    
    /* Parse MENUS section */
    if (!ParseDFNMenus(fileHandle, dfn)) {
        Printf("[DFN] ParseDFNFile: failed to parse MENUS section\n");
        FreeDFNFile(dfn);
        Close(fileHandle);
        return NULL;
    }

    /* Re-scan from start for KEYBOARD: / HOT_KEYS: (menus parser stops early). */
    if (Seek(fileHandle, 0, OFFSET_BEGINNING) >= 0) {
        if (!ParseDFNKeys(fileHandle, dfn)) {
            Printf("[DFN] ParseDFNFile: WARN key section parse failed\n");
        }
    }
    
    Close(fileHandle);
    
    Printf("[DFN] ParseDFNFile: successfully parsed '%s'\n", fileName);
    return dfn;
}

/* Free a DFNFile structure and all its data */
VOID FreeDFNFile(struct DFNFile *dfn)
{
    struct DFNMenu *menu;
    struct DFNMenu *nextMenu;
    struct DFNKeyBinding *kb;
    struct DFNKeyBinding *nextKb;
    
    if (!dfn) {
        return;
    }
    
    menu = dfn->menus;
    while (menu) {
        nextMenu = menu->next;
        FreeDFNMenu(menu);
        menu = nextMenu;
    }

    kb = dfn->keys;
    while (kb) {
        nextKb = kb->next;
        FreeDFNKeyBinding(kb);
        kb = nextKb;
    }
    
    TTX_Free(dfn);
}

/*
 * Parse KEYBOARD: and HOT_KEYS: sections.
 * Line format: <key-seq> <command> [args...]
 * Comments start with * after whitespace; blank lines skipped.
 */
static BOOL ParseDFNKeys(BPTR fileHandle, struct DFNFile *dfn)
{
    STRPTR lineBuffer = NULL;
    STRPTR line = NULL;
    STRPTR p = NULL;
    ULONG lineLen = 0;
    BOOL inKeys = FALSE;
    struct DFNKeyBinding *kb = NULL;
    struct DFNKeyBinding *tail = NULL;
    STRPTR *newArgs = NULL;
    ULONG argIdx = 0;

    if (!fileHandle || !dfn)
        return FALSE;

    lineBuffer = TTX_AllocPathBuf();
    if (!lineBuffer)
        return FALSE;

    SetIoErr(0);
    while (FGets(fileHandle, lineBuffer, TTX_PATH_BUF_LEN - 1) != NULL) {
        line = lineBuffer;
        lineLen = 0;
        while (line[lineLen] != '\0' && line[lineLen] != '\n' &&
               line[lineLen] != '\r' && lineLen < (TTX_PATH_BUF_LEN - 1))
            lineLen++;
        if (lineLen > 0 && (line[lineLen - 1] == '\n' || line[lineLen - 1] == '\r')) {
            lineLen--;
            if (lineLen > 0 && line[lineLen - 1] == '\r')
                lineLen--;
        }
        line[lineLen] = '\0';

        p = SkipWhitespace(line);
        if (*p == '\0')
            continue;
        if (*p == '*')
            continue;

        if (DFN_IsKeyword(p, "KEYBOARD:", 9) ||
            DFN_IsKeyword(p, "HOT_KEYS:", 9)) {
            inKeys = TRUE;
            continue;
        }
        if (DFN_IsKeyword(p, "MENUS:", 6) ||
            DFN_IsKeyword(p, "MOUSE_BUTTONS:", 14) ||
            DFN_IsKeyword(p, "DICTIONARY:", 11) ||
            DFN_IsKeyword(p, "TEMPLATES:", 10) ||
            DFN_IsKeyword(p, "LINKS:", 6)) {
            inKeys = FALSE;
            continue;
        }
        if (!inKeys)
            continue;

        kb = (struct DFNKeyBinding *)TTX_Alloc(sizeof(struct DFNKeyBinding), MEMF_CLEAR);
        if (!kb) {
            TTX_Free(lineBuffer);
            return FALSE;
        }

        p = ExtractToken(p, &kb->keySeq);
        if (!kb->keySeq) {
            FreeDFNKeyBinding(kb);
            continue;
        }
        p = SkipWhitespace(p);
        if (*p == '\0') {
            FreeDFNKeyBinding(kb);
            continue;
        }
        p = ExtractToken(p, &kb->command);
        if (!kb->command) {
            FreeDFNKeyBinding(kb);
            continue;
        }

        argIdx = 0;
        while (*p && *p != '\n' && *p != '\r') {
            p = SkipWhitespace(p);
            if (!*p || *p == '\n' || *p == '\r')
                break;
            newArgs = (STRPTR *)TTX_Alloc((kb->argCount + 1) * sizeof(STRPTR), MEMF_CLEAR);
            if (!newArgs) {
                FreeDFNKeyBinding(kb);
                TTX_Free(lineBuffer);
                return FALSE;
            }
            if (kb->args) {
                CopyMem(kb->args, newArgs, kb->argCount * sizeof(STRPTR));
                TTX_Free(kb->args);
            }
            kb->args = newArgs;
            if (*p == '"')
                p = ExtractQuotedString(p, &kb->args[argIdx]);
            else
                p = ExtractToken(p, &kb->args[argIdx]);
            if (!kb->args[argIdx])
                break;
            argIdx++;
            kb->argCount = argIdx;
        }

        if (!dfn->keys)
            dfn->keys = kb;
        else
            tail->next = kb;
        tail = kb;
    }

    TTX_Free(lineBuffer);
    return TRUE;
}

/* Uppercase ASCII in place for key-seq compare. */
static VOID DFN_UpperCopy(STRPTR dst, STRPTR src, ULONG maxLen)
{
    ULONG i = 0;
    UBYTE c;

    if (!dst || maxLen < 1)
        return;
    dst[0] = '\0';
    if (!src)
        return;
    while (src[i] != '\0' && i < maxLen - 1) {
        c = (UBYTE)src[i];
        if (c >= 'a' && c <= 'z')
            c = (UBYTE)(c - ('a' - 'A'));
        dst[i] = (TEXT)c;
        i++;
    }
    dst[i] = '\0';
}

/* Map Amiga raw key code to DFN key name (no qualifiers). */
static BOOL DFN_RawToKeyName(UBYTE rawCode, STRPTR outName, ULONG outLen)
{
    STRPTR name = NULL;

    if (!outName || outLen < 2)
        return FALSE;

    switch (rawCode) {
    case 0x4C: name = "CURSOR_UP"; break;
    case 0x4D: name = "CURSOR_DOWN"; break;
    case 0x4E: name = "CURSOR_RIGHT"; break;
    case 0x4F: name = "CURSOR_LEFT"; break;
    case 0x42: name = "TAB"; break;
    case 0x45: name = "ESC"; break;
    case 0x46: name = "DEL"; break;
    case 0x41: name = "BACKSPACE"; break;
    case 0x5F: name = "HELP"; break;
    case 0x44: name = "RETURN"; break;
    case 0x43: name = "ENTER"; break;
    case 0x40: name = "SPACEBAR"; break;
    case 0x50: name = "F1"; break;
    case 0x51: name = "F2"; break;
    case 0x52: name = "F3"; break;
    case 0x53: name = "F4"; break;
    case 0x54: name = "F5"; break;
    case 0x55: name = "F6"; break;
    case 0x56: name = "F7"; break;
    case 0x57: name = "F8"; break;
    case 0x58: name = "F9"; break;
    case 0x59: name = "F10"; break;
    case 0x4B: name = "F11"; break;
    case 0x6F: name = "F12"; break;
    case 0x20: name = "A"; break;
    case 0x35: name = "B"; break;
    case 0x33: name = "C"; break;
    case 0x22: name = "D"; break;
    case 0x12: name = "E"; break;
    case 0x23: name = "F"; break;
    case 0x24: name = "G"; break;
    case 0x25: name = "H"; break;
    case 0x17: name = "I"; break;
    case 0x26: name = "J"; break;
    case 0x27: name = "K"; break;
    case 0x28: name = "L"; break;
    case 0x37: name = "M"; break;
    case 0x36: name = "N"; break;
    case 0x18: name = "O"; break;
    case 0x19: name = "P"; break;
    case 0x10: name = "Q"; break;
    case 0x13: name = "R"; break;
    case 0x21: name = "S"; break;
    case 0x14: name = "T"; break;
    case 0x16: name = "U"; break;
    case 0x34: name = "V"; break;
    case 0x11: name = "W"; break;
    case 0x32: name = "X"; break;
    case 0x15: name = "Y"; break;
    case 0x31: name = "Z"; break;
    default:
        return FALSE;
    }

    DFN_UpperCopy(outName, name, outLen);
    return TRUE;
}

/*
 * Build normalized key sequence: [CTRL-][ALT-][AMIGA-][SHIFT-]<KEY>
 * matching BuiltIn.dfn ordering.
 */
static BOOL DFN_BuildKeySeq(UBYTE rawCode, ULONG qualifier, STRPTR outSeq, ULONG outLen)
{
    TEXT keyName[32];
    ULONG i = 0;
    STRPTR s;

    if (!outSeq || outLen < 8)
        return FALSE;
    if (!DFN_RawToKeyName(rawCode, keyName, sizeof(keyName)))
        return FALSE;

    outSeq[0] = '\0';
    if (qualifier & IEQUALIFIER_CONTROL) {
        s = "CTRL-";
        while (*s && i < outLen - 1) outSeq[i++] = *s++;
    }
    if (qualifier & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) {
        s = "ALT-";
        while (*s && i < outLen - 1) outSeq[i++] = *s++;
    }
    if (qualifier & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)) {
        s = "AMIGA-";
        while (*s && i < outLen - 1) outSeq[i++] = *s++;
    }
    if (qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) {
        s = "SHIFT-";
        while (*s && i < outLen - 1) outSeq[i++] = *s++;
    }
    s = keyName;
    while (*s && i < outLen - 1) outSeq[i++] = *s++;
    outSeq[i] = '\0';
    return TRUE;
}

BOOL
TTX_DFNTryKeyCommand(
    struct TTXApplication *app,
    struct Session *session,
    UBYTE rawCode,
    ULONG qualifier)
{
    struct DFNFile *dfn = NULL;
    struct DFNKeyBinding *kb = NULL;
    TEXT seq[64];
    TEXT want[64];

    if (!app || !session)
        return FALSE;
    dfn = session->menuDFNBacking;
    if (!dfn || !dfn->keys)
        return FALSE;
    if (!DFN_BuildKeySeq(rawCode, qualifier, seq, sizeof(seq)))
        return FALSE;

    for (kb = dfn->keys; kb; kb = kb->next) {
        if (!kb->keySeq || !kb->command)
            continue;
        DFN_UpperCopy(want, kb->keySeq, sizeof(want));
        if (Stricmp(want, seq) != 0)
            continue;
        return TTX_HandleCommand(app, session, kb->command, kb->args, kb->argCount);
    }
    return FALSE;
}

/*
 * Resolve a menu pick when nm_UserData holds a DFNMenuEntry* (DFN menus).
 * Returns FALSE if userData is not an entry belonging to this dfn.
 */
BOOL
TTX_DFNCommandFromUserData(
	struct DFNFile *dfn,
	APTR userData,
	STRPTR *outCommand,
	STRPTR **outArgs,
	ULONG *outArgCount)
{
	struct DFNMenu *menu = NULL;
	struct DFNMenuEntry *entry = NULL;
	struct DFNMenuEntry *want = NULL;

	if (!dfn || !userData || !outCommand)
		return FALSE;

	*outCommand = NULL;
	if (outArgs)
		*outArgs = NULL;
	if (outArgCount)
		*outArgCount = 0;

	want = (struct DFNMenuEntry *)userData;
	for (menu = dfn->menus; menu; menu = menu->next) {
		for (entry = menu->entries; entry; entry = entry->next) {
			if (entry != want)
				continue;
			if (entry->type != DFN_ENTRY_ITEM && entry->type != DFN_ENTRY_SUB)
				return FALSE;
			if (!entry->command || entry->command[0] == '\0')
				return FALSE;
			*outCommand = entry->command;
			if (outArgs)
				*outArgs = entry->args;
			if (outArgCount)
				*outArgCount = entry->argCount;
			return TRUE;
		}
	}
	return FALSE;
}

/* Count total number of NewMenu entries needed for a DFN menu structure */
static ULONG CountNewMenuEntries(struct DFNFile *dfn)
{
    struct DFNMenu *menu;
    struct DFNMenuEntry *entry;
    ULONG count = 0;
    
    if (!dfn) {
        return 1; /* Just the NM_END marker */
    }
    
    menu = dfn->menus;
    while (menu) {
        count++; /* NM_TITLE for menu */
        
        entry = menu->entries;
        while (entry) {
            count++; /* Each entry (ITEM, SUB, BAR, SBAR) */
            entry = entry->next;
        }
        
        menu = menu->next;
    }
    
    count++; /* NM_END marker */
    
    return count;
}

/* Convert DFN menu structure to NewMenu array for CreateMenus */
/* Returns allocated NewMenu array, or NULL on failure */
/* Caller must free the returned array */
struct NewMenu *ConvertDFNToNewMenu(struct DFNFile *dfn, ULONG *outCount)
{
    struct DFNMenu *menu;
    struct DFNMenuEntry *entry;
    struct NewMenu *newMenu;
    ULONG count;
    ULONG idx = 0;
    ULONG menuNum = 0;
    ULONG itemNum = 0;
    ULONG subItemNum = 0;
    BOOL inSubMenu = FALSE;
    
    if (!dfn || !outCount) {
        return NULL;
    }
    
    count = CountNewMenuEntries(dfn);
    newMenu = (struct NewMenu *)TTX_Alloc(count * sizeof(struct NewMenu), MEMF_CLEAR);
    if (!newMenu) {
        return NULL;
    }
    
    menu = dfn->menus;
    while (menu) {
        /* Add menu title */
        newMenu[idx].nm_Type = NM_TITLE;
        newMenu[idx].nm_Label = menu->name;
        newMenu[idx].nm_CommKey = NULL;
        newMenu[idx].nm_Flags = 0;
        newMenu[idx].nm_MutualExclude = 0;
        newMenu[idx].nm_UserData = NULL;
        idx++;
        
        itemNum = 0;
        subItemNum = 0;
        inSubMenu = FALSE;
        
        entry = menu->entries;
        while (entry) {
            if (entry->type == DFN_ENTRY_ITEM) {
                /* Check if next entry is a SUB - if so, this ITEM becomes a submenu */
                struct DFNMenuEntry *nextEntry = entry->next;
                BOOL hasSubItems = FALSE;
                
                /* Check if this ITEM is followed by SUB entries */
                if (nextEntry && nextEntry->type == DFN_ENTRY_SUB) {
                    hasSubItems = TRUE;
                }
                
                if (hasSubItems) {
                    /* This ITEM will become a submenu - mark it as NM_SUB */
                    newMenu[idx].nm_Type = NM_SUB;
                    inSubMenu = TRUE;
                    subItemNum = 0;
                } else {
                    /* Regular menu item */
                    newMenu[idx].nm_Type = NM_ITEM;
                    inSubMenu = FALSE;
                    subItemNum = 0;
                }
                
                newMenu[idx].nm_Label = entry->name;
                newMenu[idx].nm_CommKey = entry->shortcut;
                newMenu[idx].nm_Flags = 0;
                newMenu[idx].nm_MutualExclude = 0;
                /* Pointer to DFN entry — pick handler reads command from here. */
                newMenu[idx].nm_UserData = (APTR)entry;
                idx++;
                itemNum++;
            } else if (entry->type == DFN_ENTRY_SUB) {
                /* Sub-menu item - should only appear after an ITEM that was marked as NM_SUB */
                if (!inSubMenu) {
                    /* This shouldn't happen, but handle gracefully */
                    Printf("[DFN] ConvertDFNToNewMenu: WARN - SUB item without parent ITEM\n");
                    /* Convert previous item to sub-menu if possible */
                    if (idx > 0 && newMenu[idx - 1].nm_Type == NM_ITEM) {
                        newMenu[idx - 1].nm_Type = NM_SUB;
                        inSubMenu = TRUE;
                        subItemNum = 0;
                    }
                }
                
                newMenu[idx].nm_Type = NM_ITEM;
                newMenu[idx].nm_Label = entry->name;
                newMenu[idx].nm_CommKey = entry->shortcut;
                newMenu[idx].nm_Flags = 0;
                newMenu[idx].nm_MutualExclude = 0;
                newMenu[idx].nm_UserData = (APTR)entry;
                idx++;
                subItemNum++;
            } else if (entry->type == DFN_ENTRY_BAR) {
                /* Menu separator bar */
                inSubMenu = FALSE;
                subItemNum = 0;
                newMenu[idx].nm_Type = NM_ITEM;
                newMenu[idx].nm_Label = NM_BARLABEL;
                newMenu[idx].nm_CommKey = NULL;
                newMenu[idx].nm_Flags = 0;
                newMenu[idx].nm_MutualExclude = 0;
                newMenu[idx].nm_UserData = NULL;
                idx++;
            } else if (entry->type == DFN_ENTRY_SBAR) {
                /* Sub-menu separator bar */
                newMenu[idx].nm_Type = NM_ITEM;
                newMenu[idx].nm_Label = NM_BARLABEL;
                newMenu[idx].nm_CommKey = NULL;
                newMenu[idx].nm_Flags = 0;
                newMenu[idx].nm_MutualExclude = 0;
                newMenu[idx].nm_UserData = NULL;
                idx++;
            }
            
            entry = entry->next;
        }
        
        menu = menu->next;
        menuNum++;
    }
    
    /* Add end marker */
    newMenu[idx].nm_Type = NM_END;
    newMenu[idx].nm_Label = NULL;
    newMenu[idx].nm_CommKey = NULL;
    newMenu[idx].nm_Flags = 0;
    newMenu[idx].nm_MutualExclude = 0;
    newMenu[idx].nm_UserData = NULL;
    idx++;
    
    *outCount = idx;
    
    return newMenu;
}
