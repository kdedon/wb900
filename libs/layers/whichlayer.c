/*
    Copyright (C) 1995-2007, The AROS Development Team. All rights reserved.

    Desc:
*/
#include <aros/libcall.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include "layers_intern.h"
#include "basicfuncs.h"

/*****************************************************************************

    NAME */

        AROS_LH3(struct Layer *, WhichLayer,

/*  SYNOPSIS */
        AROS_LHA(struct Layer_Info *, li, A0),
        AROS_LHA(LONG               , x,  D0),
        AROS_LHA(LONG               , y,  D1),

/*  LOCATION */
        struct LayersBase *, LayersBase, 22, Layers)

/*  FUNCTION
        Determines in which layer the point (x,y) is to be found.
        Starts with the frontmost layer.

    INPUTS
        li - pointer to Layers_Info structure
        x  - x-coordinate
        y  - y-coordinate

    RESULT

    NOTES
        The function does not lock Layer_Info structure. It is
        the responsibility of the caller to issue the lock via
        LockLayerInfo()/UnlockLayerInfo()

        A layer with an input shape (LA_InputShape) is hit only inside it.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY
        27-11-96    digulla automatically created from
                            layers_lib.fd and clib/layers_protos.h

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct Layer *l;

    /* Front to back, the first layer the point hits */
    for
    (
        l = li->top_layer;
        l != NULL && !_IsLayerHitAt(l, x, y, LayersBase);
        l = l->back
    );

    return l;

    AROS_LIBFUNC_EXIT
} /* WhichLayer */
