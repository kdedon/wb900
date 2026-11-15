/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <proto/layers.h>
#include "intuition_intern.h"
#include "inputhandler_actions.h"

struct ChangeWindowShapeActionMsg
{
    struct IntuiActionMsg    msg;
    struct Window           *window;
    struct Region           *shape;
    struct Hook             *callback;
};

static VOID int_changewindowshape(struct ChangeWindowShapeActionMsg *msg,
                                  struct IntuitionBase *IntuitionBase);

/*****************************************************************************

    NAME */
#include <proto/intuition.h>

        AROS_LH3(struct Region *, ChangeWindowShape,

/*  SYNOPSIS */
        AROS_LHA(struct Window *, window, A0),
        AROS_LHA(struct Region *, newshape, A1),
        AROS_LHA(struct Hook *, callback, A2),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 143, Intuition)

/*  FUNCTION

    INPUTS
        window - The window to affect.

    RESULT

    NOTES
        This function is also present in MorphOS v50, however
        not implemented and reserved.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct ChangeWindowShapeActionMsg msg;

    if (IS_GZZWINDOW(window)) return NULL;

    msg.window   = window;
    msg.shape    = newshape;
    msg.callback = callback;
    DoSyncAction((APTR)int_changewindowshape, &msg.msg, IntuitionBase);

    return msg.shape;

    AROS_LIBFUNC_EXIT

} /* ChangeWindowShape */

static VOID int_changewindowshape(struct ChangeWindowShapeActionMsg *msg,
                                  struct IntuitionBase *IntuitionBase)
{
    struct LayersBase *LayersBase = GetPrivIBase(IntuitionBase)->LayersBase;
    struct Window   *window = msg->window;
    struct Region   *shape = msg->shape;
    struct Hook     *callback = msg->callback;
    struct Screen   *screen = window->WScreen;

    if (!ResourceExisting(window, RESOURCE_WINDOW, IntuitionBase)) return;

    LOCK_REFRESH(screen);
    msg->shape = ChangeLayerShape(window->WLayer, shape, callback);
    UNLOCK_REFRESH(screen);

    CheckLayers(screen, IntuitionBase);
}
