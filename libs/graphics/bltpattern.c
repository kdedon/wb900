/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc:
*/

#include <proto/graphics.h>

#include "graphics_intern.h"
#include "gfxfuncsupport.h"

/****************************************************************************************/

/*
 * The template of a mask under the area pattern, pattern rows and bits
 * by RastPort coordinates: set where both are (or the mask is and the
 * pattern is not, for INVERSVID). Only the first plane of a multicolour
 * pattern counts.
 */
static UBYTE *pattern_template(struct RastPort *rp, PLANEPTR mask, WORD byteCnt,
                               WORD xMin, WORD yMin, WORD w, WORD h, ULONG *size,
                               struct GfxBase *GfxBase)
{
    UWORD  rows = 1 << ((rp->AreaPtSz >= 0) ? rp->AreaPtSz : -rp->AreaPtSz);
    UWORD  inv = (rp->DrawMode & INVERSVID) ? 0xFFFF : 0;
    UWORD  bpr = ((w + 15) >> 4) << 1;
    UBYTE *t, *row;
    UWORD  p;
    WORD   x, y;

    *size = (ULONG)bpr * h;
    t = AllocMem(*size, MEMF_CLEAR);
    if(!t)
        return NULL;
    for(y = 0, row = t; y < h; y++, row += bpr, mask += byteCnt) {
        p = rp->AreaPtrn[(yMin + y) & (rows - 1)] ^ inv;
        for(x = 0; x < w; x++) {
            if((mask[x >> 3] & (0x80 >> (x & 7))) &&
                    (p & (0x8000 >> ((xMin + x) & 15))))
                row[x >> 3] |= 0x80 >> (x & 7);
        }
    }
    return t;
}

/*****************************************************************************

    NAME */

        AROS_LH7(void, BltPattern,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort *, rp, A1),
        AROS_LHA(PLANEPTR         , mask, A0),
        AROS_LHA(WORD             , xMin, D0),
        AROS_LHA(WORD             , yMin, D1),
        AROS_LHA(WORD             , xMax, D2),
        AROS_LHA(WORD             , yMax, D3),
        AROS_LHA(WORD             , byteCnt, D4),

/*  LOCATION */
        struct GfxBase *, GfxBase, 52, Graphics)

/*  FUNCTION
        Blit using drawmode, areafill pattern and mask.

    INPUTS
        rp - destination RastPort for blit.
        mask - Mask bitplane. Set this to NULL for a rectangle.
        xMin, yMin - upper left corner.
        xMax, yMax - lower right corner.
        byteCnt - BytesPerRow for mask.

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY
        27-11-96    digulla automatically created from
                            graphics_lib.fd and clib/graphics_protos.h

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    /*
     * COMPLEMENT inverts the destination and uses neither pen, and JAM2 asks
     * for both phases of the pattern to be drawn. Together they invert the
     * whole area, and the pattern has nothing left to say. Under JAM1 only
     * the pattern's set bits are drawn, so only those invert, which the
     * pattern path below already gets right.
     */
    if((rp->DrawMode & (COMPLEMENT | JAM2)) == (COMPLEMENT | JAM2)) {
        if(mask) {
            ULONG old_drawmode = rp->DrawMode;

            /* JAM1 leaves COMPLEMENT to reach the driver, so the template's
             * set bits, and only those, invert. */
            SetDrMd(rp, JAM1 | COMPLEMENT);
            BltTemplate(mask, 0, byteCnt, rp, xMin, yMin,
                        xMax - xMin + 1, yMax - yMin + 1);
            SetDrMd(rp, old_drawmode);
        } else {
            fillrect_pendrmd(rp, xMin, yMin, xMax, yMax, rp->FgPen,
                             COMPLEMENT, GfxBase);
        }

        return;
    }

    if(rp->AreaPtrn) {
        BOOL solidpattern = FALSE;

        /* A solid, non-inverted area pattern plus a mask is a template blit.
         * The native Amiga bitmap class accelerates masked templates with the
         * blitter, while its pattern method cannot accelerate masked input. */
        if(mask && rp->AreaPtSz >= 0 && !(rp->DrawMode & INVERSVID)) {
            UWORD rows = 1U << rp->AreaPtSz;
            UWORD row;

            solidpattern = TRUE;
            for(row = 0; row < rows; row++) {
                if(((UWORD *)rp->AreaPtrn)[row] != 0xffff) {
                    solidpattern = FALSE;
                    break;
                }
            }
        }

        if(solidpattern) {
            ULONG old_drawmode = rp->DrawMode;

            if((old_drawmode & ~INVERSVID) == JAM2)
                SetDrMd(rp, JAM1);

            BltTemplate(mask, 0, byteCnt, rp, xMin, yMin,
                        xMax - xMin + 1, yMax - yMin + 1);
            SetDrMd(rp, old_drawmode);
        } else if(!mask) {
            struct Rectangle        rr;

            FIX_GFXCOORD(xMin);
            FIX_GFXCOORD(yMin);

            rr.MinX = xMin;
            rr.MinY = yMin;
            rr.MaxX = xMax;
            rr.MaxY = yMax;

            do_render_func(rp, NULL, &rr, fillrect_render, NULL, GfxBase);
        } else {
            WORD   w = xMax - xMin + 1, h = yMax - yMin + 1;
            ULONG  old_drawmode = rp->DrawMode;
            UBYTE  old_fgpen = rp->FgPen;
            ULONG  size;
            UBYTE *t;

            if(w <= 0 || h <= 0)
                return;
            t = pattern_template(rp, mask, byteCnt, xMin, yMin, w, h, &size, GfxBase);
            if(!t)
                return;

            /* JAM2 lays BPen under all of the mask first */
            if((old_drawmode & (JAM2 | COMPLEMENT)) == JAM2) {
                SetABPenDrMd(rp, rp->BgPen, rp->BgPen, JAM1);
                BltTemplate(mask, 0, byteCnt, rp, xMin, yMin, w, h);
                SetAPen(rp, old_fgpen);
            }
            SetDrMd(rp, JAM1 | (old_drawmode & COMPLEMENT));
            BltTemplate(t, 0, ((w + 15) >> 4) << 1, rp, xMin, yMin, w, h);
            SetDrMd(rp, old_drawmode);
            FreeMem(t, size);
        }
    } else {
        if(mask) {
            ULONG old_drawmode = rp->DrawMode;

            if((old_drawmode & ~INVERSVID) == JAM2)
                SetDrMd(rp, JAM1 | (old_drawmode & INVERSVID));

            BltTemplate(mask, 0, byteCnt, rp, xMin, yMin, xMax - xMin + 1, yMax - yMin + 1);

            SetDrMd(rp, old_drawmode);
        } else {
            RectFill(rp, xMin, yMin, xMax, yMax);
        }
    }

    AROS_LIBFUNC_EXIT

} /* BltPattern */
