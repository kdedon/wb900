/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

/****************************************************************************************/

#include <proto/exec.h>
#include <proto/graphics.h>

#include "graphics_intern.h"
#include "intregions.h"
#include "gfxfuncsupport.h"

/****************************************************************************************/

void GetRPPens(struct RastPort *rp, struct GfxPens *p)
{
    WORD rows = rp->AreaPtSz < 0 ? -rp->AreaPtSz : rp->AreaPtSz;

    p->gp_APen = rp->FgPen;
    p->gp_BPen = rp->BgPen;
    p->gp_DrMd = rp->DrawMode;
    p->gp_Mask = rp->Mask;
    p->gp_Ptrn = rp->AreaPtrn;
    p->gp_PtrnMask = (1 << rows) - 1;
}

/* What stays the same for every piece of one render operation */
struct render_call {
    RENDERFUNC       render_func;
    APTR             funcdata;
    struct GfxPens  *pens;
    struct GfxBase  *GfxBase;
};

static ULONG CallRenderFunc(struct render_call *rc, WORD srcx, WORD srcy,
                            struct BitMap *bm, struct Rectangle *rect)
{
    struct GfxPens pens;

    pens = *rc->pens;
    pens.gp_APen = BM_PIXEL(bm, pens.gp_APen);
    pens.gp_BPen = BM_PIXEL(bm, pens.gp_BPen);

    return rc->render_func(rc->funcdata, srcx, srcy, bm, rect, &pens, rc->GfxBase);
}

ULONG do_render_func(struct RastPort *rp, Point *src, struct Rectangle *rr,
                     RENDERFUNC render_func, APTR funcdata, struct GfxBase *GfxBase)
{
    struct GfxPens pens;

    GetRPPens(rp, &pens);

    return do_render_with_pens(rp, src, rr, render_func, funcdata, &pens, GfxBase);
}

/*
 * Documented AmigaOS behaviour for superbitmap layers: rendering that
 * falls outside the layer goes into the superbitmap. 'torender' is the
 * area in screen coordinates; the parts of it outside the layer's bounds
 * are rendered at superbitmap coordinates (rastport coordinates).
 */
static ULONG render_outside_super(struct Layer *L, struct Rectangle *torender,
                                  struct render_call *rc, WORD srcx, WORD srcy)
{
    struct GfxBase *GfxBase = rc->GfxBase;
    struct Rectangle parts[4];
    ULONG pixwritten = 0, n = 0, i;
    LONG  sbw = GetBitMapAttr(L->SuperBitMap, BMA_WIDTH);
    LONG  sbh = GetBitMapAttr(L->SuperBitMap, BMA_HEIGHT);

    /* the strips above and below the layer, then those left and right of it
       (between the layer's rows); each clipped to the area itself */
    if(torender->MinY < L->bounds.MinY) {
        parts[n] = *torender;
        if(parts[n].MaxY > L->bounds.MinY - 1) parts[n].MaxY = L->bounds.MinY - 1;
        n++;
    }
    if(torender->MaxY > L->bounds.MaxY) {
        parts[n] = *torender;
        if(parts[n].MinY < L->bounds.MaxY + 1) parts[n].MinY = L->bounds.MaxY + 1;
        n++;
    }
    if(torender->MinX < L->bounds.MinX) {
        parts[n] = *torender;
        if(parts[n].MaxX > L->bounds.MinX - 1) parts[n].MaxX = L->bounds.MinX - 1;
        if(parts[n].MinY < L->bounds.MinY) parts[n].MinY = L->bounds.MinY;
        if(parts[n].MaxY > L->bounds.MaxY) parts[n].MaxY = L->bounds.MaxY;
        n++;
    }
    if(torender->MaxX > L->bounds.MaxX) {
        parts[n] = *torender;
        if(parts[n].MinX < L->bounds.MaxX + 1) parts[n].MinX = L->bounds.MaxX + 1;
        if(parts[n].MinY < L->bounds.MinY) parts[n].MinY = L->bounds.MinY;
        if(parts[n].MaxY > L->bounds.MaxY) parts[n].MaxY = L->bounds.MaxY;
        n++;
    }

