/*
    Copyright (C) 1995-2004, The AROS Development Team. All rights reserved.
*/

/****************************************************************************************/

#include <exec/types.h>
#include "strings.h"

extern struct IntuitionBase     *IntuitionBase;
extern struct GfxBase           *GfxBase;
extern struct Library           *GadToolsBase;
extern struct Library           *UtilityBase;
extern struct Screen            *scr;
extern struct DrawInfo          *dri;
extern APTR                     vi;
extern struct Menu              *menus;
extern struct Window            *win;

extern ULONG                    gotomask, findmask;
extern UBYTE                    filenamebuffer[300];

/* The menu items' user data */
enum
{
    MEN_OPEN = 1,
    MEN_EDIT,
    MEN_ABOUT,
    MEN_QUIT,
    MEN_FIND,
    MEN_FIND_NEXT,
    MEN_FIND_PREV,
    MEN_JUMP
};

/****************************************************************************************/

/* more.c */

BOOL Fail(CONST_STRPTR msg);

/* misc.c */

BOOL MakeMenus(void);
void KillMenus(void);
STRPTR GetFile(void);
void About(void);

/* crt.c */

ULONG strlen(CONST_STRPTR s);
char *strncpy(char *d, CONST_STRPTR s, ULONG n);
char *strchr(CONST_STRPTR s, int c);
void sprintf(char *buf, CONST_STRPTR fmt, ...);

/****************************************************************************************/
