/*
    Copyright (C) 1995-2016, The AROS Development Team. All rights reserved.

    Desc: Diskfont initialization code.
*/

#include <exec/types.h>
#include <exec/resident.h>
#include <exec/alerts.h>

#include "diskfont_intern.h"

static char dfname[] = "diskfont.library";
static char dfid[] = "diskfont 50.3 (15.11.2026)";

extern APTR DiskfontFuncs[];

/****************************************************************************************/

static LONG CleanMem(struct MemHandlerData *mhdata, struct DiskfontBase *DiskfontBase,
                     APTR sysbase);

/****************************************************************************************/

static struct Library *dfinit(struct DiskfontBase *DiskfontBase, APTR seg, APTR sysbase)
{
    DiskfontBase->lib.lib_Node.ln_Name = dfname;
    DiskfontBase->lib.lib_IdString = (APTR)dfid;
    DiskfontBase->lib.lib_Version = 50;
    DiskfontBase->lib.lib_Revision = 3;
    DiskfontBase->seglist = seg;

    NewList(&DiskfontBase->diskfontlist);
    NewList((struct List *)&DiskfontBase->fontsdirentrylist);
    InitSemaphore(&DiskfontBase->fontssemaphore);

    DiskfontBase->memint.is_Node.ln_Name = dfname;
    DiskfontBase->memint.is_Data = (APTR) DiskfontBase;
    DiskfontBase->memint.is_Code = (VOID (*)()) CleanMem;
    DiskfontBase->memint.is_Node.ln_Pri = 1; /* Just above RamLib */

    return &DiskfontBase->lib;
}

/****************************************************************************************/

/* Opened by the first OpenLibrary(), in the caller's task */
static BOOL openall(struct DiskfontBase *DiskfontBase)
{
    DOSBase = OpenLibrary("dos.library", 36L);
    GfxBase = (APTR)OpenLibrary("graphics.library", 39L);
    UtilityBase = OpenLibrary("utility.library", 39L);

    /* graphics.library never expunges while opened */
    Forbid();
    if (GfxBase)
        DiskfontBase->gfxfontsem = FindSemaphore("graphics.library/TextFonts");
    Permit();

    if (DOSBase && GfxBase && UtilityBase && DiskfontBase->gfxfontsem)
    {
        AddMemHandler(&DiskfontBase->memint);
        return TRUE;
    }
    if (UtilityBase) CloseLibrary(UtilityBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (DOSBase) CloseLibrary(DOSBase);
    DOSBase = NULL;
    return FALSE;
}

struct Library *dfopen(struct DiskfontBase *DiskfontBase, ULONG ver)
{
    if (!DOSBase && !openall(DiskfontBase))
        return NULL;

    DiskfontBase->lib.lib_OpenCnt++;
    DiskfontBase->lib.lib_Flags &= ~LIBF_DELEXP;
    return &DiskfontBase->lib;
}

/****************************************************************************************/

/*
 * The fonts it loaded stay in graphics' list without the library's code,
 * so no handler frees them after an expunge.
 */
APTR dfexpunge(struct DiskfontBase *DiskfontBase)
{
    APTR seg;

    if (DiskfontBase->lib.lib_OpenCnt != 0)
    {
        DiskfontBase->lib.lib_Flags |= LIBF_DELEXP;
        return NULL;
    }

    if (DOSBase)
    {
        RemMemHandler(&DiskfontBase->memint);
        CleanUpFontsDirEntryList(DiskfontBase);
        CloseLibrary(UtilityBase);
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary(DOSBase);
    }

    seg = DiskfontBase->seglist;
    Remove(&DiskfontBase->lib.lib_Node);
    FreeMem((char *)DiskfontBase - DiskfontBase->lib.lib_NegSize,
        DiskfontBase->lib.lib_NegSize + DiskfontBase->lib.lib_PosSize);
    return seg;
}

APTR dfclose(struct DiskfontBase *DiskfontBase)
{
    if (--DiskfontBase->lib.lib_OpenCnt == 0 &&
        (DiskfontBase->lib.lib_Flags & LIBF_DELEXP))
        return dfexpunge(DiskfontBase);
    return NULL;
}

APTR dfnull(void)
{
    return NULL;
}

/****************************************************************************************/

/* Frees the unused fonts it loaded, when an allocation fails */
static LONG CleanMem(struct MemHandlerData *mhdata, struct DiskfontBase *DiskfontBase,
                     APTR sysbase)
{
    struct DiskFontHeader *dfh, *dfh2;
    BOOL freed = FALSE;

    /* Unlinking needs the font semaphore, but low-memory-handler context
     * (Forbid, possibly inside an allocator) must never block on it. */
    if (!AttemptSemaphore(DiskfontBase->gfxfontsem))
        return MEM_DID_NOTHING;

    ForeachNodeSafe(&DiskfontBase->diskfontlist, dfh, dfh2)
    {
        if (dfh->dfh_TF.tf_Accessors < 1)
        {
            /* Possible paranoia check */
            if (!(dfh->dfh_TF.tf_Flags & FPF_REMOVED))
            {
                /* Unlink from GfxBase->TextFonts */
                Remove(&dfh->dfh_TF.tf_Message.mn_Node);

                /* Unlink from DiskfontBase->diskfontlist */
                Remove(&dfh->dfh_DF);

                UnLoadSeg(dfh->dfh_Segment);
                freed = TRUE;
            }
        }
    }

    ReleaseSemaphore(DiskfontBase->gfxfontsem);

    return freed ? MEM_ALL_DONE : MEM_DID_NOTHING;
}

/****************************************************************************************/

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} dfinittab = {
    sizeof(struct DiskfontBase), (APTR)DiskfontFuncs, NULL, (APTR)dfinit
};

struct Resident dftag = {
    RTC_MATCHWORD, &dftag, (APTR)(&dftag + 1), RTF_AUTOINIT, 50,
    NT_LIBRARY, 0, dfname, dfid, (APTR)&dfinittab
};
