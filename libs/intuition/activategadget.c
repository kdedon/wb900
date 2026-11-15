/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include "intuition_intern.h"
#include "inputhandler_actions.h"
#include "inputhandler.h"
#include "gadgets.h"

#include <proto/exec.h>

struct ActivateGadgetActionMsg
{
    struct IntuiActionMsg    msg;
    struct Window           *window;
    struct Requester        *requester;
    struct Gadget           *gadget;
    BOOL                     success;
};

#define DEBUG_ACTIVATEGADGET(x) ;

static VOID int_activategadget(struct ActivateGadgetActionMsg *msg,
                               struct IntuitionBase *IntuitionBase);

/*****************************************************************************

    NAME */
#include <proto/intuition.h>

        AROS_LH3(BOOL, ActivateGadget,

/*  SYNOPSIS */
        AROS_LHA(struct Gadget *   , gadget, A0),
        AROS_LHA(struct Window *   , window, A1),
        AROS_LHA(struct Requester *, requester, A2),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 77, Intuition)

/*  FUNCTION
        Activates the specified gadget.

    INPUTS
        gadget - The gadget to activate
        window - The window which contains the gadget
        requester - The requester which contains the gadget or
            NULL if it is not a requester gadget

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    BOOL success = FALSE;

    if (gadget && (window || requester))
    {
        if (1)
        {
            if ((((gadget->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET) && (!(gadget->Activation & GACT_ACTIVEGADGET))) ||
                ((gadget->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET))
            {
                struct ActivateGadgetActionMsg msg;

                msg.window    = window;
                msg.requester = requester;
                msg.gadget    = gadget;

                DoSyncAction((APTR)int_activategadget, &msg.msg, IntuitionBase);

                success = msg.success;
            }
            else
            {

            }

        } /* if (!(gadget->Activation & GACT_ACTIVEGADGET)) */
        else
        {

            success = TRUE;
        }

    } /* if (gadget && (window || requester)) */
    else
    {

    }

    return success;

    AROS_LIBFUNC_EXIT

} /* ActivateGadget */

static VOID int_activategadget(struct ActivateGadgetActionMsg *msg,
                               struct IntuitionBase *IntuitionBase)
{
    struct IIHData   *iihdata = (struct IIHData *)GetPrivIBase(IntuitionBase)->InputHandler->is_Data;
    struct Window    *window = msg->window;
    struct Requester *req = msg->requester;
    struct Gadget    *gadget = msg->gadget;

    msg->success = FALSE;

    /* Activate gadget only if no other gadget is
       actually active and the gadget to be
       activated is in the actual active window
       and the gadget is not disabled */

    if ((iihdata->ActiveGadget == NULL) &&
        (IntuitionBase->ActiveWindow == window) &&
        ((gadget->Flags & GFLG_DISABLED) == 0))
    {
        msg->success = (DoActivateGadget(window, req, gadget, IntuitionBase) != NULL);
    }
}
