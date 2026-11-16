/*
    Copyright (C) 1995-2013, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <graphics/displayinfo.h>
#include <proto/graphics.h>

#include "intuition_intern.h"

/*****************************************************************************

    NAME */
#include <proto/intuition.h>

        AROS_LH3(LONG, QueryOverscan,

/*  SYNOPSIS */
        AROS_LHA(ULONG             , displayid, A0),
        AROS_LHA(struct Rectangle *, rect     , A1),
        AROS_LHA(WORD              , oscantype, D0),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 79, Intuition)

/*  FUNCTION
        Query overscan dimensions. The resulting rectangle can be used
        with SA_DisplayID.

        Overscan types:
        OSCAN_TEXT: completely visible. Left/Top is always 0,0.
        OSCAN_STANDARD: visible bounds of monitor. Left/Top may be negative.
        OSCAN_MAX: The largest displayable region.
        OSCAN_VIDEO: The absolute largest region that the graphics.library
            can display.  This region must be used as-is.

    INPUTS
        displayid - ID to be queried
        rect      - Pointer to struct Rectangle to store result
        oscantype - OSCAN_TEXT, OSCAN_STANDARD, OSCAN_MAX, OSCAN_VIDEO

    RESULT
        TRUE  - Monitorspec exists
        FALSE - Monitorspec doesn't exist

    NOTES
        The C900's displays have no overscan: every type is the whole
        display.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct GfxBase      *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct DimensionInfo diminfo;
    LONG                 retval = FALSE;

    if (GetDisplayInfoData(NULL, (UBYTE *)&diminfo, sizeof(diminfo), DTAG_DIMS, displayid) > 0)
    {
        retval = TRUE;

        switch(oscantype)
        {
            case OSCAN_TEXT:
                memcpy(rect,&diminfo.TxtOScan,sizeof(struct Rectangle));
                break;

            case OSCAN_STANDARD:
                memcpy(rect,&diminfo.StdOScan,sizeof(struct Rectangle));
                break;

            case OSCAN_MAX:
                memcpy(rect,&diminfo.MaxOScan,sizeof(struct Rectangle));
                break;

            case OSCAN_VIDEO:
                memcpy(rect,&diminfo.VideoOScan,sizeof(struct Rectangle));
                break;

            default:
                /* or should we assume OSCAN_TEXT? */
                retval = FALSE;
                break;

        } /* switch(oscantype) */

    } /* if (GetDisplayInfoData(NULL, &diminfo, sizeof(diminfo), DTAG_DIMS, displayid) > 0) */

    return retval;

    AROS_LIBFUNC_EXIT

} /* QueryOverscan */
