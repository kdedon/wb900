/*
    Copyright (C) 1995-2011, The AROS Development Team. All rights reserved.
*/

#include <intuition/classes.h>
#include <proto/alib.h>

/******************************************************************************

    NAME */
#include <intuition/classusr.h>
#include <proto/alib.h>

        IPTR CoerceMethodA (

/*  SYNOPSIS */
        Class  * cl,
        Object * obj,
        Msg      message)

/*  FUNCTION
        Invokes a method on a BOOPSI object, as if this was a object, inherited
        from the class passed in. Ie. the dispatcher of the this class is called
        instead of the objects classes dispatcher.

    INPUTS
        cl - Class, which dispatcher is to be called.
        obj - The object, on which the method is to be performed on.
        message - The message. The first field is the same for all methods and
                  specifies which method is to be invokes (see
                  <intuition/classusr.h>).

    RESULT
        Class and method depending. See the class documentation. A value of 0
        can mean a valid return code but can also mean that a method was not
        supported.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

******************************************************************************/
{
        if ((!obj) || (!cl))
                return 0L;

        return CALLHOOKPKT((struct Hook *) cl, obj, message);
} /* CoerceMethodA() */

IPTR CoerceMethod (Class * cl, Object * obj, IPTR MethodID, ...)
{
        if ((!obj) || (!cl))
                return 0L;

        return CALLHOOKPKT((struct Hook *) cl, obj, (Msg)&MethodID);
} /* CoerceMethod() */
