/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include "intuition_intern.h"

#include <intuition/intuition.h>
#include <proto/intuition.h>

/*****************************************************************************

    NAME */
        AROS_LH0(LONG, CloseWorkBench,

/*  SYNOPSIS */

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 13, Intuition)

/*  FUNCTION
        Attempt to close the Workbench screen. This will fail if there are any
        non-Drawer windows open on it.

    INPUTS

    RESULT
        success - TRUE if Workbench screen could be closed.

    NOTES
        If the Workbench screen is already closed when this function is called,
        FALSE is returned.

    EXAMPLE

    BUGS

    SEE ALSO
        OpenWorkBench()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct Screen *wbscreen;
    BOOL           retval = TRUE;

    wbscreen = GetPrivIBase(IntuitionBase)->WorkBench;


    LockPubScreenList();

    if (!wbscreen)
    {

        UnlockPubScreenList();


        return(FALSE);
    }
    else
    {

        if (GetPrivScreen(wbscreen)->pubScrNode->psn_VisitorCount != 0)
        {

            retval = FALSE;
        }
    }

    UnlockPubScreenList();

    /* If there is a Workbench process running, tell it to close its windows. */

    /* Don't call this function while pub screen list is locked! */

    TellWBTaskToCloseWindows(IntuitionBase);

    LockPubScreenList();

    wbscreen = GetPrivIBase(IntuitionBase)->WorkBench;

    /* Try to close the Workbench screen, if there is any. */
    if( wbscreen != NULL )
    {

        if (CloseScreen(wbscreen))
        {

            GetPrivIBase(IntuitionBase)->WorkBench = NULL;
            wbscreen = NULL;
            retval = TRUE;
        }
        else
        {
            /* Grrr ... closing screen failed. I inc psn_VisitorCount by hand here,
             * to avoid that someone else can kill it, because I must tell Workbench
             * task that it shall open its windows again
             */

            GetPrivScreen(wbscreen)->pubScrNode->psn_VisitorCount++;
            retval = FALSE;
        }
    }
    else
    {
        retval = FALSE;
    }

    UnlockPubScreenList();

    if (wbscreen && !retval)
    {
        /* Don't call this function while pub screen list is locked! */

        TellWBTaskToOpenWindows(IntuitionBase);

        /* Fix psn_VisitorCount which was increased above */

        LockPubScreenList();

        GetPrivScreen(wbscreen)->pubScrNode->psn_VisitorCount--;

        UnlockPubScreenList();

    }

    return retval;

    AROS_LIBFUNC_EXIT

} /* CloseWorkBench */
