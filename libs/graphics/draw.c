/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Graphics function Draw()
*/

#include <graphics/rastport.h>
#include <proto/graphics.h>

#include "gfxfuncsupport.h"
#include "graphics_intern.h"
#include "intregions.h"

/****************************************************************************************/

struct draw_render_data {
    WORD x1, y1, x2, y2;
    UWORD ptrn;     /* the line pattern, bit 15 for (x1, y1) */
};

static ULONG draw_render(APTR draw_rd, WORD srcx, WORD srcy,
                         struct BitMap *bm, struct Rectangle *rect,
                         struct GfxPens *pens, struct GfxBase *GfxBase)
{
    struct draw_render_data *drd = draw_rd;

    GFX_DRIVER(bm)->gd_Line(bm, rect, drd->x1 + rect->MinX - srcx,
                            drd->y1 + rect->MinY - srcy,
                            drd->x2 + rect->MinX - srcx,
                            drd->y2 + rect->MinY - srcy,
                            drd->ptrn, pens);

    return 0;
}

/*****************************************************************************

    NAME */

        AROS_LH3(void, Draw,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort *, rp, A1),
        AROS_LHA(WORD             , x, D0),
        AROS_LHA(WORD             , y, D1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 41, Graphics)

/*  FUNCTION
        Draw a line from the current pen position to the given coordinate.

    INPUTS
        rp  - destination RastPort.
        x,y - line end coordinate.

    RESULT

    NOTES
        Move() sets FRST_DOT and this clears it, so a run of Draw() calls
        paints the vertex between two segments once rather than twice. Only
        COMPLEMENT can see the difference - drawing the same pen twice looks
        the same as drawing it once - and there it is the difference between
        a vertex and a hole.

        Not yet implemented:

          - handle layer->Scroll_X/Scroll_Y.

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY
        20-Oct-26 Use the rectangle renderer for solid axis-aligned lines.
        29-10-95    digulla automatically created from
                            graphics_lib.fd and clib/graphics_protos.h

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct Rectangle        rr;
    struct GfxPens          pens;
    struct draw_render_data drd;
    WORD                    dx;
    WORD                    x1, y1;
    UWORD                   linepat;
    UBYTE                   cnt;

    FIX_GFXCOORD(x);
    FIX_GFXCOORD(y);

    x1 = rp->cp_x;
    y1 = rp->cp_y;

    if(x1 > x) {
        rr.MinX = x;
        rr.MaxX = x1;
    } else {
        rr.MinX = x1;
        rr.MaxX = x;
    }

    if(y1 > y) {
        rr.MinY = y;
        rr.MaxY = y1;
    } else {
        rr.MinY = y1;
        rr.MaxY = y;
    }

    GetRPPens(rp, &pens);
    pens.gp_Ptrn = NULL;

    /* Only Draw() uses line pattern attributes, so we set them only here */
    linepat = (rp->DrawMode & INVERSVID) ? ~rp->LinePtrn : rp->LinePtrn;
    pens.gp_DrMd &= ~INVERSVID;
    cnt = rp->linpatcnt & 15;
    drd.ptrn = (UWORD)((linepat << (15 - cnt)) | (linepat >> (cnt + 1)));

    drd.x1 = x1 - rr.MinX;
    drd.y1 = y1 - rr.MinY;
    drd.x2 = x  - rr.MinX;
    drd.y2 = y  - rr.MinY;

    /* A solid horizontal or vertical line is a one-pixel-wide rectangle.
     * Let the bitmap driver fill it in bulk instead of dispatching a pixel
     * operation for every point. Keep patterned and diagonal lines on the
     * line renderer, including its pattern direction and clipping rules. */
    if (linepat == 0xffff && (x1 == x || y1 == y))
        do_render_with_pens(rp, NULL, &rr, fillrect_render, NULL, &pens, GfxBase);
    else
        do_render_with_pens(rp, NULL, &rr, draw_render, &drd, &pens, GfxBase);

    /*
     * FRST_DOT clear means the starting point belongs to the segment before
     * this one, which has already drawn it. Complementing it a second time
     * would take it back to the background, so undo that dot - but only if
     * the line pattern actually laid it down.
     */
    if(!(rp->Flags & FRST_DOT)
            && (rp->DrawMode & COMPLEMENT)
            && (linepat & (1 << cnt))) {
        fillrect_pendrmd(rp, x1, y1, x1, y1, rp->FgPen, COMPLEMENT, GfxBase);
    }

    /* Whatever comes next continues from this segment's end point. */
    rp->Flags &= ~FRST_DOT;

    /* The pattern moves on by the steps of the major axis, whichever way
       the line goes (AROS took the end point's offset from the corner,
       which is 0 for a line drawn left or up). */
    dx = x > x1 ? x - x1 : x1 - x;
    if(dx < (y > y1 ? y - y1 : y1 - y))
        dx = y > y1 ? y - y1 : y1 - y;

    rp->linpatcnt = ((LONG)rp->linpatcnt - dx) & 15;

    rp->cp_x = x;
    rp->cp_y = y;

    AROS_LIBFUNC_EXIT

} /* Draw */
