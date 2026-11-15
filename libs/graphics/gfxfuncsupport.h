#ifndef GFXFUNCSUPPORT_H
#define GFXFUNCSUPPORT_H

/****************************************************************************************/

/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

/****************************************************************************************/

#include <graphics/clip.h>
#include <graphics/gfxbase.h>

#include "graphics_intern.h"

#define FIX_GFXCOORD(x) x = (WORD)x

/* The pixel of a pen in a bitmap */
#define BM_PIXEL(bitmap, pen) \
    ((IS_GFXBM(bitmap) && GFXBM(bitmap)->gb_PixTab) ? \
        GFXBM(bitmap)->gb_PixTab[(UBYTE)(pen)] : (UBYTE)(pen))

#define TranslateRect(rect, dx, dy) \
do                                  \
{                                   \
    struct Rectangle *_rect = rect; \
    WORD _dx = dx;                  \
    WORD _dy = dy;                  \
    (_rect)->MinX += (_dx);         \
    (_rect)->MinY += (_dy);         \
    (_rect)->MaxX += (_dx);         \
    (_rect)->MaxY += (_dy);         \
} while(0)

/****************************************************************************************/

/*
 * A piece of a rendering operation: the area rect of bm (clipped, bitmap
 * coordinates) gets what the operation would put at (srcx, srcy) of its
 * own coordinates, with pens already mapped for bm.
 */
typedef void (*RENDERFUNC)(APTR, WORD, WORD, struct BitMap *, struct Rectangle *,
                           struct GfxPens *, struct GfxBase *);
typedef LONG (*PIXELFUNC)(APTR, struct BitMap *, WORD, WORD, struct GfxPens *,
                          struct GfxBase *);

/* The pens, draw mode, mask and area pattern of a RastPort */
void GetRPPens(struct RastPort *rp, struct GfxPens *p);

void do_render_func(struct RastPort *rp, Point *src, struct Rectangle *rr,
                    RENDERFUNC render_func, APTR funcdata, struct GfxBase *GfxBase);

void do_render_with_pens(struct RastPort *rp, Point *src, struct Rectangle *rr,
                         RENDERFUNC render_func, APTR funcdata, struct GfxPens *pens,
                         struct GfxBase *GfxBase);

LONG do_pixel_func(struct RastPort *rp, WORD x, WORD y,
                   PIXELFUNC render_func, APTR funcdata, struct GfxBase *GfxBase);

void fillrect_render(APTR funcdata, WORD srcx, WORD srcy,
                     struct BitMap *bm, struct Rectangle *rect,
                     struct GfxPens *pens, struct GfxBase *GfxBase);

void fillrect_pendrmd(struct RastPort *rp, WORD x1, WORD y1, WORD x2, WORD y2,
                      UBYTE pen, UBYTE drmd, struct GfxBase *GfxBase);

void int_bltbitmap(struct BitMap *srcBitMap, WORD xSrc, WORD ySrc,
                   struct BitMap *dstBitMap, WORD xDest, WORD yDest,
                   WORD xSize, WORD ySize, UBYTE minterm, UBYTE mask,
                   struct GfxBase *GfxBase);

BOOL MoveRaster(struct RastPort *rp, WORD dx, WORD dy, WORD x1, WORD y1,
                WORD x2, WORD y2, BOOL UpdateDamageList, struct GfxBase *GfxBase);

/* ScrollRaster() and ScrollRasterBF() (scrollraster.c) */
void internal_ScrollRaster(struct RastPort *rp, WORD dx, WORD dy, WORD xMin, WORD yMin,
                           WORD xMax, WORD yMax, BOOL backfill, struct GfxBase *GfxBase);

void GetRPClipRectangleForBitMap(struct BitMap *bm, struct Rectangle *r,
                                 struct GfxBase *GfxBase);

/* The render data of BltBitMapRastPort() */
struct bitmap_render_data {
    UBYTE                      minterm;
    struct BitMap             *srcbm;
};

void bitmap_render(APTR bitmap_rd, WORD srcx, WORD srcy, struct BitMap *dstbm,
                   struct Rectangle *rect, struct GfxPens *pens,
                   struct GfxBase *GfxBase);

/****************************************************************************************/

#endif
