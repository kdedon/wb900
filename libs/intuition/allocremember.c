/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include "intuition_intern.h"
#include <exec/memory.h>
#include <proto/exec.h>

/*****************************************************************************

    NAME */
#include <intuition/intuition.h>
#include <proto/intuition.h>

        AROS_LH3(APTR, AllocRemember,

/*  SYNOPSIS */
        AROS_LHA(struct Remember **, rememberKey, A0),
        AROS_LHA(ULONG             , size, D0),
        AROS_LHA(ULONG             , flags, D1),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 66, Intuition)

/*  FUNCTION
        Allocate some memory and remember it in the Remember-List.

    INPUTS
        rememberKey - Store information in this list. Must be NULL for
                      initial call.
        size - How many bytes to allocate
        flags - Attributes (see AllocMem())

    RESULT
        Pointer to the allocated memory or NULL.

    NOTES

    EXAMPLE
        struct Remember *remkey;
        remkey = NULL;
        AllocRemember(&remkey, BUFSIZE, MEMF_ANY);
        FreeRemember(&remkey, TRUE);

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct Remember *newKey;
    APTR             ptr = NULL;

    newKey = AllocMem (sizeof (struct Remember), MEMF_ANY);

    if (newKey)
    {
        ptr = AllocMem (size, flags);

        if (ptr)
        {
            newKey->NextRemember = *rememberKey;
            newKey->Memory       = ptr;
            newKey->RememberSize = size;

            *rememberKey = newKey;
        }
        else
        {
            FreeMem (newKey, sizeof (struct Remember));
        }
    }

    return ptr;

    AROS_LIBFUNC_EXIT
} /* AllocRemember */
