/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function WaitBOVP()
*/
#include <graphics/view.h>

#include "graphics_intern.h"

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH1(void, WaitBOVP,

/*  SYNOPSIS */
        AROS_LHA(struct ViewPort *, vp, A0),

/*  LOCATION */
        struct GfxBase *, GfxBase, 67, Graphics)

/*  FUNCTION

        Wait until the beam has passed the bottom of the ViewPort.

    INPUTS
        vp - pointer to ViewPort structure

    RESULT
        None.

    NOTES
        There is no beam to follow: this waits for the next 100 Hz tick,
        as WaitTOF().

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    WaitTOF();

    AROS_LIBFUNC_EXIT
} /* WaitBOVP */
