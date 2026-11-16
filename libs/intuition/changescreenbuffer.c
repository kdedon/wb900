/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.

    Desc: Intuition function ChangeScreenBuffer()
*/
#include <proto/graphics.h>
#include "intuition_intern.h"

/*****************************************************************************

    NAME */
#include <intuition/screens.h>
#include <proto/intuition.h>

        AROS_LH2(ULONG, ChangeScreenBuffer,

/*  SYNOPSIS */
        AROS_LHA(struct Screen *      , screen, A0),
        AROS_LHA(struct ScreenBuffer *, screenbuffer, A1),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 130, Intuition)

/*  FUNCTION
        Do double or multiple buffering on an intuition screen in an
        intuition-cooperative way. The ScreenBuffer's BitMap will be
        installed on the specified screen, if possible. After a signal from
        graphics.library, the previously installed BitMap will be available
        for re-use. Consult graphics.library/AllocDBufInfo() and
        graphics.library/ChangeVPBitMap() for further information.

    INPUTS
        screen - The screen this screenbuffer belongs to
        screenbuffer - The screenbuffer obtained by AllocScreenBuffer()

    RESULT
        Non-zero if fuction succeeded, or zero if operation could not be
        performed, e.g. if user selects menus or gadgets.

    NOTES
        There is no display start to move: a BitMap in RAM is copied to the
        display, and both DBufInfo messages are replied once it is there.
        You need not re-install the original ScreenBuffer before closing
        a screen. Just FreeScreenBuffer() all buffers used for that screen.

    EXAMPLE

    BUGS

    SEE ALSO
        AllocScreenBuffer(), FreeScreenBuffer(),
        graphics.library/ChangeVPBitMap()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    ULONG lock;

    if (!screen) return 0;
    if (!screenbuffer) return 0;

    lock = LockIBase(0);

    ChangeVPBitMap(&screen->ViewPort,screenbuffer->sb_BitMap,screenbuffer->sb_DBufInfo);

    screen->RastPort.BitMap = screenbuffer->sb_BitMap;
    UpdateScreenBitMap(screen, IntuitionBase);

    UnlockIBase(lock);

    return TRUE;

    AROS_LIBFUNC_EXIT
} /* ChangeScreenBuffer */

/* Old programs read the screen's bitmap from BitMap_OBSOLETE: keep a copy
   of the header there. Drawing goes to the real one. */
void UpdateScreenBitMap(struct Screen *screen, struct IntuitionBase *IntuitionBase)
{
    CopyMem(screen->RastPort.BitMap, &screen->BitMap_OBSOLETE, sizeof(struct BitMap));
}
