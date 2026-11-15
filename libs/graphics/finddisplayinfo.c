/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function FindDisplayInfo()
*/
#include <proto/graphics.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>

#include "graphics_intern.h"

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH1(DisplayInfoHandle, FindDisplayInfo,

/*  SYNOPSIS */
        AROS_LHA(ULONG, ID, D0),

/*  LOCATION */
        struct GfxBase *, GfxBase, 121, Graphics)

/*  FUNCTION
        Search for a DisplayInfo which matches the ID key.

    INPUTS
        ID - identifier

    RESULT
        handle - handle to a displayinfo record with that key
                 or NULL if no match

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        graphics/displayinfo.h

    INTERNALS

    HISTORY


******************************************************************************/
{
    AROS_LIBFUNC_INIT

    /* The database is the display driver's modes */
    return (DisplayInfoHandle)gfx_FindMode(ID, GfxBase);

    AROS_LIBFUNC_EXIT
} /* FindDisplayInfo */
