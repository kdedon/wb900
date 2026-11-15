/*
    Copyright (C) 1995-2007, The AROS Development Team. All rights reserved.

    Desc: Free memory allocated by CreateMenusA()
*/
#include <proto/exec.h>
#include "gadtools_intern.h"

/*********************************************************************

    NAME */
#include <proto/gadtools.h>
#include <intuition/intuition.h>

        AROS_LH1(VOID, FreeMenus,

/*  SYNOPSIS */
        AROS_LHA(struct Menu *, menu, A0),

/*  LOCATION */
        struct Library *, GadToolsBase, 9, GadTools)

/*  FUNCTION
        Frees the menus allocated by CreateMenusA().

    INPUTS
        menu - pointer to the menu (or first MenuItem) to be freed, may be NULL.

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        CreateMenusA()

    INTERNALS

    HISTORY

***************************************************************************/
{
    AROS_LIBFUNC_INIT

    /*
     * ULONG BarLabelTable[BarLabelCount]
     * ULONG BarLabelCount
     * ULONG Menu==0|Item!=0
     * Menu:
     */
    if (menu)
    {
        ULONG *p = (ULONG *)menu - 1;
        ULONG BarLabels;
        int i;


        p = p - 1;
        /*
         * p points now on BarLabelCount
         */
        BarLabels       =       *p;
        for (i=0;i<BarLabels;i++)
        {
                // get the next Image pointer address
                p = (ULONG *)((IPTR) p - sizeof(APTR));
                if (*(APTR*)p)
                {
                        DisposeObject(*(APTR*)p);
                }
        }
        FreeVec(p);
    }

    AROS_LIBFUNC_EXIT
} /* FreeMenus */
