/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.

    Find a BOOPSI Class in the class list.
*/

#include <exec/lists.h>
#include <proto/exec.h>

#include "intuition_intern.h"

#undef SDEBUG

/*****i************************************************************************

    NAME */
#include <intuition/classes.h>
#include <proto/intuition.h>

        AROS_LH1(struct IClass *, FindClass,

/*  SYNOPSIS */
        AROS_LHA(ClassID,  classID, A0),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 112, Intuition)

/*  FUNCTION

    INPUTS

    RESULT

    NOTES
        This function is binary-compatible with AmigaOS v3, however it is
        considered private.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

******************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct IntIntuitionBase *intuitionBase = GetPrivIBase(IntuitionBase);
    Class * classPtr = NULL;

    

    if (!classID)
        return NULL;

    /* Lock the list */
    ObtainSemaphoreShared (&intuitionBase->ClassListLock);

    /* Search for the class */
    for (classPtr = (Class *)intuitionBase->ClassList.mlh_Head; ((struct Node *)classPtr)->ln_Succ; classPtr = (Class *)((struct Node *)classPtr)->ln_Succ)
    {

        if (!strcmp (classPtr->cl_ID, classID))
            goto found;
    }

    classPtr = NULL; /* Nothing found */

found:
    /* Unlock list */
    ReleaseSemaphore (&intuitionBase->ClassListLock);

    return classPtr;

    AROS_LIBFUNC_EXIT
}
