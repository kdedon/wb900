#ifndef ICON_INTERN_H
#define ICON_INTERN_H

/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/semaphores.h>
#include <aros/libcall.h>
#include <aros/asmcall.h>
#include <dos/dos.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <graphics/view.h>
#include <intuition/screens.h>

#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/icon.h>
#include <proto/utility.h>
#include <proto/dos.h>

/****************************************************************************************/

/* The C library functions icon.library uses (crt.c) */
#define offsetof(t, m)  ((ULONG)&((t *)0)->m)
ULONG strlen(CONST_STRPTR s);
char *strcpy(char *d, CONST_STRPTR s);
char *strcat(char *d, CONST_STRPTR s);
int strncmp(CONST_STRPTR a, CONST_STRPTR b, ULONG n);
APTR memset(APTR d, int c, ULONG n);

/* Constants */

/* Number of entries in the mementrys in the freelists */
#define FREELIST_MEMLISTENTRIES 10

/* This must be a power of 2 */
#define ICONLIST_HASHSIZE 32

/****************************************************************************************/

/*
    To get right alignment we make our very own memlist structure.
    Look at the original struct MemList in <exec/memory.h> to see why.
*/

struct IconInternalMemList
{
    struct Node     	iiml_Node;
    UWORD   	    	iiml_NumEntries;
    struct MemEntry 	iiml_ME[FREELIST_MEMLISTENTRIES];
};

struct NativeIcon
{
    struct MinNode    ni_Node;
    struct DiskObject ni_DiskObject;
    struct FreeList   ni_FreeList;

    /* Parameters */
    BOOL              ni_IsDefault;
    BOOL              ni_Frameless;

    /* The 'laid out' icon */
    struct Screen    *ni_Screen;      /* Screen for the layout */
    ULONG             ni_Width;
    ULONG             ni_Height;

    /* Pens for drawing the border and frame */
    UWORD ni_Pens[NUMDRIPENS];        /* Copied from DrawInfo for the screen */

    struct NativeIconImage {
        /* Allocated by LayoutIconA(), and _not_ in the ni_FreeList.
         *
         * You must call LayoutIconA(icon, NULL, NULL) or
         * FreeDiskObject() to free this memory.
         */
        struct BitMap *BitMap;      /* 'friend' of the Screen */
    } ni_Image[2];
};

#define NATIVEICON(icon) ((struct NativeIcon *)((UBYTE *)(icon) - offsetof(struct NativeIcon, ni_DiskObject)))

struct IconBase
{
    struct Library          ib_Lib;

    struct SignalSemaphore  iconlistlock;
    struct MinList          iconlists[ICONLIST_HASHSIZE];

    /* Global settings -----------------------------------------------------*/
    struct Screen          *ib_Screen;
    LONG                    ib_Precision;
    struct Rectangle        ib_EmbossRectangle;
    BOOL                    ib_Frameless;
    struct Hook            *ib_IdentifyHook;
    LONG                    ib_MaxNameLength;
    BPTR                    ib_SegList;

    /* Required External libraries */
    APTR                    ib_DOSBase;
    APTR                    ib_GfxBase;
    APTR                    ib_IntuitionBase;
    APTR                    ib_UtilityBase;

    /* Optional external libraries */
    APTR                    ib_WorkbenchBase;
};

typedef struct IconBase IconBase_T;

/* A label's box: its text with this margin beside, above and below it */
#define LABEL_MX 2
#define LABEL_MY 1

/* FIXME: Remove these #define xxxBase hacks
   Do not use this in new code !
*/
#define DOSBase		((struct DosLibrary *)IconBase->ib_DOSBase)
#define GfxBase		((struct GfxBase *)IconBase->ib_GfxBase)
#define IntuitionBase	((struct IntuitionBase *)IconBase->ib_IntuitionBase)
#define UtilityBase	((struct Library *)IconBase->ib_UtilityBase)

/****************************************************************************************/
/* On-demand open of optional libraries. Just that is
 * non-NULL before you use it!
 */
APTR DemandOpenLibrary(struct Library **libp, CONST_STRPTR libname, ULONG version);

#define WorkbenchBase	((struct Library *)DemandOpenLibrary((struct Library **)&IconBase->ib_WorkbenchBase, "workbench.library", 0))

/****************************************************************************************/

#include "support.h"

#define LB(ib)          ((struct IconBase *) (ib))

#endif /* ICON_INTERN_H */
