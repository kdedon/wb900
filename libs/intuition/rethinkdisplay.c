/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <intuition/pointerclass.h>
#include <proto/graphics.h>

#include "intuition_intern.h"
#include "inputhandler.h"

/*****************************************************************************

    NAME */
#include <proto/intuition.h>

        AROS_LH0(LONG, RethinkDisplay,

/*  SYNOPSIS */

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 65, Intuition)

/*  FUNCTION
        Check and update, i.e. redisplay the whole Intuition display.

    INPUTS
        None.

    RESULT
        Zero for success, non-zero for failure.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        RemakeDisplay(), MakeScreen(), graphics.library/MakeVPort(),
        graphics.library/MrgCop(), graphics.library/LoadView()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct RethinkDisplayActionMsg msg;

    msg.lock = TRUE;
    DoSyncAction((APTR)int_RethinkDisplay, &msg.msg, IntuitionBase);

    return msg.failure;

    AROS_LIBFUNC_EXIT
} /* RethinkDisplay */

void int_RethinkDisplay(struct RethinkDisplayActionMsg *msg,struct IntuitionBase *IntuitionBase)
{
    struct GfxBase   *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct Screen    *screen;
    struct ViewPort  *viewport;
    struct ViewPort **viewportptr;
    UWORD             modes;
    LONG              failure = 0;
    ULONG             ilock = 0;

    if (msg->lock)
        ilock = LockIBase(0);

    screen = IntuitionBase->FirstScreen;

    if (screen == NULL)
    {

        IntuitionBase->ViewLord.ViewPort = NULL;
        LoadView(NULL);
    }
    else
    {
        /* Find visible screens */

        screen->ViewPort.Modes |= SPRITES;
        screen->ViewPort.Modes &= ~VP_HIDE;

        /* Build the list of viewports in the view */

        viewportptr = &IntuitionBase->ViewLord.ViewPort;
        for (screen = IntuitionBase->FirstScreen; screen; screen = screen->NextScreen)
        {
            if ((screen->ViewPort.Modes & VP_HIDE) == 0)
            {

                *viewportptr = &screen->ViewPort;
                viewportptr = &screen->ViewPort.Next;
            }
        }
        *viewportptr = NULL;

        /* Find view mode */

        modes = (IntuitionBase->ViewLord.Modes & ~LACE) | SPRITES;
        for (viewport = IntuitionBase->ViewLord.ViewPort; viewport; viewport = viewport->Next) {
            modes |= viewport->Modes & LACE;
        }

        /* Reinitialize the view */
        IntuitionBase->ViewLord.DxOffset = 0; /***/
        IntuitionBase->ViewLord.DyOffset = 0; /***/
        IntuitionBase->ViewLord.Modes = modes;
        screen = IntuitionBase->FirstScreen;
        /*
         * Rebuild the copper lists of every visible screen, as AmigaOS does.
         * Applications rely on this to get changes made behind Intuition's
         * back merged in: the documented way to attach a user copper list to
         * a screen is to set ViewPort.UCopIns and call RethinkDisplay().
         */

        for (viewport = IntuitionBase->ViewLord.ViewPort; viewport; viewport = viewport->Next)
        {
            {
                LONG error;

                viewport->Modes = (viewport->Modes & ~LACE) | (modes & LACE);
                error = MakeVPort(&IntuitionBase->ViewLord, viewport);

                if (error)
                    failure = error;
            }
        }

        if (!failure)
        {
            failure = MrgCop(&IntuitionBase->ViewLord);

            if (!failure)
            {
                ObtainSemaphore(&GetPrivIBase(IntuitionBase)->ViewLordLock);
                LoadView(&IntuitionBase->ViewLord);

                ReleaseSemaphore(&GetPrivIBase(IntuitionBase)->ViewLordLock);

            }
        }
    }

    if (!failure)
    {
        /* validate screen positions, scrolling limits are not necessarily same anymore. */
        for (screen = IntuitionBase->FirstScreen; screen; screen = screen->NextScreen)
        {
            if ((screen->ViewPort.Modes & VP_HIDE) == 0)
            {
                ScreenPosition( screen, SPOS_RELATIVE, 0,0,0,0  );
            }
        }
        /* The pointer is on the frontmost screen */
        ShowPointer(IntuitionBase);
    }

    if (msg->lock)
        UnlockIBase(ilock);

    msg->failure = failure;
}
