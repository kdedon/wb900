/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include "intuition_intern.h"

/*****************************************************************************

    NAME */
#include <proto/intuition.h>

        AROS_LH0(VOID, UnlockPubScreenList,

/*  SYNOPSIS */

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 88, Intuition)

/*  FUNCTION
        Release lock made by LockPubScreenList().

    INPUTS
        None.

    RESULT
        None.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        LockPubScreenList()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ReleaseSemaphore(&GetPrivIBase(IntuitionBase)->PubScrListLock);

    AROS_LIBFUNC_EXIT
} /* UnlockPubScreenList */
