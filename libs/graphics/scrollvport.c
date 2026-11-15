/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function ScrollVPort()
*/

#include "graphics_intern.h"

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH1(void, ScrollVPort,

/*  SYNOPSIS */
        AROS_LHA(struct ViewPort *, vp, A0),

/*  LOCATION */
        struct GfxBase *, GfxBase, 98, Graphics)

/*  FUNCTION
        Move the ViewPort to the position specified in DxOffset and DyOffset
        members of the ViewPort structure.

    INPUTS

    RESULT
        None.

    NOTES
        AROS video drivers can perform a validation of offsets, and may refuse
        to scroll the screen too far (if they somehow can't provide the requested
        offset). In this case offset values in the ViewPort will be updated in
        order to reflect the real result of the operation.

        The displays have no display start to move: a ViewPort on
        display with a bitmap in RAM is copied to the display from its
        new offsets, and one with the display's own bitmap stays.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY


******************************************************************************/
{
    AROS_LIBFUNC_INIT

    ObtainSemaphore(GfxBase->ActiViewCprSemaphore);
    if(gfx_ViewPortShown(vp, GfxBase))
        LoadView(GfxBase->ActiView);
    ReleaseSemaphore(GfxBase->ActiViewCprSemaphore);

    AROS_LIBFUNC_EXIT
} /* ScrollVPort */
