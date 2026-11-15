/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <proto/graphics.h>
#include <proto/utility.h>
#include "intuition_intern.h"
#include <intuition/pointerclass.h>

/*****************************************************************************

    NAME */
#include <proto/intuition.h>

        AROS_LH2(void, SetWindowPointerA,

/*  SYNOPSIS */
        AROS_LHA(struct Window * , window , A0),
        AROS_LHA(struct TagItem *, taglist, A1),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 136, Intuition)

/*  FUNCTION

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct Library *UtilityBase = GetPrivIBase(IntuitionBase)->UtilityBase;

    if (window)
    {
        ULONG   lock;
        Object *pointer = (Object *)GetTagData(WA_Pointer, 0, taglist);
        BOOL    busy = (GetTagData(WA_BusyPointer, FALSE, taglist) != 0) ? TRUE : FALSE;

        lock = LockIBase(0);

        if (IW(window)->pointer != pointer || IW(window)->busy != busy)
        {
            Object *oldpointer = NULL;

            if (IW(window)->free_pointer)
                oldpointer = IW(window)->pointer;

            IW(window)->pointer = pointer;
            IW(window)->busy = busy;
            IW(window)->free_pointer = FALSE;
            window->Pointer = NULL;

            if (window->Flags & WFLG_WINDOWACTIVE)
            {
                struct IntScreen *scr = GetPrivScreen(window->WScreen);

                if (GetTagData(WA_PointerDelay, FALSE, taglist) &&
                        IntuitionBase->ActiveScreen == &scr->Screen)
                {

                    GetPrivIBase(IntuitionBase)->PointerDelay = 5;
                }
                else
                {
                    if (pointer == NULL)
                        pointer = GetPrivIBase(IntuitionBase)->DefaultPointer;
                    if (busy)
                        pointer = GetPrivIBase(IntuitionBase)->BusyPointer;

                    SetScreenPointer(scr, window, pointer, IntuitionBase);
                }
            }

            DisposeObject(oldpointer);
        }

        UnlockIBase(lock);
    }

    AROS_LIBFUNC_EXIT
} /* SetWindowPointerA */
