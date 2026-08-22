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

/* Definition file structure */
struct DFNFile {
    struct DFNMenu *menus;   /* List of menus */
    /* TODO: Add keyboard, hotkeys, mouse buttons, etc. */
};

/* Forward declarations */
static VOID FreeDFNMenuEntry(struct DFNMenuEntry *entry);
static VOID FreeDFNMenu(struct DFNMenu *menu);
static STRPTR SkipWhitespace(STRPTR line);
static STRPTR ExtractQuotedString(STRPTR line, STRPTR *outStr);
static STRPTR ExtractToken(STRPTR line, STRPTR *outStr);
static BOOL ParseMenuLine(STRPTR line, struct DFNMenuEntry *entry);
static BOOL ParseDFNMenus(BPTR fileHandle, struct DFNFile *dfn);

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
            currentMenu->next = dfn->menus;
            dfn->menus = currentMenu;
            
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
    
    Close(fileHandle);
    
    Printf("[DFN] ParseDFNFile: successfully parsed '%s'\n", fileName);
    return dfn;
}

/* Free a DFNFile structure and all its data */
VOID FreeDFNFile(struct DFNFile *dfn)
{
    struct DFNMenu *menu;
    struct DFNMenu *nextMenu;
    
    if (!dfn) {
        return;
    }
    
    menu = dfn->menus;
    while (menu) {
        nextMenu = menu->next;
        FreeDFNMenu(menu);
        menu = nextMenu;
    }
    
    TTX_Free(dfn);
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
                newMenu[idx].nm_UserData = (APTR)((menuNum << 8) | itemNum);
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
                /* Sub-items use the same itemNum as their parent */
                newMenu[idx].nm_UserData = (APTR)((menuNum << 8) | (itemNum - 1));
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
