/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

#include <exec/resident.h>
#include <exec/alerts.h>
#include <utility/utility.h>
#include "icon_intern.h"

#define AN_IconLib  0x09000000L

static char icname[] = ICONNAME;
static char icid[] = "icon 44.11 (15.11.2026)";

extern APTR IconFuncs[];

/****************************************************************************************/

/* Offsets of the library bases in the IconBase, in the order they are closed.
 * The last four are the required libraries, opened last to first. */
#define IBOFF(field)    offsetof(struct IconBase, field)

static const UWORD libbases[] =
{
    IBOFF(ib_WorkbenchBase),
    IBOFF(ib_IntuitionBase), IBOFF(ib_GfxBase), IBOFF(ib_DOSBase), IBOFF(ib_UtilityBase)
};
#define NUM_LIBBASES    5
#define FIRST_REQUIRED  1

static const char * const libnames[] =
{
    "intuition.library", "graphics.library", "dos.library", "utility.library"
};

#define LIBPTR(base, n) (*(struct Library **)((UBYTE *)(base) + libbases[n]))

static struct Library *icinit(struct Library *lib, APTR seg, APTR sysbase)
{
    struct IconBase *lh = LB(lib);
    LONG i;

    lib->lib_Node.ln_Name = icname;
    lib->lib_IdString = (APTR)icid;
    lib->lib_Version = 44;
    lib->lib_Revision = 11;
    lh->ib_SegList = seg;

    InitSemaphore(&lh->iconlistlock);
    for(i = 0; i < ICONLIST_HASHSIZE; i++)
    {
        NewList((struct List *)&lh->iconlists[i]);
    }

    /* Setup default global settings ---------------------------------------*/
    lh->ib_Screen               = NULL;
    lh->ib_Precision            = PRECISION_ICON;
    lh->ib_EmbossRectangle.MinX = -4;
    lh->ib_EmbossRectangle.MaxX = 4;
    lh->ib_EmbossRectangle.MinY = -4;
    lh->ib_EmbossRectangle.MaxY = 4;
    lh->ib_Frameless            = FALSE;
    lh->ib_IdentifyHook         = NULL;
    lh->ib_MaxNameLength        = 25;

    for (i = NUM_LIBBASES - 1; i >= FIRST_REQUIRED; i--)
    {
        if ((LIBPTR(lh, i) = OpenLibrary(libnames[i - FIRST_REQUIRED],
                (i == FIRST_REQUIRED + 1) ? 39L : 0L)) == NULL)
        {
            /* Close the ones opened so far, latest first */
            while (++i < NUM_LIBBASES)
                CloseLibrary(LIBPTR(lh, i));
            FreeMem((char *)lib - lib->lib_NegSize,
                lib->lib_NegSize + lib->lib_PosSize);
            return NULL;
        }
    }

    /* Optional libraries are loaded dynamically if needed */
    return lib;
}

/****************************************************************************************/

struct Library *icopen(struct Library *lib, ULONG ver)
{
    lib->lib_OpenCnt++;
    lib->lib_Flags &= ~LIBF_DELEXP;
    return lib;
}

APTR icexpunge(struct Library *lib)
{
    struct IconBase *IconBase = LB(lib);
    APTR seg;
    LONG i;

    if (lib->lib_OpenCnt != 0)
    {
        lib->lib_Flags |= LIBF_DELEXP;
        return NULL;
    }
    seg = IconBase->ib_SegList;
    Remove(&lib->lib_Node);

    /* Drop optional libraries, then the rest */
    for (i = 0; i < NUM_LIBBASES; i++)
    {
        if (LIBPTR(IconBase, i)) CloseLibrary(LIBPTR(IconBase, i));
    }
    FreeMem((char *)lib - lib->lib_NegSize,
        lib->lib_NegSize + lib->lib_PosSize);
    return seg;
}

APTR icclose(struct Library *lib)
{
    if (--lib->lib_OpenCnt == 0 && (lib->lib_Flags & LIBF_DELEXP))
        return icexpunge(lib);
    return NULL;
}

APTR icnull(void)
{
    return NULL;
}

/* Vector of every unimplemented function */
ULONG icunimpl(void)
{
    Alert(AN_IconLib);
    return 0;
}

static struct {
    ULONG it_Size;
    APTR it_Funcs, it_Struct, it_Init;
} icinittab = {
    sizeof(struct IconBase), (APTR)IconFuncs, NULL, (APTR)icinit
};

struct Resident ictag = {
    RTC_MATCHWORD, &ictag, (APTR)(&ictag + 1), RTF_AUTOINIT, 44,
    NT_LIBRARY, -122, icname, icid, (APTR)&icinittab
};
