/* SPDX-License-Identifier: MIT */
/*
 * Planar bitmaps: 1 to 8 Amiga planes of one bit a pixel, bit 7 of a
 * byte leftmost.  A plane pointer of NULL reads as all 0 and of -1 as
 * all 1, and neither is written.
 */
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include "gfxformat.h"

#define NOPLANE(pl)	((pl) == NULL || (pl) == (PLANEPTR)-1)

/* Byte o of plane i */
static UBYTE *pl_at(struct BitMap *bm, UWORD i, ULONG o)
{
    if (o < 0x10000L)
        return bm->Planes[i] + (UWORD)o;
    return GFXBM(bm)->gb_Half1 + (UWORD)o;
}

static ULONG pl_ReadPixel(struct BitMap *bm, WORD x, WORD y)
{
    ULONG o = (ULONG)y * bm->BytesPerRow + (x >> 3);
    UBYTE bit = 0x80 >> (x & 7);
    UBYTE v = 0;
    UWORD i;
    PLANEPTR pl;

    for (i = 0; i < bm->Depth; i++) {
        pl = bm->Planes[i];
        if (pl == (PLANEPTR)-1 || (pl != NULL && (*pl_at(bm, i, o) & bit)))
            v |= 1 << i;
    }
    return v;
}

static void pl_WritePixel(struct BitMap *bm, WORD x, WORD y,
    const struct GfxPens *p)
{
    ULONG o = (ULONG)y * bm->BytesPerRow + (x >> 3);
    UBYTE bit = 0x80 >> (x & 7);
    UBYTE *d;
    UWORD i;

    for (i = 0; i < bm->Depth; i++) {
        if (!((p->gp_Mask >> i) & 1) || NOPLANE(bm->Planes[i]))
            continue;
        d = pl_at(bm, i, o);
        if (p->gp_DrMd & COMPLEMENT)
            *d ^= bit;
        else if ((p->gp_APen >> i) & 1)
            *d |= bit;
        else
            *d &= ~bit;
    }
}

static void pl_ReadRow(struct BitMap *bm, WORD x, WORD y, UWORD n, UBYTE *pix)
{
    while (n-- > 0)
        *pix++ = pl_ReadPixel(bm, x++, y);
}

static void pl_WriteRow(struct BitMap *bm, WORD x, WORD y, UWORD n,
    const UBYTE *pix, UBYTE mask)
{
    struct GfxPens p;

    p.gp_DrMd = JAM1;
    p.gp_Mask = mask;
    while (n-- > 0) {
        p.gp_APen = *pix++;
        pl_WritePixel(bm, x++, y, &p);
    }
}

struct GfxDriver gfx_Planar = {
    { NULL, NULL, 0, 0, "planar" },
    GDF_PLANAR,
    gfx_Fill,
    gfx_Template,
    gfx_Copy,
    pl_ReadPixel,
    pl_WritePixel,
    gfx_Line,
    pl_ReadRow,
    pl_WriteRow
};
