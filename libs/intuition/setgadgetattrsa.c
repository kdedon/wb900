/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <exec/memory.h>
#include <intuition/cghooks.h>
#include <intuition/classusr.h>
#include <intuition/classes.h>
#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include "intuition_intern.h"

/*****************************************************************************

    NAME */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/intuition.h>
#include <utility/tagitem.h>

        AROS_LH4(IPTR, SetGadgetAttrsA,

/*  SYNOPSIS */
        AROS_LHA(struct Gadget *,    gadget,    A0),
        AROS_LHA(struct Window *,    window,    A1),
        AROS_LHA(struct Requester *, requester, A2),
        AROS_LHA(struct TagItem *,   tagList,   A3),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 110, Intuition)

/*  FUNCTION
        Sets some tags and provides gadget specific data. Prefer this to
        SetAttrsA(), if you are manipulating gadgets.

    INPUTS
        gadget - Change the attributes of this gadget
        window - The window of the gadget
        requester - The requester of the gadget (or NULL)
        tagList - This is a list of attribute/value-pairs

    RESULT
        Depends in the class. For gadgets, this value is non-zero if
        they need redrawing after the values have changed. Other classes
        will define other return values.

    NOTES
        This function sends OM_SET to the gadget object.

    EXAMPLE

    BUGS

    SEE ALSO
        NewObjectA(), SetAttrsA(), GetAttr(), DoGadgetMethodA(),
        "Basic Object-Oriented Programming System for Intuition" and
        "Boopsi Class Reference" Document.

    INTERNALS
        SetGadgetAttrsA(gad, win, req, tags) is just a replacement for
        DoGadgetMethod(gad, win, req, OM_SET, tags, NULL).

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct opSet ops;
    IPTR         result;

    SANITY_CHECKR(gadget,FALSE)
    SANITY_CHECKR(tagList,FALSE)

    ops.MethodID     = OM_SET;
    ops.ops_AttrList = tagList;
    ops.ops_GInfo    = 0; /* Not really necessary as it will be filled in by DoGadgetMethodA */

    if (window || requester)
    {
        result = DoGadgetMethodA(gadget, window, requester, (Msg)&ops);
    }
    else
    {
        result = DoMethodA((Object *)gadget, (Msg)&ops);
    }

    return result;

    AROS_LIBFUNC_EXIT

} /* SetGadgetAttrsA */
