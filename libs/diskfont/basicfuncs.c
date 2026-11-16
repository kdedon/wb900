/*
    Copyright (C) 1995-2004, The AROS Development Team. All rights reserved.

    Some support functions
*/

/****************************************************************************************/

#include "diskfont_intern.h"

/****************************************************************************************/

/*
 * A block laid out as LoadSeg() lays out a hunk, its size and its link
 * first, so UnLoadSeg() frees it; linked after prevsegment's unless that
 * is NULL.
 */
APTR AllocSegment
(
    BPTR *prevsegment, ULONG segmentsize, ULONG memflags,
    struct DiskfontBase *DiskfontBase
)
{
    ULONG *mem;
    
    if ((mem = AllocMem(segmentsize + sizeof(ULONG) + sizeof(BPTR), memflags)))
    {
        BPTR *membptr;

        *mem++ = segmentsize + sizeof(ULONG) + sizeof(BPTR);
        if (prevsegment) prevsegment[-1] = MKBADDR(mem);
        membptr = (BPTR *) mem;
        *membptr++ = NULL;
        mem = (ULONG *)membptr;
    }
    
    return mem;
}

/****************************************************************************************/
