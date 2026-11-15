/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Set one color register for this Viewport
*/

#include <proto/graphics.h>

#include "graphics_intern.h"
#include "gfxfuncsupport.h"

/*
 * The display of the ViewPort's bitmap gets the colours: a programmable
 * palette is loaded, and with a fixed one the pens take the nearest of
 * its colours.
 */
void internal_SetRGB32Colors(struct ViewPort *vp, UBYTE *rgb,
                             ULONG first, ULONG count,
                             struct GfxBase *GfxBase)
{
    struct BitMap *bm = vp->RasInfo ? vp->RasInfo->BitMap : NULL;
    struct GfxDriver *gd;
    UBYTE *pixtab;
    ULONG i, best, d, bestd, c;
    UWORD p;
    WORD dr, dg, db;

    if (!bm || !IS_GFXBM(bm) || !(GFXBM(bm)->gb_Flags & GBF_DISPLAY))
        return;
    gd = GFXBM(bm)->gb_Driver;
    pixtab = GFXBM(bm)->gb_PixTab;

    for (i = 0; i < count; i++, rgb += 3)
    {
        if (gd->gd_SetColor)
            gd->gd_SetColor(gd, first + i, rgb[0], rgb[1], rgb[2]);
        else if (pixtab && first + i < 256)
        {
            bestd = 0xFFFFFFFFL;
            best = 0;
            for (p = 0; p < (1 << bm->Depth); p++)
            {
                c = gd->gd_GetColor(gd, p);
                dr = (WORD)((c >> 16) & 0xFF) - rgb[0];
                dg = (WORD)((c >> 8) & 0xFF) - rgb[1];
                db = (WORD)(c & 0xFF) - rgb[2];
                d = (LONG)dr * dr + (LONG)dg * dg + (LONG)db * db;
                if (d < bestd)
                {
                    bestd = d;
                    best = p;
                }
            }
            pixtab[first + i] = best;
        }
    }
}

/*****************************************************************************

    NAME */
#include <graphics/view.h>
#include <proto/graphics.h>

        AROS_LH5(void, SetRGB32,

/*  SYNOPSIS */
        AROS_LHA(struct ViewPort *, vp, A0),
        AROS_LHA(ULONG            , n, D0),
        AROS_LHA(ULONG            , r, D1),
        AROS_LHA(ULONG            , g, D2),
        AROS_LHA(ULONG            , b, D3),

/*  LOCATION */
        struct GfxBase *, GfxBase, 142, Graphics)

/*  FUNCTION
        Changes a single color of a viewport.

    INPUTS
        vp - Modify this viewport
        n - Change this color. If the color is outside the range of
                valid colors, it will be ignored.
        r, g, b - The new values for the red, green and blue. The
                valid range is from 0x000000 (no intensity) to
                0xFFFFFFFF (full intensity).

    RESULT
        If there is a ColorMap for this viewport, then the value will
        be stored in the ColorMap.
        The selected color register is changed to match your specs.
        If the color value is unused then nothing will happen.

    NOTES
        Lower order bits of the palette specification will be discarded,
        depending on the color palette resolution of the target graphics
        device. Use 0xffffffff for the full value, 0x7fffffff for 50%,
        etc. You can find out the palette range for your screen by
        querying the graphics data base.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    UBYTE rgb[3];

    if(vp->ColorMap)
        SetRGB32CM(vp->ColorMap, n, r, g, b);

    rgb[0] = r >> 24;
    rgb[1] = g >> 24;
    rgb[2] = b >> 24;
    internal_SetRGB32Colors(vp, rgb, n, 1, GfxBase);

    AROS_LIBFUNC_EXIT

} /* SetRGB32 */
