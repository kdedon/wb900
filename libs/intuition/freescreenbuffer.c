/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.

    Desc: Intuition function FreeScreenBuffer()
*/
#include <graphics/rastport.h>
#include <proto/graphics.h>
#include "intuition_intern.h"

/*****************************************************************************

    NAME */
#include <intuition/screens.h>
#include <proto/intuition.h>

        AROS_LH2(void, FreeScreenBuffer,

/*  SYNOPSIS */
        AROS_LHA(struct Screen *      , screen, A0),
        AROS_LHA(struct ScreenBuffer *, screenbuffer, A1),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 129, Intuition)

/*  FUNCTION
        Frees a ScreenBuffer allocated by AllocScreenBuffer() and releases
        associated resources. You have to call this before closing your
        screen.

    INPUTS
        screen - The screen this screenbuffer belongs to
        screenbuffer - The screenbuffer obtained by AllocScreenBuffer().
            It is safe to pass NULL.

    RESULT
        None.

    NOTES
        When used SB_SCREEN_BITMAP on allocating the ScreenBuffer
        (ie. the ScreenBuffer only refers to the screen's BitMap) you must
        FreeScreenBuffer() the ScreenBuffer before closing the screen.
        Intuition will recognize when FreeScreenBuffer() is called for the
        currently installed ScreenBuffer that it must not free the BitMap.
        This is left to the CloseScreen() function.

    EXAMPLE

    BUGS

    SEE ALSO
        AllocScreenBuffer(), ChangeScreenBuffer()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;

    if (screenbuffer)
    {
        /* Show the screen's own bitmap again; its DBufInfo replies to no one */
        if (((struct IntScreenBuffer *)screenbuffer)->free_bitmap &&
            (screen->RastPort.BitMap == screenbuffer->sb_BitMap))
        {
            struct ScreenBuffer scb;

            scb.sb_BitMap = IS(screen)->RestoreBitMap;
            scb.sb_DBufInfo = IS(screen)->RestoreDBufInfo;

            ChangeScreenBuffer(screen, &scb);
        }

        FreeDBufInfo(screenbuffer->sb_DBufInfo);

        if (((struct IntScreenBuffer *)screenbuffer)->free_bitmap)
        {
            FreeBitMap(screenbuffer->sb_BitMap);
        }

        FreeMem(screenbuffer, sizeof(struct IntScreenBuffer));
    }

    AROS_LIBFUNC_EXIT
} /* FreeScreenBuffer */

