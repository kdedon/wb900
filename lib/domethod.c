/*
    Copyright (C) 1995-2011, The AROS Development Team. All rights reserved.
*/

#include <intuition/classes.h>


/******************************************************************************

    NAME */
#include <intuition/classusr.h>
#include <proto/alib.h>

        IPTR DoMethodA (

/*  SYNOPSIS */
        Object * obj,
        Msg      message)

/*  FUNCTION
        Invokes a method on a BOOPSI object. The dispatcher of the class the
        object is inherited from is called. For more information about methods
        a class supports, see the class documentation.

    INPUTS
        obj - The object on which the method is to be performed.
        message - The message. The first field is the same for all methods and
                  specifies which method is to be invoked (see
                  <intuition/classusr.h>).

    RESULT
        Class and method dependent. See the class documentation. A value of 0
        can be a valid return code but can also mean that a method is not
        supported.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        intuition.library/NewObjectA(), intuition.library/SetAttrsA(), intuition.library/GetAttr(),
        intuition.library/DisposeObject(), CoerceMethodA(),
        DoSuperMethodA(), <intuition/classusr.h>

******************************************************************************/
{
    if (!obj)
        return 0L;

    return CALLHOOKPKT((struct Hook *)OCLASS(obj), obj, message);
} /* DoMethodA */

IPTR DoMethod (Object * obj, IPTR MethodID, ...)
{
    if (!obj)
        return 0L;

    return CALLHOOKPKT((struct Hook *)OCLASS(obj), obj, (Msg)&MethodID);
} /* DoMethod() */
