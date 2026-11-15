/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function MrgCop()
*/

#include <graphics/view.h>

#include "graphics_intern.h"

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH1(ULONG, MrgCop,

/*  SYNOPSIS */
        AROS_LHA(struct View *, view, A1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 35, Graphics)

/*  FUNCTION
        Prepare the view for being displayed. Calculate necessary internal data.
        For Amiga(tm) chipset this function also merges together the display, color, sprite and user
        coprocessor instructions into a single coprocessor instruction stream.

    INPUTS
        view - a pointer to the view structure to prepare

    RESULT
        error - ULONG error value indicating either lack of memory to build the system data,
                        or that MrgCop() has no work to do - ie there where no viewPorts in the list.

    NOTES
        Pre-v39 AmigaOS returns void.
        There are no copperlists to merge, so this always succeeds.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY


******************************************************************************/
{
    AROS_LIBFUNC_INIT

    return MCOP_OK;

    AROS_LIBFUNC_EXIT
} /* MrgCop */