    for(i = 0; i < n; i++) {
        struct Rectangle part = parts[i], sb;
        WORD xoffset, yoffset;

        if(part.MinX > part.MaxX || part.MinY > part.MaxY)
            continue;

        /* to superbitmap coordinates, clipped to the superbitmap */
        sb.MinX = part.MinX - L->bounds.MinX + L->Scroll_X;
        sb.MinY = part.MinY - L->bounds.MinY + L->Scroll_Y;
        sb.MaxX = part.MaxX - L->bounds.MinX + L->Scroll_X;
        sb.MaxY = part.MaxY - L->bounds.MinY + L->Scroll_Y;
        if(sb.MinX < 0) sb.MinX = 0;
        if(sb.MinY < 0) sb.MinY = 0;
        if(sb.MaxX > sbw - 1) sb.MaxX = sbw - 1;
        if(sb.MaxY > sbh - 1) sb.MaxY = sbh - 1;
        if(sb.MinX > sb.MaxX || sb.MinY > sb.MaxY)
            continue;

        /* source offset of the clipped part within the whole area */
        xoffset = (sb.MinX - L->Scroll_X + L->bounds.MinX) - torender->MinX;
        yoffset = (sb.MinY - L->Scroll_Y + L->bounds.MinY) - torender->MinY;

        pixwritten += CallRenderFunc(rc, srcx + xoffset, srcy + yoffset, L->SuperBitMap, &sb);
    }

    return pixwritten;
}

ULONG do_render_with_pens(struct RastPort *rp, Point *src, struct Rectangle *rr,
                          RENDERFUNC render_func, APTR funcdata, struct GfxPens *pens,
                          struct GfxBase *GfxBase)
{

    struct BitMap       *bm = rp->BitMap;
    struct Layer        *L = rp->Layer;
    WORD                 srcx, srcy;
    LONG                 pixwritten = 0;
    struct render_call   rc;
    BOOL                 sprite;

    if((rr->MaxX < rr->MinX) || (rr->MaxY < rr->MinY)) return 0;

    rc.render_func      = render_func;
    rc.funcdata         = funcdata;
    rc.pens             = pens;
    rc.GfxBase          = GfxBase;

    if(NULL != src) {
        srcx = src->x;
        srcy = src->y;
    } else {
        srcx = 0;
        srcy = 0;
    }

    if(NULL == L) {
        /* No layer, probably a screen, but may be a user inited bitmap */
        struct Rectangle torender = *rr;
        struct Rectangle bm_rect;

        GetRPClipRectangleForBitMap(bm, &bm_rect, GfxBase);
        if(!(_AndRectRect(rr, &bm_rect, &torender))) {
            return 0;
        }

        srcx += (torender.MinX - rr->MinX);
        srcy += (torender.MinY - rr->MinY);

        sprite = gfx_SpriteBefore(bm, &torender, GfxBase);
        pixwritten = CallRenderFunc(&rc, srcx, srcy, bm, &torender);
        if(sprite)
            gfx_SpriteAfter(GfxBase);
    } else {
        struct ClipRect *CR;
        WORD xrel;
        WORD yrel;
        struct Rectangle torender, intersect;

        LockLayerRom(L);
        /* the RastPort's BitMap is read under the layer lock, nothing may change it now */
        bm = rp->BitMap;

        xrel = L->bounds.MinX;
        yrel = L->bounds.MinY;

        torender.MinX = rr->MinX + xrel - L->Scroll_X;
        torender.MinY = rr->MinY + yrel - L->Scroll_Y;
        torender.MaxX = rr->MaxX + xrel - L->Scroll_X;
        torender.MaxY = rr->MaxY + yrel - L->Scroll_Y;

        /* the pointer is locked, and taken out if it is in the way, once for all cliprects */
        sprite = _AndRectRect(&torender, &L->bounds, &intersect) &&
                 gfx_SpriteBefore(bm, &intersect, GfxBase);

        CR = L->ClipRect;

        for(; NULL != CR; CR = CR->Next) {
            /* Does this cliprect intersect with area to rectfill ? */
            if(_AndRectRect(&CR->bounds, &torender, &intersect)) {
                WORD xoffset, yoffset;

                xoffset = intersect.MinX - torender.MinX;
                yoffset = intersect.MinY - torender.MinY;

                if(NULL == CR->lobs) {
                    pixwritten += CallRenderFunc(&rc, srcx + xoffset, srcy + yoffset, bm, &intersect);
                } else {
                    /* Render into offscreen cliprect bitmap */
                    if(L->Flags & LAYERSIMPLE)
                        continue;
                    else if(L->Flags & LAYERSUPER) {
                        /* Render into the superbitmap. RastPort coordinates are
                           superbitmap coordinates; the layer shows the superbitmap
                           from (Scroll_X, Scroll_Y). */
                        intersect.MinX = intersect.MinX - L->bounds.MinX + L->Scroll_X;
                        intersect.MinY = intersect.MinY - L->bounds.MinY + L->Scroll_Y;
                        intersect.MaxX = intersect.MaxX - L->bounds.MinX + L->Scroll_X;
                        intersect.MaxY = intersect.MaxY - L->bounds.MinY + L->Scroll_Y;
                        pixwritten += CallRenderFunc(&rc, srcx + xoffset, srcy + yoffset, L->SuperBitMap, &intersect);
                    } else {

                        intersect.MinX = intersect.MinX - CR->bounds.MinX + CR_BMOFFX(CR);
                        intersect.MinY = intersect.MinY - CR->bounds.MinY + CR_BMOFFY(CR);
                        intersect.MaxX = intersect.MaxX - CR->bounds.MinX + CR_BMOFFX(CR);
                        intersect.MaxY = intersect.MaxY - CR->bounds.MinY + CR_BMOFFY(CR);

                        pixwritten += CallRenderFunc(&rc, srcx + xoffset, srcy + yoffset, CR->BitMap, &intersect);
                    }

                } /* if (CR->lobs == NULL) */

            } /* if (cliprect intersects with area to render into) */

        } /* for (each cliprect in the layer) */

        if(L->Flags & LAYERSUPER) {
            pixwritten += render_outside_super(L, &torender, &rc, srcx, srcy);
        }

        if(sprite)
            gfx_SpriteAfter(GfxBase);
        UnlockLayerRom(L);
    } /* if (rp->Layer) */

