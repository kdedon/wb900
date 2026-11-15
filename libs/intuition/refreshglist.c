/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <proto/layers.h>
#include "intuition_intern.h"
#include "boolgadgets.h"
#include "boopsigadgets.h"
#include "propgadgets.h"
#include "strgadgets.h"

#include <intuition/classes.h>

BOOL qualifygadget(struct Gadget *gadgets,LONG mustbe, LONG mustnotbe,struct IntuitionBase *IntuitionBase);
void rendergadget(struct Gadget *gadgets,struct Window *window, struct Requester *requester,struct IntuitionBase *IntuitionBase);

/*****************************************************************************

    NAME */
#include <intuition/intuition.h>
#include <proto/intuition.h>

        AROS_LH4(void, RefreshGList,

/*  SYNOPSIS */
        AROS_LHA(struct Gadget    *, gadgets, A0),
        AROS_LHA(struct Window    *, window, A1),
        AROS_LHA(struct Requester *, requester, A2),
        AROS_LHA(WORD              , numGad, D0),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 72, Intuition)

/*  FUNCTION
        Refresh (draw anew) the specified number of gadgets starting
        at the specified gadget.

    INPUTS
        gadgets - This is the first gadget which will be refreshed.
        window - The window which contains the gadget
        requester - If the gadget has GTYP_REQGADGET set, this must be
            a pointer to a Requester; otherwise the value is
            ignored.
        numGad - How many gadgets should be refreshed. The value
            may range from 0 to MAXLONG. If there are less gadgets
            in the list than numGad, only the gadgets in the
            list will be refreshed.

    RESULT
        None.

    NOTES
        This function *must not* be called inside a
        BeginRefresh()/EndRefresh() pair.

    EXAMPLE
        // Refresh one gadget
        RefreshGList (&gadget, win, NULL, 1);

        // Refresh all gadgets in the window
        RefreshGList (win->FirstGadget, win, NULL, -1L);

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    if (!gadgets || !numGad)
        return;

    if ((gadgets->GadgetType & GTYP_REQGADGET) == 0)
    {
        requester = NULL;
    }
    else if (numGad == -2)
    {
        gadgets = requester->ReqGadget;
    }

    LOCKGADGET(IntuitionBase)

    int_refreshglist(gadgets,
                     window,
                     requester,
                     numGad,
                     0,
                     0,
                     IntuitionBase);

    UNLOCKGADGET(IntuitionBase)

    return;

    AROS_LIBFUNC_EXIT
} /* RefreshGList */

void int_refreshglist(struct Gadget *gadgets, struct Window *window,
                      struct Requester *requester, LONG numGad, LONG mustbe, LONG mustnotbe,
                      struct IntuitionBase *IntuitionBase)
{

    // in case we're not called from RefreshGList...
    LOCKGADGET(IntuitionBase)

    for ( ; gadgets && numGad; gadgets=gadgets->NextGadget, numGad --)
    {

        if (!(qualifygadget(gadgets,mustbe,mustnotbe,IntuitionBase))) continue;
        rendergadget(gadgets,window,requester,IntuitionBase);
    } /* for ( ; gadgets && numGad; gadgets=gadgets->NextGadget, numGad --) */

    UNLOCKGADGET(IntuitionBase)
}

BOOL qualifygadget(struct Gadget *gadgets,LONG mustbe, LONG mustnotbe,struct IntuitionBase *IntuitionBase)
{
    if ((mustbe != 0) || (mustnotbe != 0))
    {
        if (gadgets->Activation & (GACT_LEFTBORDER | GACT_RIGHTBORDER |
                                   GACT_TOPBORDER  | GACT_BOTTOMBORDER |
                                   GACT_BORDERSNIFF))
        {
            if (mustnotbe & REFRESHGAD_BORDER) return FALSE; /* don't refresh if border gadget */
        }
        else
        {
            if (mustbe & REFRESHGAD_BORDER) return FALSE; /* don't refresh if not a border gadget */
        }

        if (gadgets->Activation & GACT_TOPBORDER)
        {
            if (mustnotbe & REFRESHGAD_TOPBORDER) return FALSE; /* don't refresh if border gadget */
        }
        else
        {
            if (mustbe & REFRESHGAD_TOPBORDER) return FALSE; /* don't refresh if not a border gadget */
        }

        if (gadgets->Flags & (GFLG_RELRIGHT | GFLG_RELBOTTOM |
                              GFLG_RELWIDTH | GFLG_RELHEIGHT))
        {
            if (mustnotbe & REFRESHGAD_REL) return FALSE; /* don't refresh if rel??? gadget */
        }
        else
        {
            if (mustbe & REFRESHGAD_REL) return FALSE; /* don't refresh if not rel??? gadget */
        }

        if (gadgets->Flags & GFLG_RELSPECIAL)
        {
            if (mustnotbe & REFRESHGAD_RELS) return FALSE; /* don't refresh if relspecial gadget */
        }
        else
        {
            if (mustbe & REFRESHGAD_RELS) return FALSE; /* don't refresh if not relspecial gadget */
        }

        if ((gadgets->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET)
        {
            if (mustnotbe & REFRESHGAD_BOOPSI) return FALSE; /* don't refresh if boopsi gadget */
        }
        else
        {
            if (mustbe & REFRESHGAD_BOOPSI) return FALSE; /* don't refresh if not boopsi gadget */
        }

    } /* if ((mustbe != 0) || (mustnotbe != 0)) */
    return TRUE;
}

void rendergadget(struct Gadget *gadgets,struct Window *window, struct Requester *requester,struct IntuitionBase *IntuitionBase)
{
    switch (gadgets->GadgetType & GTYP_GTYPEMASK)
    {
        case GTYP_BOOLGADGET:
            RefreshBoolGadget (gadgets, window, requester, IntuitionBase);
            break;

        case GTYP_GADGET0002:
            break;

        case GTYP_PROPGADGET:
            RefreshPropGadget (gadgets, window, requester, IntuitionBase);
            break;

        case GTYP_STRGADGET:
            RefreshStrGadget (gadgets, window, requester, IntuitionBase);
            break;

        case GTYP_CUSTOMGADGET:
            RefreshBoopsiGadget (gadgets, window, requester, IntuitionBase);
            break;

        default:
            RefreshBoolGadget (gadgets, window, requester, IntuitionBase);
            break;

    } /* switch GadgetType */
}
