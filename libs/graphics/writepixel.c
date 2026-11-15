/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function WritePixel()
*/

#include <graphics/rastport.h>
#include <graphics/clip.h>
#include <proto/graphics.h>

#include "graphics_intern.h"
#include "gfxfuncsupport.h"

static LONG pix_write(APTR pr_data, struct BitMap *bm, WORD x, WORD y,
                      struct GfxPens *pens, struct GfxBase *GfxBase);

/*****************************************************************************

    NAME */

        AROS_LH3(LONG, WritePixel,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort *, rp, A1),
        AROS_LHA(WORD             , x, D0),
        AROS_LHA(WORD             , y, D1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 54, Graphics)

/*  FUNCTION
        Write the primary (A) pen colour to the given coordinates of a
        RastPort.

    INPUTS
        rp  - destination RastPort
        x,y - coordinate

    RESULT
         0: pixel could be written
        -1: coordinate was outside rastport

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS
        This function takes layers into account. Some pixel that is
        being read is not found on the display-bitmap but in some
        clipped rectangle (cliprect) in a layer structure.

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    FIX_GFXCOORD(x);
    FIX_GFXCOORD(y);

    return do_pixel_func(rp, x, y, pix_write, NULL, GfxBase);

    AROS_LIBFUNC_EXIT
} /* WritePixel */


/*
 * The foreground pen in the RastPort's draw mode, as AmigaOS does (AROS
 * wrote the pen whatever the mode).
 */
static LONG pix_write(APTR pr_data, struct BitMap *bm, WORD x, WORD y,
                      struct GfxPens *pens, struct GfxBase *GfxBase)
{
    struct Rectangle r;

    pens->gp_Ptrn = NULL;
    if(pens->gp_DrMd & INVERSVID) {
        r.MinX = r.MaxX = x;
        r.MinY = r.MaxY = y;
        GFX_DRIVER(bm)->gd_Fill(bm, &r, pens);
    } else
        GFX_DRIVER(bm)->gd_WritePixel(bm, x, y, pens);

    return 0;
}
