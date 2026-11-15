/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function NextDisplayInfo()
*/
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include "graphics_intern.h"

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH1(ULONG, NextDisplayInfo,

/*  SYNOPSIS */
        AROS_LHA(ULONG, last_ID, D0),

/*  LOCATION */
        struct GfxBase *, GfxBase, 122, Graphics)

/*  FUNCTION
        Go to next entry in the DisplayInfo database.

    INPUTS
        last_ID - previous displayinfo identifier
                  or INVALID_ID if beginning iteration

    RESULT
        next_ID - subsequent displayinfo identifier
                  or INVALID_ID if no more records

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        FindDisplayInfo(), GetDisplayInfoData(), graphics/displayinfo.h

    INTERNALS

    HISTORY


******************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct GfxDriver *gd = gfx_Display(GfxBase);
    const struct GfxMode *m;
    UWORD i = 0;

    if(!gd)
        return INVALID_ID;
    if(last_ID != INVALID_ID) {
        /* Find the current mode, then take the next one */
        m = gfx_FindMode(last_ID, GfxBase);
        if(!m)
            return INVALID_ID;
        i = m - gd->gd_Modes + 1;
    }
    return i < gd->gd_NumModes ? gd->gd_Modes[i].gm_ModeID : INVALID_ID;

    AROS_LIBFUNC_EXIT
} /* NextDisplayInfo */
