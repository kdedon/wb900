/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function MakeVPort()
*/

#include <graphics/view.h>
#include <graphics/gfxnodes.h>

#include "graphics_intern.h"

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH2(ULONG, MakeVPort,

/*  SYNOPSIS */
        AROS_LHA(struct View *, view, A0),
        AROS_LHA(struct ViewPort *, viewport, A1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 36, Graphics)

/*  FUNCTION
        Prepare a ViewPort to be displayed. Calculate all necessary internal data.
        For Amiga(tm) chipset bitmaps this includes calculating preliminary copperlists.

    INPUTS
        view     - pointer to a View structure
        viewport - pointer to a ViewPort structure
                   the viewport must have a valid pointer to a RasInfo

    RESULT
        error - Result of the operation:
            MVP_OK         - Everything is OK, ViewPort is ready
            MVP_NO_MEM     - There was not enough memory for internal data
            MVP_NO_VPE     - There was no ViewPortExtra for this ViewPort and no memory to
                             allocate a temporary one.
            MVP_NO_DSPINS  - There was not enough memory for Amiga(tm) copperlist.
            MVP_NO_DISPLAY - The BitMap can't be displayed using specified mode (for example,
                             misaligned or wrong depth).

    NOTES
        There are no copperlists: a display shows its bitmap whole, from
        LoadView().

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY


******************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct ViewPortExtra *vpe;

    if(!viewport || !viewport->RasInfo || !viewport->RasInfo->BitMap)
        return MVP_NO_DISPLAY;

    /* Attach a temporary ViewPortExtra if needed */
    vpe = (struct ViewPortExtra *)GfxLookUp(viewport);
    if(!vpe) {
        vpe = (struct ViewPortExtra *)GfxNew(VIEWPORT_EXTRA_TYPE);
        if(!vpe)
            return MVP_NO_VPE;

        vpe->Flags = VPXF_FREE_ME;
        GfxAssociate(viewport, &vpe->n);
    }

    return MVP_OK;

    AROS_LIBFUNC_EXIT
} /* MakeVPort */