    return pixwritten;
}

/****************************************************************************************/

static LONG CallPixelFunc(PIXELFUNC render_func, APTR funcdata, struct BitMap *bm,
                          struct GfxPens *rppens, WORD x, WORD y, struct GfxBase *GfxBase)
{
    struct GfxPens pens;
    struct Rectangle r;
    BOOL sprite;
    LONG retval;

    pens = *rppens;
    pens.gp_APen = BM_PIXEL(bm, pens.gp_APen);
    pens.gp_BPen = BM_PIXEL(bm, pens.gp_BPen);

    r.MinX = r.MaxX = x;
    r.MinY = r.MaxY = y;
    sprite = gfx_SpriteBefore(bm, &r, GfxBase);
    retval = render_func(funcdata, bm, x, y, &pens, GfxBase);
    if(sprite)
        gfx_SpriteAfter(GfxBase);

    return retval;
}

LONG do_pixel_func(struct RastPort *rp
                   , WORD x, WORD y
                   , PIXELFUNC render_func
                   , APTR funcdata
                   , struct GfxBase *GfxBase)
{
    struct BitMap       *bm = rp->BitMap;
    struct Layer        *L = rp->Layer;
    struct GfxPens       pens;
    struct Rectangle     bm_rect;
    LONG                 retval = -1;

    GetRPPens(rp, &pens);

    if(NULL == L) {
        GetRPClipRectangleForBitMap(bm, &bm_rect, GfxBase);
        if(!_IsPointInRect(&bm_rect, x, y)) {
            return -1;
        }

        /* This is a screen */
        retval = CallPixelFunc(render_func, funcdata, bm, &pens, x, y, GfxBase);
    } else {
        struct ClipRect *CR;
        WORD absx, absy;
        BOOL found = FALSE;

        LockLayerRom(L);
        bm = rp->BitMap;

        CR = L->ClipRect;

        absx = x + L->bounds.MinX - L->Scroll_X;
        absy = y + L->bounds.MinY - L->Scroll_Y;

        for(; NULL != CR; CR = CR->Next) {

            if(absx >= CR->bounds.MinX
                    && absy >= CR->bounds.MinY
                    && absx <= CR->bounds.MaxX
                    && absy <= CR->bounds.MaxY) {

                if(NULL == CR->lobs) {
                    retval = CallPixelFunc(render_func, funcdata, bm, &pens,
                                           absx, absy, GfxBase);
                } else {
                    /* This is the tricky one: render into offscreen cliprect bitmap */
                    if(L->Flags & LAYERSIMPLE) {
                        /* We cannot do anything */
                        retval =  0;

                    } else if(L->Flags & LAYERSUPER) {
                        /* Into the superbitmap, at rastport coordinates */
                        retval = CallPixelFunc(render_func, funcdata, L->SuperBitMap, &pens,
                                               absx - L->bounds.MinX + L->Scroll_X,
                                               absy - L->bounds.MinY + L->Scroll_Y,
                                               GfxBase);
                    } else {
                        retval = CallPixelFunc(render_func, funcdata, CR->BitMap, &pens,
                                               absx - CR->bounds.MinX + CR_BMOFFX(CR),
                                               absy - CR->bounds.MinY + CR_BMOFFY(CR),
                                               GfxBase);
                    } /* If (SMARTREFRESH cliprect) */

                }   /* if (intersecton inside hidden cliprect) */

                /* The pixel was found and put inside one of the cliprects, just exit */
                found = TRUE;
                break;

            } /* if (cliprect intersects with area we want to draw to) */

        } /* while (cliprects to examine) */

        if(!found && (L->Flags & LAYERSUPER) &&
           (absx < L->bounds.MinX || absx > L->bounds.MaxX || absy < L->bounds.MinY || absy > L->bounds.MaxY)) {
            /* Outside the layer: documented AmigaOS behaviour is that this
               goes into the superbitmap (at rastport coordinates) */
            LONG sx = absx - L->bounds.MinX + L->Scroll_X;
            LONG sy = absy - L->bounds.MinY + L->Scroll_Y;

            if(sx >= 0 && sy >= 0 &&
               sx < GetBitMapAttr(L->SuperBitMap, BMA_WIDTH) &&
               sy < GetBitMapAttr(L->SuperBitMap, BMA_HEIGHT)) {
                retval = CallPixelFunc(render_func, funcdata, L->SuperBitMap, &pens, sx, sy, GfxBase);
            }
        }

        UnlockLayerRom(L);
    }

    return retval;
}

