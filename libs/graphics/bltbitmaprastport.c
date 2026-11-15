/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc:
*/

#include "graphics_intern.h"
#include "gfxfuncsupport.h"

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH9 (void, BltBitMapRastPort,

/*  SYNOPSIS */
        AROS_LHA(struct BitMap   *, srcBitMap, A0),
        AROS_LHA(WORD             , xSrc, D0),
        AROS_LHA(WORD             , ySrc, D1),
        AROS_LHA(struct RastPort *, destRP, A1),
        AROS_LHA(WORD             , xDest, D2),
        AROS_LHA(WORD             , yDest, D3),
        AROS_LHA(WORD             , xSize, D4),
        AROS_LHA(WORD             , ySize, D5),
        AROS_LHA(UBYTE            , minterm, D6),

/*  LOCATION */
        struct GfxBase *, GfxBase, 101, Graphics)
            
/*  FUNCTION
        Moves part of a bitmap around or into another bitmap.

    INPUTS
        srcBitMap - Copy from this bitmap.
        xSrc, ySrc - This is the upper left corner of the area to copy.
        destRP - Destination RastPort.
        xDest, yDest - Upper left corner where to place the copy
        xSize, ySize - The size of the area to copy
        minterm - How to copy. See BltBitMap() for an explanation.

    RESULT
        None.

    NOTES
        If special hardware is available, this function will use it.

    EXAMPLE

    BUGS

    SEE ALSO
        ClipBlit()

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT


    FIX_GFXCOORD(xSrc);
    FIX_GFXCOORD(ySrc);
    FIX_GFXCOORD(xDest);
    FIX_GFXCOORD(yDest);

    struct bitmap_render_data brd;
    struct Rectangle          rr;
    Point                     src;

    brd.minterm = minterm;
    brd.srcbm   = srcBitMap;

    rr.MinX = xDest;
    rr.MinY = yDest;
    rr.MaxX = xDest + xSize - 1;
    rr.MaxY = yDest + ySize - 1;

    src.x = xSrc;
    src.y = ySrc;

    do_render_func(destRP, &src, &rr, bitmap_render, &brd, GfxBase);


    AROS_LIBFUNC_EXIT

} /* BltBitMapRastPort */
