/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <proto/exec.h>
#include <intuition/iprefs.h>
#include <intuition/pointerclass.h>

#include <proto/intuition.h>
#include <proto/graphics.h>

#include "intuition_intern.h"

/*****************************************************************************

    NAME */
        AROS_LH3(ULONG, SetIPrefs,

/*  SYNOPSIS */
        AROS_LHA(APTR , data, A0),
        AROS_LHA(ULONG, length, D0),
        AROS_LHA(ULONG, type, D1),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 96, Intuition)

/*  FUNCTION

    INPUTS
        data   - the preferences of the type
        length - their size
        type   - IPREFS_TYPE_*

    RESULT
        Depending on the operation.

    NOTES
        This function is currently considered private.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct Library *UtilityBase = GetPrivIBase(IntuitionBase)->UtilityBase;
    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    ULONG Result = TRUE;
    BOOL screenmode = type == IPREFS_TYPE_SCREENMODE_V37;
    ULONG lock;

    /* OpenWorkBench/CloseWorkBench take the public-screen list first.
     * Keep the bitmap alive while comparing the requested mode below. */
    if (screenmode)
        LockPubScreenList();
    lock = LockIBase(0);

    switch (type)
    {
        case IPREFS_TYPE_ICONTROL_V37:
            if (length > sizeof(struct IIControlPrefs))
                length = sizeof(struct IIControlPrefs);
            CopyMem(data, &GetPrivIBase(IntuitionBase)->IControlPrefs, length);
            break;

        case IPREFS_TYPE_SCREENMODE_V37:
        {
            BOOL reopen = FALSE, closed = (GetPrivIBase(IntuitionBase)->WorkBench) ? FALSE : TRUE;

            if (length > sizeof(struct IScreenModePrefs))
                length = sizeof(struct IScreenModePrefs);

            if (!GetPrivIBase(IntuitionBase)->ScreenModePrefs)
                SetDisplayDefaults(IntuitionBase);
            if (!GetPrivIBase(IntuitionBase)->ScreenModePrefs)
            {
                Result = FALSE;
                break;
            }

            if (memcmp(GetPrivIBase(IntuitionBase)->ScreenModePrefs, data, length) == 0)
                break;

            if (!closed)
            {
                BOOL try = TRUE;
                struct IScreenModePrefs *cur = GetPrivIBase(IntuitionBase)->ScreenModePrefs;
                struct IScreenModePrefs *requested = data;
                struct Screen *wb = GetPrivIBase(IntuitionBase)->WorkBench;

                /* The request may describe the bitmap already supplied by
                 * an RTG driver. Store that request unchanged, so applying
                 * a shallower request again remains a no-op too. */
                if (requested->smp_DisplayID == cur->smp_DisplayID &&
                    requested->smp_Width == cur->smp_Width &&
                    requested->smp_Height == cur->smp_Height &&
                    requested->smp_Control == cur->smp_Control &&
                    requested->smp_Depth == GetBitMapAttr(wb->RastPort.BitMap, BMA_DEPTH))
                {
                    CopyMem(requested, cur, sizeof(*cur));
                    break;
                }

                reopen = TRUE;

                UnlockIBase(lock);
                UnlockPubScreenList();

                while (try && !(closed = CloseWorkBench()))
                {
                    struct EasyStruct es =
                    {
                        sizeof(struct EasyStruct),
                        0,
                        "System Request",
                        "Intuition is attempting to reset the screen,\n"
                        "please close all windows except Workbench's ones.",
                        "Retry|Cancel"
                    };

                    try = EasyRequestArgs(NULL, &es, NULL, NULL) == 1;
                }
            }

            if (closed)
            {
                CopyMem(data, GetPrivIBase(IntuitionBase)->ScreenModePrefs, length);
                GetPrivIBase(IntuitionBase)->IPrefsScreenMode = TRUE;
            }

            if (reopen)
            {
                if (closed)
                {
                    if (!OpenWorkBench())
                    {
                        /* FIXME: handle the error condition if OpenWorkBench() fails */
                        /* What to do if OpenWorkBench() fails? Try until it succeeds?
                           Try for a finite amount of times? Don't try and do nothing
                           at all? */
                    }
                }
                else
                    Result = FALSE;
                return Result;
            }

            break;
        }

        case IPREFS_TYPE_POINTER_V39:
        {
            struct IPointerPrefs *fp = data;
            struct TagItem pointertags[4];
            Object *pointer;

            pointertags[0].ti_Tag = POINTERA_BitMap;
            pointertags[0].ti_Data = (IPTR)fp->BitMap;
            pointertags[1].ti_Tag = POINTERA_XOffset;
            pointertags[1].ti_Data = fp->XOffset;
            pointertags[2].ti_Tag = POINTERA_YOffset;
            pointertags[2].ti_Data = fp->YOffset;
            pointertags[3].ti_Tag = TAG_DONE;

            pointer = NewObjectA(
                          GetPrivIBase(IntuitionBase)->pointerclass,
                          NULL,
                          pointertags);

            Object **oldptr = fp->Which ?
                      &GetPrivIBase(IntuitionBase)->BusyPointer :
                      &GetPrivIBase(IntuitionBase)->DefaultPointer;

            if (pointer)
                InstallPointer(IntuitionBase, oldptr, pointer);
            /* return -1 so that WB3.x C:IPrefs is happy */
            Result = -1;
        }
        break;

        case IPREFS_TYPE_POINTER_V37:
        {
            struct Preferences *ActivePrefs = &GetPrivIBase(IntuitionBase)->ActivePreferences;
            struct IPointerPrefsV37 *fp = data;
            UWORD size = fp->YSize * 2;
            Object *pointer;

            if (size > POINTERSIZE)
                size = POINTERSIZE;
            SetMem(ActivePrefs->PointerMatrix, 0, POINTERSIZE * sizeof (UWORD));
            CopyMem(fp->data, ActivePrefs->PointerMatrix, size * sizeof (UWORD));
            ActivePrefs->XOffset = fp->XOffset;
            ActivePrefs->YOffset = fp->YOffset;

            pointer = MakePointerFromPrefs(IntuitionBase, ActivePrefs);
            if (pointer)
                 InstallPointer(IntuitionBase, &GetPrivIBase(IntuitionBase)->DefaultPointer, pointer);
            /* return -1 so that WB2.x C:IPrefs is happy */
            Result = -1;
        }
        break;

        case IPREFS_TYPE_PALETTE_V39:
        case IPREFS_TYPE_PALETTE_V37:
        {
            BOOL closed = (GetPrivIBase(IntuitionBase)->WorkBench) ? FALSE : TRUE;
            struct Preferences *ActivePrefs = &GetPrivIBase(IntuitionBase)->ActivePreferences;
            struct Color32 *p = GetPrivIBase(IntuitionBase)->Colors;
            struct ColorSpec *pp = data;
            BOOL update_pointer = FALSE;

            while (pp->ColorIndex != -1)
            {
                WORD idx;
                    
                idx = pp->ColorIndex;
                if (type == IPREFS_TYPE_PALETTE_V37) {
                    /* v37 cursor colors are 17 to 19 */
                    if (idx >= 17)
                        idx = idx - 17 + 8;
                    else if (idx >= 8)
                        idx = -1;
                }
                if (idx >= 0 && idx < COLORTABLEENTRIES)
                {
                    UWORD red, green, blue;
                    if (type == IPREFS_TYPE_PALETTE_V37) {
                        /* 4-bit color components */
                        red = (pp->Red << 4) | pp->Red;
                        green = (pp->Green << 4) | pp->Green;
                        blue = (pp->Blue << 4) | pp->Blue;
                        red = (red << 8) | red;
                        green = (green << 8) | green;
                        blue = (blue << 8) | blue;
                    } else {
                        /* 16-bit color components */
                        red = pp->Red;
                        green = pp->Green;
                        blue = pp->Blue;
                    }
                   
                    p[idx].red   = ((ULONG)red << 16) | red;
                    p[idx].green = ((ULONG)green << 16) | green;
                    p[idx].blue  = ((ULONG)blue << 16) | blue;

                    /* Update oldstyle preferences */
                    if (ActivePrefs)
                    {
                        UWORD *cols = NULL;
                        UWORD baseindex;
                        
                        if (idx < 4) {
                            baseindex = 0;
                            cols = &ActivePrefs->color0;
                        } else if (idx >= 8 && idx <= 10) {
                            baseindex = 8;
                            cols=&ActivePrefs->color17;
                            update_pointer = TRUE;
                        }
                        
                        if (cols)
                            cols[idx - baseindex] = ((red >> 4) & 0xf00) | ((green >> 8) & 0x0f0) | ((blue >> 12));
                    }

                    /* update the wokrbench displays palette ... */
                    if (!closed)
                    {
                        UBYTE wbdepth = GetPrivIBase(IntuitionBase)->WorkBench->RastPort.BitMap->Depth;

                        if (idx < 4)
                            SetRGB32(&GetPrivIBase(IntuitionBase)->WorkBench->ViewPort, idx, p[idx].red, p[idx].green, p[idx].blue);
                        else if ((idx < 8) && (wbdepth >= 3))
                        {
                            ULONG lastcol = ((wbdepth > 8) ? 256 : (1 << wbdepth)) - 4;

                            SetRGB32(&GetPrivIBase(IntuitionBase)->WorkBench->ViewPort, lastcol + idx - 4, p[idx].red, p[idx].green, p[idx].blue);
                        }
                    }
                }
                pp++;
            }
            
            if (update_pointer) {
                SetPointerColors(IntuitionBase);
            }
        }
        break;

        case IPREFS_TYPE_PENS_V39:
        {
            BOOL closed = (GetPrivIBase(IntuitionBase)->WorkBench) ? FALSE : TRUE;
            struct IOldPenPrefs *fp = data;
            UWORD *defpenptr, *scrppenptr, *scrpenptr = NULL;
            struct IntScreen      *wbsint;
            UBYTE wbdepth;
            int i;

            /* Type 0 are the pens of screens of up to 4 colours, the others of deeper ones */
            defpenptr = (fp->Type == 0) ? &GetPrivIBase(IntuitionBase)->DriPens4[0]
                                        : &GetPrivIBase(IntuitionBase)->DriPens8[0];
            if (!closed)
            {
                struct IntDrawInfo      *dri;

                wbsint = GetPrivScreen(GetPrivIBase(IntuitionBase)->WorkBench);
                wbdepth = GetPrivIBase(IntuitionBase)->WorkBench->RastPort.BitMap->Depth;

                /* Update the Workbench screen too if these are the pens of its depth */
                if ((fp->Type == 0 ? wbdepth == 2 : wbdepth >= 3) && (dri = (struct IntDrawInfo *)GetScreenDrawInfo(GetPrivIBase(IntuitionBase)->WorkBench)))
                {
                    scrpenptr = dri->dri_Pens;
                    scrppenptr = wbsint->Pens;
                }
            }
            for (i=0;i<NUMDRIPENS;i++)
            {
                if (fp->PenTable[i]==(UWORD)~0UL)
                {
                    /*
                     * end of the array
                     */
                    break;
                }
                else
                {

                    defpenptr[i] = fp->PenTable[i];

                    if (scrpenptr)
                    {
                        UWORD penval = fp->PenTable[i];

                        scrpenptr[i] = penval;
                        scrppenptr[i] = penval;
                    }
                }
            }
        }
        break;

        case IPREFS_TYPE_OVERSCAN_V37:
        break;

        case IPREFS_TYPE_FONT_V37:
        {
            struct IFontPrefs *fp = data;
            struct TextFont *font = OpenFont(&fp->fp_TextAttr);
            struct TextFont **fontptr;

            if (font)
            {
                if (fp->fp_ScrFont==0)
                {
                    /*
                     * We can't free graphics defaultfont..it`s shared
                     */
                    fontptr = &GfxBase->DefaultFont;
                }
                else
                {
                    fontptr = &GetPrivIBase(IntuitionBase)->ScreenFont;
                    CloseFont(*fontptr);
                }
                *fontptr = font;
            }
        }
        break;

        default:
            Result = FALSE;
            break;
    }

    UnlockIBase(lock);
    if (screenmode)
        UnlockPubScreenList();

    return(Result);

    AROS_LIBFUNC_EXIT
} /* SetIPrefs */