/****************************************************************************************/

ULONG fillrect_render(APTR funcdata, WORD srcx, WORD srcy,
                      struct BitMap *bm, struct Rectangle *rect,
                      struct GfxPens *pens, struct GfxBase *GfxBase)
{
    GFX_DRIVER(bm)->gd_Fill(bm, rect, pens);

    return (rect->MaxX - rect->MinX + 1) * (rect->MaxY - rect->MinY + 1);
}

/****************************************************************************************/

LONG fillrect_pendrmd(struct RastPort *rp, WORD x1, WORD y1, WORD x2, WORD y2,
                      UBYTE pen, UBYTE drmd, struct GfxBase *GfxBase)
{
    struct GfxPens   pens;
    struct Rectangle rr;

    GetRPPens(rp, &pens);
    pens.gp_APen = pen;
    pens.gp_DrMd = drmd;
    pens.gp_Ptrn = NULL;

    rr.MinX = x1;
    rr.MinY = y1;
    rr.MaxX = x2;
    rr.MaxY = y2;

    return do_render_with_pens(rp, NULL, &rr, fillrect_render, NULL, &pens, GfxBase);
}

/****************************************************************************************/

/* Pixels a cross-format blit converts at a time */
#define BLTCHUNK 64

/*
 * The minterm on pixels for mt on pens, when each pen's pixel is the pen
 * with all or none of the depth bits flipped; else FALSE.
 */
static BOOL pixminterm(UBYTE mt, const UBYTE *pix, UWORD depth, UBYTE *pmt)
{
    UWORD all = (1 << depth) - 1, k = pix[0] & all, i;

    if(k != 0 && k != all)
        return FALSE;
    for(i = 1; i <= all; i++)
        if((pix[i] & all) != (i ^ k))
            return FALSE;
    /* flipped: (b, c) takes mt's bit for (~b, ~c), inverted */
    *pmt = k ? ((((mt & 0x10) ? 0 : 0x80) | ((mt & 0x20) ? 0 : 0x40) |
                 ((mt & 0x40) ? 0 : 0x20) | ((mt & 0x80) ? 0 : 0x10)) |
                (mt & 0x0F)) : mt;
    return TRUE;
}

