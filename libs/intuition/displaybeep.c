/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <proto/graphics.h>
#include "intuition_intern.h"

#define BEEPTICKS       3   /* in 1/10th sec units */

/*****************************************************************************

    NAME */
#include <proto/intuition.h>

        AROS_LH1(void, DisplayBeep,

/*  SYNOPSIS */
        AROS_LHA(struct Screen *, screen, A0),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 16, Intuition)

/*  FUNCTION
        The Amiga has no internal speaker, so it flashes the background
        color of the specified screen as a signal. If the argument is
        NULL all screens will be flashed.

    INPUTS
        screen - The Screen that will be flashed.
            If NULL all screens will flash.

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS
        Hardware with a speaker should make an audible beep, too.
        Maybe even leave out the flashing on those architectures.

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    BOOL AllScreens = FALSE;
    //ULONG lock;

    ObtainSemaphore(&GetPrivIBase(IntuitionBase)->WindowLock);
    //jDc: screenlist will NOT be modified while we hold WINDOWLOCK!
    //lock = LockIBase(0);

    /* Start Beep */
    if (screen == (struct Screen *) -1 || screen == NULL)
    {
        screen = IntuitionBase->FirstScreen;
        AllScreens = TRUE;
    }

    while (screen)
    {

        if (screen->Flags & BEEPING)
        {
            GetPrivScreen(screen)->BeepingCounter = BEEPTICKS;
        }
        else
        {
            screen->Flags |= BEEPING;

            RenderScreenBar(screen, FALSE, IntuitionBase);

            GetPrivIBase(IntuitionBase)->BeepingScreens++;
            GetPrivScreen(screen)->BeepingCounter = BEEPTICKS;
        }

        screen =  AllScreens ? screen->NextScreen : NULL;
    }

    //UnlockIBase(lock);

    ReleaseSemaphore(&GetPrivIBase(IntuitionBase)->WindowLock);

    AROS_LIBFUNC_EXIT
} /* DisplayBeep */
