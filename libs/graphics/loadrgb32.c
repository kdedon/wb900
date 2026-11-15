/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc:
*/

#include "graphics_intern.h"

/*****************************************************************************

    NAME */
#include <proto/graphics.h>

        AROS_LH2(void, LoadRGB32,

/*  SYNOPSIS */
        AROS_LHA(struct ViewPort *, vp, A0),
        AROS_LHA(const ULONG     *, table, A1),

/*  LOCATION */
        struct GfxBase *, GfxBase, 147, Graphics)

/*  FUNCTION
        Load RGB color values from table.

    INPUTS
        vp    - ViewPort
        table - pointer to table of records
                1 Word with the number of colors to load
                1 Word with the first color to be loaded.
                3 Longwords representing a left justified 32 bit RGB triplet.
                The list is terminated by a count value of 0.
    RESULT

    NOTES

    EXAMPLE
        ULONG table[] = { 1l << 16 + 0 , 0xffffffff , 0 , 0 , 0}
        ULONG table[] = { 256l << 16 + 0 , r1 , g1 , b1 , r2 , g2 , b2 , ..... 0}

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY
        27-11-96    digulla automatically created from
                            graphics_lib.fd and clib/graphics_protos.h

*****************************************************************************/
{
    AROS_LIBFUNC_INIT


    /* it is legal to pass a NULL table */

    if(table) {
        ULONG count;

        /* table is terminated by a count value of 0 */
        while((count = (*table) >> 16)) {
            ULONG first, t;
            UBYTE rgb[3];

            first = (*table) & 0xFFFF;
            table ++;

            for(t = 0; t < count; t++) {
                if (vp->ColorMap)
                    SetRGB32CM(vp->ColorMap, t + first,
                               table[0], table[1], table[2]);
                rgb[0] = table[0] >> 24;
                rgb[1] = table[1] >> 24;
                rgb[2] = table[2] >> 24;
                internal_SetRGB32Colors(vp, rgb, t + first, 1, GfxBase);
                table += 3;
            }
        } /* while (*table) */
    }

    AROS_LIBFUNC_EXIT

} /* LoadRGB32 */
