/*
    Copyright (C) 1995-2016, The AROS Development Team. All rights reserved.
*/

#ifndef DISKFONT_INTERN_H
#define DISKFONT_INTERN_H

#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/semaphores.h>
#include <exec/interrupts.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <graphics/gfxbase.h>
#include <graphics/text.h>
#include <libraries/diskfont.h>
#include <aros/libcall.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/utility.h>

/**************/
/* Constants  */
/**************/

#define FONTTYPE_SCALEDFONT  0
#define FONTTYPE_FLAGMASK    (FPF_ROMFONT | FPF_DISKFONT)

#define IS_SCALED_FONT(ta) (((ta)->tta_Flags & FONTTYPE_FLAGMASK) == FONTTYPE_SCALEDFONT)

#define FONTSDIR        "FONTS:"

struct FontDescrHeader
{
    UWORD		NumEntries;
    struct  TTextAttr	*TAttrArray;
};

/**************/
/* Prototypes */
/**************/

struct DiskfontBase; /* prereference */

/* memoryfontfunc.c */

APTR MF_IteratorInit(struct DiskfontBase *);
struct TTextAttr *MF_IteratorGetNext(APTR, struct DiskfontBase *);
VOID MF_IteratorFree(APTR, struct DiskfontBase *);

/* diskfontfunc.c */

VOID CleanUpFontsDirEntryList(struct DiskfontBase *);
APTR DF_IteratorInit(struct TTextAttr *, struct DiskfontBase *);
struct TTextAttr *DF_IteratorGetNext(APTR, struct DiskfontBase *);
VOID DF_IteratorRemember(APTR, struct DiskfontBase *);
struct TextFont *DF_IteratorRememberOpen(APTR, struct DiskfontBase *);
VOID DF_IteratorFree(APTR, struct DiskfontBase *);
VOID DF_AddFont(struct TextFont *, struct DiskfontBase *);

/* diskfont_io.c */

struct DiskFontHeader *FontHeader(BPTR);
struct TextFont *ReadDiskFont(struct TTextAttr *, CONST_STRPTR, struct DiskfontBase *);

/* af_fontdescr_io.c */

struct FontDescrHeader *ReadFontDescr(CONST_STRPTR, struct DiskfontBase *);
VOID FreeFontDescr(struct FontDescrHeader *, struct DiskfontBase *);

/* basicfuncs.c */

APTR AllocSegment(BPTR *, ULONG, ULONG, struct DiskfontBase *);

/* newscaleddiskfont.c */

struct DiskFontHeader *Diskfont_9(struct Library *, struct TextFont *, struct TextAttr *);

/* crt.c */

ULONG strlen(CONST_STRPTR s);
char *strcpy(char *d, CONST_STRPTR s);
char *strcat(char *d, CONST_STRPTR s);
int strcmp(CONST_STRPTR a, CONST_STRPTR b);
STRPTR strrchr(CONST_STRPTR s, int c);

/********************/
/* Some nice macros */
/********************/

#undef	UB
#define UB(p)   ((UBYTE                   *)p)

#define ForeachNodeSafe(list, node, next) \
    for (node = (APTR)((struct List *)(list))->lh_Head; \
         (next = (APTR)((struct Node *)(node))->ln_Succ) != NULL; \
         node = next)

/* Internal prototypes */

struct DiskfontBase
{
    struct Library	   lib;
    APTR		   seglist;

    /* Opened by the first OpenLibrary() */
    struct Library	   *dosbase;
    struct GfxBase	   *gfxbase;
    struct Library	   *utilbase;

    /* The fonts it loaded, by their DiskFontHeaders' dfh_DF */
    struct List		   diskfontlist;

    struct MinList         fontsdirentrylist;
    struct SignalSemaphore fontssemaphore;
    /* graphics.library's public TextFonts semaphore, found by name at init */
    struct SignalSemaphore *gfxfontsem;

    /* MemHandler interrupt for flushing library */
    struct Interrupt       memint;
};

#define DFB(dfb)        ((struct DiskfontBase *)dfb)

#define DOSBase		(DFB(DiskfontBase)->dosbase)
#define GfxBase		(DFB(DiskfontBase)->gfxbase)
#define UtilityBase	(DFB(DiskfontBase)->utilbase)

#endif /* diskfont_intern.h */
