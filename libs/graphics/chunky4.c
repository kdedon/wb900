/* SPDX-License-Identifier: MIT */
/*
 * Chunky bitmaps of 4 bits a pixel: 2 pixels a byte, the high nibble
 * leftmost (the HR card's IRGB layout).
 */
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include "gfxformat.h"

static UBYTE *ch_at(struct BitMap *bm, ULONG o)
{
    return GD_ADDR(bm, o);
}

static ULONG ch_ReadPixel(struct BitMap *bm, WORD x, WORD y)
{
    UBYTE v = *ch_at(bm, (ULONG)y * bm->BytesPerRow + (x >> 1));

    return (x & 1) ? v & 0xF : v >> 4;
}

static void ch_WritePixel(struct BitMap *bm, WORD x, WORD y,
    const struct GfxPens *p)
{
    UBYTE *d = ch_at(bm, (ULONG)y * bm->BytesPerRow + (x >> 1));
    UBYTE m = p->gp_Mask & 0xF;
    UBYTE v = p->gp_APen;

    if (!(x & 1)) {
        m <<= 4;
        v <<= 4;
    }
    if (p->gp_DrMd & COMPLEMENT)
        *d ^= m;
    else
        *d = (*d & ~m) | (v & m);
}

static void ch_ReadRow(struct BitMap *bm, WORD x, WORD y, UWORD n, UBYTE *pix)
{
    while (n-- > 0)
        *pix++ = ch_ReadPixel(bm, x++, y);
}

static void ch_WriteRow(struct BitMap *bm, WORD x, WORD y, UWORD n,
    const UBYTE *pix, UBYTE mask)
{
    struct GfxPens p;

    p.gp_DrMd = JAM1;
    p.gp_Mask = mask;
    while (n-- > 0) {
        p.gp_APen = *pix++;
        ch_WritePixel(bm, x++, y, &p);
    }
}

struct GfxDriver gfx_Chunky4 = {
    { NULL, NULL, 0, 0, "chunky4" },
    GDF_CHUNKY4,
    gfx_Fill,
    gfx_Template,
    gfx_Copy,
    ch_ReadPixel,
    ch_WritePixel,
    gfx_Line,
    ch_ReadRow,
    ch_WriteRow
};