/*
 * A copy between bitmaps of two formats, or through a pixel table that
 * the minterm cannot go through, goes over pens, a chunk of a row at a
 * time: each pixel to the pen it stands for, the minterm on the pens,
 * and the result to the destination's pixel for it.  Rows and chunks go
 * in the order that keeps an overlap in one bitmap safe.
 */
void int_bltbitmap(struct BitMap *srcBitMap, WORD xSrc, WORD ySrc,
                   struct BitMap *dstBitMap, WORD xDest, WORD yDest,
                   WORD xSize, WORD ySize, UBYTE minterm, UBYTE mask,
                   struct GfxBase *GfxBase)
{
    struct GfxDriver *sd = GFX_DRIVER(srcBitMap);
    struct GfxDriver *dd = GFX_DRIVER(dstBitMap);
    ULONG srcdepth = GetBitMapAttr(srcBitMap, BMA_DEPTH);
    UBYTE *spix = IS_GFXBM(srcBitMap) ? GFXBM(srcBitMap)->gb_PixTab : NULL;
    UBYTE *dpix = IS_GFXBM(dstBitMap) ? GFXBM(dstBitMap)->gb_PixTab : NULL;
    UBYTE sbuf[BLTCHUNK], dbuf[BLTCHUNK], spen[16], dpen[16];
    struct Rectangle r, sr;
    BOOL ssprite, dsprite, up, left;
    WORD x, y, n, i, yy, xx;
    UBYTE pmt;

    /*
     * A blit covers min(src, dst) planes, so a source shallower than the
     * destination leaves the planes above its own depth alone.
     */
    if(srcdepth < GetBitMapAttr(dstBitMap, BMA_DEPTH))
        mask &= (1 << srcdepth) - 1;

    /* The pointer leaves both areas while they are read and written */
    sr.MinX = xSrc;
    sr.MinY = ySrc;
    sr.MaxX = xSrc + xSize - 1;
    sr.MaxY = ySrc + ySize - 1;
    r.MinX = xDest;
    r.MinY = yDest;
    r.MaxX = xDest + xSize - 1;
    r.MaxY = yDest + ySize - 1;
    ssprite = gfx_SpriteBefore(srcBitMap, &sr, GfxBase);
    dsprite = gfx_SpriteBefore(dstBitMap, &r, GfxBase);

    /* A minterm of neither source nor destination is pen 0 or all 1s */
    if((minterm & 0xF0) == 0x00 || (minterm & 0xF0) == 0xF0) {
        struct GfxPens p;

        p.gp_APen = dpix ? dpix[minterm & 0x80 ? 0xFF : 0] : (minterm & 0x80 ? 0xFF : 0);
        p.gp_BPen = p.gp_APen;
        p.gp_DrMd = JAM2;
        p.gp_Mask = mask;
        p.gp_Ptrn = NULL;
        p.gp_PtrnMask = 0;
        dd->gd_Fill(dstBitMap, &r, &p);
        goto done;
    }

    /* Through a pixel table the minterm goes to one on pixels */
    if(sd->gd_Format == dd->gd_Format && spix == dpix &&
       (!dpix || minterm == 0xC0 ||
        pixminterm(minterm, dpix, GetBitMapAttr(dstBitMap, BMA_DEPTH), &pmt))) {
        if(dpix && minterm != 0xC0)
            minterm = pmt;
        dd->gd_Copy(srcBitMap, xSrc, ySrc, dstBitMap, &r, minterm, mask);
        goto done;
    }

    /* pixel -> pen of a 16 colour pixel table */
    for(i = 0; i < 16; i++)
        spen[i] = dpen[i] = i;
    for(i = 15; spix && i >= 0; i--)
        spen[spix[i] & 15] = i;
    for(i = 15; dpix && i >= 0; i--)
        dpen[dpix[i] & 15] = i;

