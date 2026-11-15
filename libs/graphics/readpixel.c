/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function ReadPixel()
*/

#include <graphics/rastport.h>
#include <graphics/clip.h>
#include <proto/graphics.h>

#include "graphics_intern.h"
#include "gfxfuncsupport.h"


static LONG pix_read(APTR pr_data, struct BitMap *bm, WORD x, WORD y,
                     struct GfxPens *pens, struct GfxBase *GfxBase);

/*****************************************************************************

    NAME */

        AROS_LH3(LONG, ReadPixel,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort *, rp, A1),
        AROS_LHA(WORD             , x, D0),
        AROS_LHA(WORD             , y, D1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 53, Graphics)

/*  FUNCTION
        Read the pen number of the given x,y coordinate.

    INPUTS
        rp  - RastPort
        x,y - Coordinate

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS
        This function takes layers into account. Some pixel that is
        being read is not found on the display-bitmap but in some
        clipped rectangle (cliprect) in a layer structure.
        There is no support of anything else than bitplanes now.
        (No chunky pixels)
        This function resembles very much the function WritePixel()!!

    HISTORY
        29-10-95    digulla automatically created from
                            graphics_lib.fd and clib/graphics_protos.h

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    ULONG pen = -1;
    LONG  ret;

    FIX_GFXCOORD(x);
    FIX_GFXCOORD(y);

    ret = do_pixel_func(rp, x, y, pix_read, &pen, GfxBase);

    if(-1 == ret || -1 == (LONG)pen) {
        return (ULONG) - 1;
    }

    return pen;

    AROS_LIBFUNC_EXIT

} /* ReadPixel */

/* The pen the pixel stands for: the first one the pixel table gives it */
static LONG pix_read(APTR pr_data, struct BitMap *bm, WORD x, WORD y,
                     struct GfxPens *pens, struct GfxBase *GfxBase)
{
    ULONG pix = GFX_DRIVER(bm)->gd_ReadPixel(bm, x, y);
    UBYTE *pixtab = IS_GFXBM(bm) ? GFXBM(bm)->gb_PixTab : NULL;
    UWORD pen;

    if(pixtab) {
        for(pen = 0; pen < 256 && pixtab[pen] != pix; pen++)
            ;
        pix = pen < 256 ? pen : 0;
    }

    *(ULONG *)pr_data = pix;

    return 0;
}
