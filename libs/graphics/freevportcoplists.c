/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function FreeVPortCopLists()
*/

#include <graphics/view.h>
#include <graphics/gfxnodes.h>

#include "graphics_intern.h"

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH1(void, FreeVPortCopLists,

/*  SYNOPSIS */
        AROS_LHA(struct ViewPort *, vp, A0),

/*  LOCATION */
        struct GfxBase *, GfxBase, 90, Graphics)

/*  FUNCTION
        Deallocate all memory associated with the CopList structures
        for display, color, sprite and the user copper list. The
        corresponding fields in this structure will be set to NULL.

    INPUTS
        vp - pointer to a ViewPort structure

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        graphics/view.h

    INTERNALS

    HISTORY

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct ViewPortExtra *vpe;

    vpe = (struct ViewPortExtra *)GfxLookUp(vp);
    if(vpe && (vpe->Flags & VPXF_FREE_ME))
        GfxFree(&vpe->n);

    AROS_LIBFUNC_EXIT
} /* FreeVPortCopLists */
