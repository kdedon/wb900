/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <intuition/intuition.h>
#include <intuition/iprefs.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <proto/input.h>
#include <devices/inputevent.h>
#include <graphics/displayinfo.h>
#include <prefs/prefhdr.h>
#include <prefs/screenmode.h>

#include "intuition_intern.h"

#define SCREENMODE_ENV "ENV:Sys/screenmode.prefs"

/* The SCRM chunk of the saved FORM PREF */
static BOOL LoadScreenMode(struct ScreenModePrefs *smp, struct IntuitionBase *IntuitionBase)
{
    struct DosLibrary *DOSBase = GetPrivIBase(IntuitionBase)->DOSBase;
    ULONG buf[24];
    UBYTE *b = (UBYTE *)buf;
    LONG len, at, size;
    BPTR fh;

    if (!DOSBase || !(fh = Open(SCREENMODE_ENV, MODE_OLDFILE)))
        return FALSE;
    len = Read(fh, buf, sizeof(buf));
    Close(fh);
    if (len < 12 || buf[0] != ID_FORM || buf[2] != ID_PREF)
        return FALSE;
    for (at = 12; at + 8 <= len; at += 8 + ((size + 1) & ~1L))
    {
        size = *(LONG *)(b + at + 4);
        if (*(ULONG *)(b + at) == ID_SCRM && size >= sizeof(*smp) &&
            at + 8 + sizeof(*smp) <= len)
        {
            CopyMem(b + at + 8, smp, sizeof(*smp));
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * The Workbench's mode and size: the one SetIPrefs() gave, else the saved
 * mode, when the display has it, else the display's first mode, the
 * card's own.  A mode may be one the display has no monitor for, the
 * other setting of a jumper.  Shift held ignores both, so a bad one can
 * be undone.  *why is why the card's mode was taken, else NULL.
 */
static ULONG ChooseMode(struct IntuitionBase *IntuitionBase, char **why)
{
    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct Library *InputBase = GetPrivIBase(IntuitionBase)->InputBase;
    struct IScreenModePrefs *smp = GetPrivIBase(IntuitionBase)->ScreenModePrefs;
    struct ScreenModePrefs saved;
    struct DimensionInfo dim;
    ULONG id = NextDisplayInfo(INVALID_ID);

    *why = NULL;
    if (id == INVALID_ID)
        return INVALID_ID;
    if (PeekQualifier() & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
        *why = "Shift held";
    else if (GetPrivIBase(IntuitionBase)->IPrefsScreenMode)
    {
        if (FindDisplayInfo(smp->smp_DisplayID) &&
            !(ModeNotAvailable(smp->smp_DisplayID) & ~DI_AVAIL_NOMONITOR))
            return smp->smp_DisplayID;
        *why = "IPrefs mode not available";
    }
    else if (!LoadScreenMode(&saved, IntuitionBase))
        *why = "no saved mode";
    else if (!FindDisplayInfo(saved.smp_DisplayID) ||
             (ModeNotAvailable(saved.smp_DisplayID) & ~DI_AVAIL_NOMONITOR))
        *why = "saved mode not available";
    else
    {
        smp->smp_Width  = saved.smp_Width;
        smp->smp_Height = saved.smp_Height;
        smp->smp_Depth  = saved.smp_Depth;
        smp->smp_Control = saved.smp_Control;
        return saved.smp_DisplayID;
    }
    if (GetDisplayInfoData(NULL, (UBYTE *)&dim, sizeof(dim), DTAG_DIMS, id))
    {
        smp->smp_Width  = dim.Nominal.MaxX + 1;
        smp->smp_Height = dim.Nominal.MaxY + 1;
    }
    smp->smp_Control = 0;
    return id;
}

/*****************************************************************************

    NAME */
        AROS_LH0(IPTR, OpenWorkBench,

/*  SYNOPSIS */

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 35, Intuition)

/*  FUNCTION
        Attempt to open the Workbench screen.

    INPUTS
        None.

    RESULT
        Tries to (re)open WorkBench screen. If successful return value
        is a pointer to the screen structure, which shouldn't be used,
        because other programs may close the WorkBench and make the
        pointer invalid. If this function fails the return value is NULL.

    NOTES
        While IPrefs is applying the saved preferences, the Workbench
        screen is not opened until it has finished.

    EXAMPLE

    BUGS

    SEE ALSO
        CloseWorkBench()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct Screen *wbscreen;

    /* Intuition not up yet? */
    if (!GetPrivIBase(IntuitionBase)->DefaultPointer)
        return FALSE;

    /* Wait for IPrefs to finish applying the saved preferences before the
       screen is opened with the ROM defaults. IPrefs itself holds the
       semaphore, so its own reopening is not held up, and once the screen
       is open nobody waits. */
    if (GetPrivIBase(IntuitionBase)->WorkBench == NULL)
    {
        struct SignalSemaphore *sem;

        Forbid();
        sem = FindSemaphore(IPREFS_SEMAPHORE_NAME);
        if (sem != NULL)
            ObtainSemaphoreShared(sem);
        Permit();
        if (sem != NULL)
            ReleaseSemaphore(sem);
    }

    LockPubScreenList();

    wbscreen = GetPrivIBase(IntuitionBase)->WorkBench;

    if (wbscreen)
    {

        UnlockPubScreenList();


        return (IPTR)wbscreen;
    }
    else
    {
        /* Open the Workbench screen if we don't have one. */
        WORD  width, height, depth;
        ULONG modeid;
        char *why;
        struct TagItem screenTags[] =
        {
            { SA_Width,                0                  }, /* 0 */
            { SA_Height,               0                  }, /* 1 */
            { SA_Depth,                0              }, /* 2 */
            { SA_DisplayID,            0                  }, /* 3 */
            { SA_LikeWorkbench,        TRUE               }, /* 4 */
            { SA_Type,                 WBENCHSCREEN       }, /* 5 */
            { SA_Title,         (IPTR) "Workbench Screen" }, /* 6 */
            { SA_PubName,       (IPTR) "Workbench"        }, /* 7 */
            { SA_SharePens,            TRUE               }, /* 8 */
            { TAG_END,                 0                  }
        };

        if (!GetPrivIBase(IntuitionBase)->ScreenModePrefs)
            SetDisplayDefaults(IntuitionBase);

        modeid = ChooseMode(IntuitionBase, &why);
        width  = GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_Width;
        height = GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_Height;
        depth  = GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_Depth;

        /* wb900: without a display (the C emulator) the screen is in RAM */
        {
            struct DimensionInfo dim;

            #define BOUND(min, val, max) \
                (((val) == -1) ? -1 : ((min) > (val)) ? (min) : ((max) < (val)) ? (max) : (val))

            /* Now fix up our specified size to fit into mode's limits. */
            if (GetDisplayInfoData(NULL, (UBYTE *)&dim, sizeof(dim), DTAG_DIMS, modeid))
            {
                width  = BOUND(dim.MinRasterWidth,  width,  dim.MaxRasterWidth);
                height = BOUND(dim.MinRasterHeight, height, dim.MaxRasterHeight);
                /* A mode of the C900's displays has one depth */
                depth  = dim.MaxDepth;

                GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_Width  = width;
                GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_Height = height;
                GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_Depth  = depth;
            }

            /*
             * Cache the used display info.
             * OpenScreen() with SA_LikeWorkbench set to TRUE
             * uses the DisplayID field. We MUST have something valid here.
             */
            if (GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_DisplayID != modeid)
            {
                GetPrivIBase(IntuitionBase)->ScreenModePrefs->smp_DisplayID = modeid;
                GetPrivIBase(IntuitionBase)->ActivePreferences.wb_Width = width;
                GetPrivIBase(IntuitionBase)->ActivePreferences.wb_Height = height;
                GetPrivIBase(IntuitionBase)->ActivePreferences.wb_Depth = depth;
            }

            screenTags[0].ti_Data = width;
            screenTags[1].ti_Data = height;
            screenTags[2].ti_Data = depth;
            screenTags[3].ti_Data = modeid;


            wbscreen = OpenScreenTagList(NULL, screenTags);
            /* after the screen takes the display from the boot console,
               which would draw the line */
            if (why)
                xKPrintf("intuition: %s, Workbench in mode %lx\n", why, modeid);

            if (wbscreen)
                GetPrivIBase(IntuitionBase)->ActivePreferences.wb_Depth = GetBitMapAttr(wbscreen->RastPort.BitMap, BMA_DEPTH);
        }

        if( !wbscreen )
        {

            UnlockPubScreenList();
            return 0;
        }

        GetPrivIBase(IntuitionBase)->WorkBench = wbscreen;

        /* Make the screen public. */
        PubScreenStatus( wbscreen, 0 );
    }

    /* We have opened the Workbench Screen. Now  tell the Workbench process
       to open it's windows, if there is one. We still do have the pub screen
       list locked. But while sending the Message to the Workbench task we
       must unlock the semaphore, otherwise there can be deadlocks if the
       Workbench task itself does something which locks the pub screen list.

       But if we unlock the pub screen list, then some other task could try
       to close the Workbench screen in the meantime. The trick to solve
       this problem is to increase the psn_VisitorCount of the Workbench
       screen here, before unlocking the pub screen list. This way the
       Workbench screen cannot go away. */

    GetPrivScreen(wbscreen)->pubScrNode->psn_VisitorCount++;

    UnlockPubScreenList();

    /* Don't call this function while pub screen list is locked! */
    TellWBTaskToOpenWindows(IntuitionBase);

    /* Now fix the psn_VisitorCount we have increased by one, above. It's probably
       better to do this by hand, instead of calling UnlockPubScreen, because Un-
       lockPubScreen can send signal to psn_SigTask. */

    LockPubScreenList();
    GetPrivScreen(wbscreen)->pubScrNode->psn_VisitorCount--;

    UnlockPubScreenList();


    return (IPTR)wbscreen;

    AROS_LIBFUNC_EXIT

} /* OpenWorkBench */
