/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <proto/graphics.h>
#include <proto/layers.h>
#include <graphics/rpattr.h>
#include "intuition_intern.h"

/*****************************************************************************

    NAME */
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <proto/intuition.h>

        AROS_LH4(void, DrawBorder,

/*  SYNOPSIS */
        AROS_LHA(struct RastPort *, rp, A0),
        AROS_LHA(struct Border   *, border, A1),
        AROS_LHA(WORD             , leftOffset, D0),
        AROS_LHA(WORD             , topOffset, D1),

/*  LOCATION */
        struct IntuitionBase *, IntuitionBase, 18, Intuition)

/*  FUNCTION
        Draws one or more borders in the specified RastPort. Rendering
        will start at the position which you get when you add the offsets
        leftOffset and topOffset to the LeftEdge and TopEdge specified
        in the Border structure. All coordinates are relative to that point.

    INPUTS
        rp - The RastPort to render into
        border - Information what and how to render
        leftOffset, topOffset - Initial starting position

    RESULT
        None.

    NOTES

    EXAMPLE
        // Draw a house with one stroke
        // The drawing starts at the lower left edge
        WORD XY[] =
        {
            10, -10,
            10,   0,
             0, -10,
            10, -10,
             5, -15,
             0, -10,
             0,   0,
            10,   0,
        };
        struct Border demo =
        {
            100, 100,   // Position
            1, 2,   // Pens
            JAM1,   // Drawmode
            8,      // Number of pairs in XY
            XY,     // Vector offsets
            NULL    // No next border
        };

        // Render the house with the bottom left edge at 150, 50
        DrawBorder (rp, &demo, 50, -50);

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************/
{
    AROS_LIBFUNC_INIT

    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct LayersBase *LayersBase = GetPrivIBase(IntuitionBase)->LayersBase;
    IPTR   apen;
    IPTR   bpen;
    IPTR   drmd;
    WORD  *ptr;
    WORD   x, y;
    WORD   xoff, yoff;
    int    t;

    SANITY_CHECK(rp)
    SANITY_CHECK(border)

    if (rp->Layer) LockLayer(0,rp->Layer);

    /* Store important variables of the RastPort */
    {
        struct TagItem rptags[4];

        rptags[0].ti_Tag = RPTAG_APen; rptags[0].ti_Data = (IPTR)&apen;
        rptags[1].ti_Tag = RPTAG_BPen; rptags[1].ti_Data = (IPTR)&bpen;
        rptags[2].ti_Tag = RPTAG_DrMd; rptags[2].ti_Data = (IPTR)&drmd;
        rptags[3].ti_Tag = TAG_DONE;
        GetRPAttrsA(rp, rptags);
    }

    /* For all borders... */
    for ( ; border; border = border->NextBorder)
    {
        /* Change RastPort to the colors/mode specified */
        SetAPen (rp, border->FrontPen);
        SetBPen (rp, border->BackPen);
        SetDrMd (rp, border->DrawMode);

        /* Get base coords */

        x = border->LeftEdge + leftOffset;
        y = border->TopEdge + topOffset;

        /* Start of vector offsets */
        ptr = border->XY;

        for (t = 0; t < border->Count; t++)
        {
            /* Add vector offset to current position */
            xoff = *ptr ++;
            yoff = *ptr ++;

            if (t == 0)
            {
                Move (rp, x + xoff, y + yoff);
            }
            else
            {
                /* Stroke */
                Draw (rp, x + xoff, y + yoff);
            }
        }

    } /* for ( ; border; border = border->NextBorder) */

    /* Restore RastPort */
    {
        struct TagItem rptags[4];

        rptags[0].ti_Tag = RPTAG_APen; rptags[0].ti_Data = apen;
        rptags[1].ti_Tag = RPTAG_BPen; rptags[1].ti_Data = bpen;
        rptags[2].ti_Tag = RPTAG_DrMd; rptags[2].ti_Data = drmd;
        rptags[3].ti_Tag = TAG_DONE;
        SetRPAttrsA(rp, rptags);
    }

    if (rp->Layer) UnlockLayer(rp->Layer);

    AROS_LIBFUNC_EXIT
} /* DrawBorder */