    up = srcBitMap == dstBitMap && yDest > ySrc;
    left = srcBitMap == dstBitMap && yDest == ySrc && xDest > xSrc;
    for(yy = 0; yy < ySize; yy++) {
        y = up ? ySize - 1 - yy : yy;
        for(xx = 0; xx < xSize; xx += n) {
            n = xSize - xx < BLTCHUNK ? xSize - xx : BLTCHUNK;
            x = left ? xSize - xx - n : xx;
            sd->gd_ReadRow(srcBitMap, xSrc + x, ySrc + y, n, sbuf);
            dd->gd_ReadRow(dstBitMap, xDest + x, yDest + y, n, dbuf);
            for(i = 0; i < n; i++) {
                UBYTE s = spix ? spen[sbuf[i] & 15] : sbuf[i];
                UBYTE d = dpix ? dpen[dbuf[i] & 15] : dbuf[i];
                UBYTE res = MINTERM(minterm, s, d);

                dbuf[i] = dpix ? dpix[res] : res;
            }
            dd->gd_WriteRow(dstBitMap, xDest + x, yDest + y, n, dbuf, mask);
        }
    }

done:
    if(dsprite)
        gfx_SpriteAfter(GfxBase);
    if(ssprite)
        gfx_SpriteAfter(GfxBase);
}

/****************************************************************************************/

