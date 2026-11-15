/*
    Copyright (C) 1995-2001, The AROS Development Team. All rights reserved.

    Desc: AttemptRemNamedObject() - attempt to remove a NamedObject
*/
#include "intern.h"

/*****************************************************************************

    NAME */
        #include <proto/utility.h>

        AROS_LH1(LONG, AttemptRemNamedObject,

/*  SYNOPSIS */
        AROS_LHA(struct NamedObject *, object, A0),

/*  LOCATION */
        struct Library *, UtilityBase, 39, Utility)

/*  FUNCTION
        Checks to see whether a NamedObject can be removed. If the object
        is in use, or in the process of being removed, this function will
        return a failure code. If the object can be removed, this function
        will remove it and the object will be available for freeing.
        You must have previously have called FindNamedObject() on this
        object.

    INPUTS
        object      - NamedObject to attempt to remove. The address of the
                        NameSpace is contained within the NamedObject.

    RESULT
        If the NamedObject can be removed, then it will be removed from
        the list. Otherwise the routine will just return.

        If the NamedObject has a removal message associated with it that
        message will be returned to the owner of the NamedObject.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        utility/name.h, RemNamedObject(), AddNamedObject()

    INTERNALS

    HISTORY
        29-10-95    digulla automatically created from
                            utility_lib.fd and clib/utility_protos.h
        11-08-96    iaint   Adapted from stuff I did.

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct IntNamedObject *no = GetIntNamedObject(object);
    struct NameSpace *ns = no->no_ParentSpace;

    /* The caller holds one use; anyone else's makes it fail. The lock
       keeps FindNamedObject() from taking a use between test and removal. */
    if (ns == NULL)
        return TRUE;
    ObtainSemaphore(&ns->ns_Lock);
    if (no->no_UseCount > 1)
    {
        ReleaseSemaphore(&ns->ns_Lock);
        return FALSE;
    }
    Remove((struct Node *)&no->no_Node);
    no->no_ParentSpace = NULL;
    ReleaseSemaphore(&ns->ns_Lock);
    return TRUE;

    AROS_LIBFUNC_EXIT

} /* AttemptRemNamedObject */
