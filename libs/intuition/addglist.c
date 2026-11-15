/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <proto/layers.h>
#include "intuition_intern.h"

/*****************************************************************************

    NAME */
#include <intuition/intuition.h>
#include <proto/intuition.h>
#include "boopsigadgets.h"

        AROS_LH5(UWORD, AddGList,

/*  SYNOPSIS */
        AROS_LHA(struct Window    *, window, A0),
        AROS_LHA(struct Gadget    *, gadget, A1),
        AROS_LHA(UWORD             , position, D0),
        AROS_LHA(WORD              , numGad, D1),
        AROS_LHA(struct Requester *, requester, A2),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 73, Intuition)

/*  FUNCTION
        Add some gadgets to a window.

    INPUTS
        window - Add gadgets to this window
        gadget - This is the list of gadgets to add
        position - Where to insert the gadgets in the list of gadgets
            already in the window. Use 0 to insert the gadgets
            before all others in the window or ~0 to append them.
        numGad - How many gadgets of the list should be added.
            Use -1 to add all gadgets in the list.
        requester - Pointer to the requester structure if the window is
            a requester.

    RESULT
        The actual position where the gadgets were inserted.

    NOTES
        The gadgets will just be added. To make them visible, you must
        refresh the window or the gadgets.

    EXAMPLE

    BUGS

    SEE ALSO
        RefreshGadgets(), RefreshGList()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct Gadget *pred;
    struct Gadget *last;
    UWORD          count, count2;

    if (!gadget || !numGad || !window)
        return (UWORD)-1;

    if (requester && gadget->GadgetType & GTYP_REQGADGET)
    {
        for (last=gadget, count2 = numGad; last && count2--; last=last->NextGadget)
        {

            last->GadgetType |= GTYP_REQGADGET;
        }
        pred = (struct Gadget *)&requester->ReqGadget;
    }
    else
    {
        for (last=gadget, count2 = numGad; last && count2--; last=last->NextGadget)
        {

            last->GadgetType &= ~GTYP_REQGADGET;
        }

        pred = (struct Gadget *)&window->FirstGadget;
        requester = NULL;
    }

    /* Send all GA_RelSpecial BOOPSI gadgets in the list the GM_LAYOUT msg */
    if (!requester || requester->ReqLayer)
    {

        DoGMLayout(gadget, window, requester, numGad, TRUE, IntuitionBase);

    }

    /* gadget list must NOT be modified while gadget accesses are possible */
    LOCKGADGET(IntuitionBase)

    count = 0;
    while (position && pred->NextGadget)
    {
        position --;
        pred = pred->NextGadget;
        count ++;

    }

    count2 = numGad;
    for (last=gadget; last->NextGadget && --count2; last=last->NextGadget);

    last->NextGadget = pred->NextGadget;
    pred->NextGadget = gadget;

    if (!requester || requester->ReqLayer)
    {
        /* Refresh REL gadgets first. Wizard.library (die, die, die!) seems to rely on that. */
        int_refreshglist(gadget, window, requester, numGad, REFRESHGAD_REL, 0, IntuitionBase);
        int_refreshglist(gadget, window, requester, numGad, 0, REFRESHGAD_REL, IntuitionBase);
    }

    UNLOCKGADGET(IntuitionBase)

    return count;
    AROS_LIBFUNC_EXIT
} /* AddGList */
