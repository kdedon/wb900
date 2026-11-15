/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Copy a rectangle in a bitmap to another place or another bitmap.
*/

#include <exec/memory.h>
#include <graphics/gfx.h>
#include <proto/exec.h>


#include "graphics_intern.h"
#include "gfxfuncsupport.h"

/* The selected writable planes of a classic bitmap, up to the common
   source/destination depth. NULL and -1 destination planes are constants. */
static LONG count_planar_planes(const struct BitMap *bitmap, ULONG depth, UBYTE mask)
{
    LONG count = 0;
    ULONG plane;

    for (plane = 0; plane < depth && plane < 8; plane++) {
        if ((mask & (1U << plane)) && bitmap->Planes[plane] != NULL &&
                bitmap->Planes[plane] != (PLANEPTR)-1)
            count++;
    }
    return count;
}

/*****************************************************************************

    NAME */
#include <graphics/gfx.h>
#include <proto/graphics.h>

        AROS_LH11(LONG, BltBitMap,

/*  SYNOPSIS */
        AROS_LHA(struct BitMap *, srcBitMap, A0),
        AROS_LHA(WORD           , xSrc, D0),
        AROS_LHA(WORD           , ySrc, D1),
        AROS_LHA(struct BitMap *, destBitMap, A1),
        AROS_LHA(WORD           , xDest, D2),
        AROS_LHA(WORD           , yDest, D3),
        AROS_LHA(WORD           , xSize, D4),
        AROS_LHA(WORD           , ySize, D5),
        AROS_LHA(UBYTE          , minterm, D6),
        AROS_LHA(UBYTE          , mask, D7),
        AROS_LHA(PLANEPTR       , tempA, A2),

/*  LOCATION */
        struct GfxBase *, GfxBase, 5, Graphics)

/*  FUNCTION
        Moves a part of a bitmap around or into another bitmap.

    INPUTS
        srcBitMap - Copy from this bitmap.
        xSrc, ySrc - This is the upper left corner of the area to copy.
        destBitMap - Copy to this bitmap. May be the same as srcBitMap.
        xDest, yDest - Upper left corner where to place the copy
        xSize, ySize - The size of the area to copy
        minterm - How to copy. Most useful values are 0x00C0 for a vanilla
                copy, 0x0030 to invert the source and then copy or 0x0050
                to ignore the source and just invert the destination. If
                you want to calculate other values, then you must know that
                channel A is set, if you are inside the rectangle, channel
                B is the source and channel C is the destination of the
                rectangle.

                Bit     ABC
                 0      000
                 1      001
                 2      010
                 3      011
                 4      100
                 5      101
                 6      110
                 7      111

                So 0x00C0 means: D is set if one is inside the rectangle
                (A is set) and B (the source) is set and cleared otherwise.

                To fill the rectangle, you would want to set D when A is
                set, so the value is 0x00F0.

        mask - Which planes should be copied. Typically, you should set
                this to ~0L.
        tempA - If the copy overlaps exactly to the left or right (i.e. the
                scan line addresses overlap), and tempA is non-zero, it
                points to enough chip accessible memory to hold a line of a
                source for the blit (i.e. CHIP RAM). BltBitMap will allocate
                (and free) the needed TempA if none is provided and one is
                needed.  Blit overlap is determined from the relation of
                the first non-masked planes in the source and destination
                bit maps.

    RESULT
        The number of planes actually involved in the blit.

    NOTES
        If a special hardware is available, this function will use it.

        As a special case, plane pointers of destBitMap can contain NULL
        or -1, which will act as if the plane was filled with 0's or 1's,
        respectively.

    EXAMPLE

    BUGS

    SEE ALSO
        ClipBlit(), BltBitMapRastPort()

    INTERNALS

    HISTORY
        14-Nov-26 Through the bitmaps' driver tables; a chunky destination
                  counts as one plane per bit of its pixels.

*****************************************************************************/
{
    AROS_LIBFUNC_INIT
    LONG planecnt;
    ULONG wSrc, wDest;
    ULONG x;
    ULONG depth;

    FIX_GFXCOORD(xSrc);
    FIX_GFXCOORD(ySrc);
    FIX_GFXCOORD(xDest);
    FIX_GFXCOORD(yDest);

    wSrc  = GetBitMapAttr(srcBitMap, BMA_WIDTH);
    wDest = GetBitMapAttr(destBitMap, BMA_WIDTH);

    /* Clip all blits */

    depth = GetBitMapAttr(srcBitMap, BMA_DEPTH);
    x     = GetBitMapAttr(destBitMap, BMA_DEPTH);

    if(x < depth) depth = x;

    /* Clip X and Y */
    if(xSrc < 0) {
        xDest += -xSrc;
        xSize -= -xSrc;
        xSrc = 0;
    }

    if(ySrc < 0) {
        yDest += -ySrc;
        ySize -= -ySrc;
        ySrc = 0;
    }

    /* A negative destination is not clipped by the checks below (unsigned
       compares) and the chunky blitters do not clip: rows/columns would land
       in front of the destination buffer. */
    if(xDest < 0) {
        xSrc += -xDest;
        xSize -= -xDest;
        xDest = 0;
    }

    if(yDest < 0) {
        ySrc += -yDest;
        ySize -= -yDest;
        yDest = 0;
    }

    /* Clip width and height for source and dest */
    if(ySrc + ySize > srcBitMap->Rows) {
        ySize = srcBitMap->Rows - ySrc;
    }

    if(yDest + ySize > destBitMap->Rows) {
        ySize = destBitMap->Rows - yDest;
    }

    if((ULONG)(xSrc + xSize) >= wSrc) {
        xSize = wSrc - xSrc;
    }

    if((ULONG)(xDest + xSize) >= wDest) {
        xSize = wDest - xDest;
    }

    /* If the size is illegal or we need not copy anything, return */
    if(ySize <= 0 || xSize <= 0 || !mask) return 0;

    if(IS_GFXBM(destBitMap) && GFXBM(destBitMap)->gb_Driver->gd_Format != GDF_PLANAR)
        planecnt = depth;
    else
        planecnt = count_planar_planes(destBitMap, depth, mask);
    if(!planecnt)
        return 0;

    int_bltbitmap(srcBitMap, xSrc, ySrc, destBitMap, xDest, yDest,
                  xSize, ySize, minterm, mask, GfxBase);

    return planecnt;

    AROS_LIBFUNC_EXIT

} /* BltBitMap */