BOOL MoveRaster(struct RastPort *rp, WORD dx, WORD dy, WORD x1, WORD y1,
                WORD x2, WORD y2, BOOL UpdateDamageList, struct GfxBase *GfxBase)
{
    struct Layer     *L       = rp->Layer;
    struct Rectangle  ScrollRect;
    struct Rectangle  Rect;

    if(0 == dx && 0 == dy)
        return TRUE;

    ScrollRect.MinX = x1;
    ScrollRect.MinY = y1;
    ScrollRect.MaxX = x2;
    ScrollRect.MaxY = y2;

    if(!L) {
        Rect = ScrollRect;
        TranslateRect(&Rect, -dx, -dy);
        if(_AndRectRect(&ScrollRect, &Rect, &Rect)) {
            BltBitMap(rp->BitMap,
                      Rect.MinX + dx,
                      Rect.MinY + dy,
                      rp->BitMap,
                      Rect.MinX,
                      Rect.MinY,
                      Rect.MaxX - Rect.MinX + 1,
                      Rect.MaxY - Rect.MinY + 1,
                      0xc0, /* copy */
                      0xff,
                      NULL);
        }
    } else {
        struct ClipRect *SrcCR;

        LockLayerRom(L);

        if(L->Flags & LAYERSIMPLE && UpdateDamageList) {
            /* Scroll the old damagelist within the scroll area */
            ScrollRegion(L->DamageList, &ScrollRect, -dx, -dy);
        }

        /* The scrolling area is relative to the Layer, so make it relative to the screen */
        TranslateRect(&ScrollRect, MinX(L), MinY(L));

        /* The damage list will be formed by the now hidden layer's parts that will become visible due
           to the scrolling procedure, thus we procede this way:

           1) Calculate the invisible region out of the visible one, subtracting it from the
              scrolling area

           2) Scroll the invisible region by (-dx, -dy) and then subtract from it the not scrolled equivalent

           The regions that we obtain after (2) is the new damage list
        */

        if(L->Flags & LAYERSIMPLE && UpdateDamageList) {
            Rect = ScrollRect;

            TranslateRect(&Rect, dx, dy);

            if(_AndRectRect(&ScrollRect, &Rect, &Rect)) {
                struct Region *Damage = NewRegion();

                if(Damage) {
                    BOOL res = OrRectRegion(Damage, &ScrollRect);

                    if(!res) {
                        DisposeRegion(Damage);
                        Damage = NULL;
                    }
                }

                if(Damage) {
                    if
                    (
                        ClearRegionRegion(L->VisibleRegion, Damage)
                        &&
                        Damage->RegionRectangle
                    ) {
                        struct Region Tmp;
                        /*
                           We play sort of dirty here, by making assumptions about the internals of the
                           Region structure and the region handling functions, but we are allowed to do that,
                           aren't we? ;-)
                        */

                        Tmp = *Damage;

                        TranslateRect(Bounds(Damage), -dx, -dy);

                        if
                        (
                            ClearRegionRegion(&Tmp, Damage)
                            &&
                            Damage->RegionRectangle
                        ) {
                            AndRectRegion(Damage, &ScrollRect);
                            if(Damage->RegionRectangle)
                            {
                                /* Join the new damage list with the old one */
                                TranslateRect(Bounds(Damage), -MinX(L), -MinY(L));
                                OrRegionRegion(Damage, L->DamageList);

                                L->Flags |= LAYERREFRESH;
                            }
                        }
                    }

                    DisposeRegion(Damage);
                }
            }
        }

        if(LayersBase)
            SortLayerCR(L, dx, dy);

        for(SrcCR = L->ClipRect; SrcCR; SrcCR = SrcCR->Next) {
            if(_AndRectRect(&ScrollRect, Bounds(SrcCR), &Rect)) {
                TranslateRect(&Rect, -dx, -dy);

                if(_AndRectRect(&ScrollRect, &Rect, &Rect)) {
                    struct BitMap   *srcbm;
                    struct ClipRect *DstCR;
                    WORD             corrsrcx, corrsrcy;
                    ULONG            area;

                    if(SrcCR->lobs) {
                        if(L->Flags & LAYERSIMPLE) continue;

                        if(L->Flags & LAYERSUPER) {
                            corrsrcx = - MinX(L) + L->Scroll_X;
                            corrsrcy = - MinY(L) + L->Scroll_Y;
                            srcbm = L->SuperBitMap;
                        } else {
                            corrsrcx = - MinX(SrcCR) + CR_BMOFFX(SrcCR);
                            corrsrcy = - MinY(SrcCR) + CR_BMOFFY(SrcCR);
                            srcbm = SrcCR->BitMap;
                        }
                    } else {
                        corrsrcx  = 0;
                        corrsrcy  = 0;
                        srcbm = rp->BitMap;
                    }

                    area = (ULONG)(Rect.MaxX - Rect.MinX + 1) * (Rect.MaxY - Rect.MinY + 1);

                    for(DstCR = L->ClipRect ; area && DstCR; DstCR = DstCR->Next) {
                        struct Rectangle Rect2;

                        if(_AndRectRect(Bounds(DstCR), &Rect, &Rect2)) {
                            struct BitMap   *dstbm;
                            WORD             corrdstx, corrdsty;

                            area -= (ULONG)(Rect2.MaxX - Rect2.MinX + 1) * (Rect2.MaxY - Rect2.MinY + 1);

                            if(DstCR->lobs) {
                                if(L->Flags & LAYERSIMPLE) continue;

                                if(L->Flags & LAYERSUPER) {
                                    corrdstx = - MinX(L) + L->Scroll_X;
                                    corrdsty = - MinY(L) + L->Scroll_Y;
                                    dstbm = L->SuperBitMap;
                                } else {
                                    corrdstx = - MinX(DstCR) + CR_BMOFFX(DstCR);
                                    corrdsty = - MinY(DstCR) + CR_BMOFFY(DstCR);
                                    dstbm = DstCR->BitMap;
                                }
                            } else {
                                corrdstx  = 0;
                                corrdsty  = 0;
                                dstbm = rp->BitMap;
                            }

                            BltBitMap
                            (
                                srcbm,
                                Rect2.MinX + corrsrcx + dx,
                                Rect2.MinY + corrsrcy + dy,
                                dstbm,
                                Rect2.MinX + corrdstx,
                                Rect2.MinY + corrdsty,
                                Rect2.MaxX - Rect2.MinX + 1,
                                Rect2.MaxY - Rect2.MinY + 1,
                                0xC0,
                                0xFF,
                                NULL
                            );
                        }
                    }
                }
            }
        }

        UnlockLayerRom(L);
    }

    return TRUE;
}

/****************************************************************************************/

void GetRPClipRectangleForBitMap(struct BitMap *bm, struct Rectangle *r,
                                 struct GfxBase *GfxBase)
{
    /*
     * The whole bitmap. This prevents trashing memory by hitting
     * unallocated memory: the drivers don't check bitmap bounds.
     */
    r->MinX = 0;
    r->MinY = 0;
    r->MaxX = GetBitMapAttr(bm, BMA_WIDTH)  - 1;
    r->MaxY = GetBitMapAttr(bm, BMA_HEIGHT) - 1;
}

/****************************************************************************************/

ULONG bitmap_render(APTR bitmap_rd, WORD srcx, WORD srcy, struct BitMap *dstbm,
                    struct Rectangle *rect, struct GfxPens *pens,
                    struct GfxBase *GfxBase)
{
    struct bitmap_render_data *brd = bitmap_rd;
    WORD                       width  = rect->MaxX - rect->MinX + 1;
    WORD                       height = rect->MaxY - rect->MinY + 1;

    int_bltbitmap(brd->srcbm, srcx, srcy, dstbm, rect->MinX, rect->MinY,
                  width, height, brd->minterm, pens->gp_Mask, GfxBase);

    return (ULONG)width * height;
}
