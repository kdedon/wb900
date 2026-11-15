/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Some common macros for rectangles and regions.
*/

#ifndef INTREGIONS_H
#define INTREGIONS_H

#include <proto/graphics.h>

#define MinX(rr)   ((rr)->bounds.MinX)
#define MaxX(rr)   ((rr)->bounds.MaxX)
#define MinY(rr)   ((rr)->bounds.MinY)
#define MaxY(rr)   ((rr)->bounds.MaxY)

/* CHECKME: Aren't the following candidates for moving into graphics/gfxmacros.h ? */

#define _DoRectsOverlap(Rect, Rect2)	      \
(                                             \
    (Rect2)->MinY <= (Rect)->MaxY &&          \
    (Rect2)->MaxY >= (Rect)->MinY &&          \
    (Rect2)->MinX <= (Rect)->MaxX &&          \
    (Rect2)->MaxX >= (Rect)->MinX             \
)

#define overlap(a, b) _DoRectsOverlap(&(a), &(b))

#define _TranslateRect(rect, dx, dy) \
do                                   \
{                                    \
    WORD _trdx = (dx), _trdy = (dy); \
    (rect)->MinX += _trdx;           \
    (rect)->MinY += _trdy;           \
    (rect)->MaxX += _trdx;           \
    (rect)->MaxY += _trdy;           \
} while(0)

/* Empty a region: clear its bounds and its rectangle list */
#define InitRegion(region)                \
do                                        \
{                                         \
    ULONG *_ir = (ULONG *)&(region)->bounds; \
    _ir[0] = 0;                           \
    _ir[1] = 0;                           \
    (region)->RegionRectangle = NULL;     \
} while (0)

#endif /* !INTREGIONS_H */
