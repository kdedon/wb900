/* SPDX-License-Identifier: MIT */
/*
 * The bitmap formats graphics.library draws in RAM, and the row code
 * they share.
 *
 * Both formats are rows of bits: a planar plane has 1 bit a pixel, a
 * chunky bitmap one plane of 4.  Pixel x of a row is at bit x * bpp,
 * bit 15 of a word leftmost.  The row code works on whole words with
 * masks at the ends, so BytesPerRow and the plane pointers are even.
 */
#ifndef GFXFORMAT_H
#define GFXFORMAT_H

#include <graphics/gfxdriver.h>

extern struct GfxDriver gfx_Planar;	/* GDF_PLANAR */
extern struct GfxDriver gfx_Chunky4;	/* GDF_CHUNKY4 */

/* The table that draws bm */
#define GFX_DRIVER(bm) (IS_GFXBM(bm) ? GFXBM(bm)->gb_Driver : &gfx_Planar)

/*
 * One plane as the row code draws it: its two halves, the pens and the
 * write mask as words of pixels.  A source plane with pl_Lo NULL reads
 * as pl_F everywhere.
 */
struct GfxPlane {
    UBYTE	*pl_Lo;		/* offsets 0-0xFFFF */
    UBYTE	*pl_Hi;		/* offsets from 0x10000 */
    UWORD	 pl_F, pl_B;	/* APen and BPen */
    UWORD	 pl_M;		/* pixel bits written */
};

/* The planes of bm that p draws into pl; how many, and the bits a pixel */
WORD gfx_Planes(struct BitMap *bm, const struct GfxPens *p,
    struct GfxPlane *pl, UWORD *bpp);

/* The tables of the row code, made once */
void gfx_InitFormats(void);

/* gd_Fill, gd_Template, gd_Copy and gd_Line of both formats */
void gfx_Fill(struct BitMap *bm, const struct Rectangle *r,
    const struct GfxPens *p);
void gfx_Template(struct BitMap *bm, const struct Rectangle *r,
    const UBYTE *src, UWORD srcx, UWORD srcmod, const struct GfxPens *p);
void gfx_Copy(struct BitMap *src, WORD sx, WORD sy, struct BitMap *dst,
    const struct Rectangle *r, UBYTE minterm, UBYTE mask);
void gfx_Line(struct BitMap *bm, const struct Rectangle *clip,
    WORD x0, WORD y0, WORD x1, WORD y1, UWORD ptrn,
    const struct GfxPens *p);

/*
 * The word loops (gfxasm.s).  A run never crosses a 64 KB segment.
 * gfx_FillRows: n words of rows rows bpr bytes apart, d = (d & a) ^ x,
 * row r with a[0-3], x[0-3] (words in turn) from 8-word entry
 * (pidx + r) & pmask of tab; with a all 0 nothing is read.
 * gfx_FillRect: n words of rows rows bpr bytes apart, d = (d & a) ^ x,
 * the first word under lm and the last under rm (n 1: lm only).
 * gfx_TmplRows: the rows of a template by job (gfxformat.c).
 * gfx_CopyRows: n words of rows rows, the first under lm and the last
 * under rm, rows dstep and sstep bytes apart, upwards or (dir < 0)
 * downwards along a row.  gfx_ShiftRows: the same with each word from
 * the bits sh (1-15) on of two source words, reading n + 1 a row.
 * gfx_LineRun: count pixels of a line in one half of a plane, from
 * job (gfxformat.c), x bytes stepping by xstep.  gfx_WCopy: n
 * words, upwards.
 * gfx_Bits: n words of the bits of byte string s from bit sh (0-7),
 * reading s[0] to s[2n] (s[2n - 1] when sh is 0).
 */
void gfx_FillRows(UWORD *d, UWORD n, UWORD rows, UWORD bpr,
    const UWORD *tab, UWORD pmask, UWORD pidx);
void gfx_FillRect(UWORD *d, UWORD n, UWORD rows, UWORD bpr, UWORD a,
    UWORD x, UWORD lm, UWORD rm);
void gfx_TmplRows(APTR job);
void gfx_CopyRows(UWORD *d, const UWORD *s, UWORD n, UWORD rows,
    WORD dstep, WORD sstep, UWORD lm, UWORD rm, WORD dir);
void gfx_ShiftRows(UWORD *d, const UWORD *s, UWORD n, UWORD rows,
    WORD dstep, WORD sstep, UWORD sh, UWORD lm, UWORD rm, WORD dir);
void gfx_LineRun(APTR job, WORD xstep, UWORD count);
void gfx_WCopy(UWORD *d, const UWORD *s, UWORD n);
void gfx_Bits(UWORD *out, const UBYTE *s, UWORD sh, UWORD n);

/* The minterm's upper nibble on source b and destination c */
#define MINTERM(mt, b, c) \
	((((mt) & 0x80) ? ((b) & (c)) : 0) | \
	 (((mt) & 0x40) ? ((b) & ~(c)) : 0) | \
	 (((mt) & 0x20) ? (~(b) & (c)) : 0) | \
	 (((mt) & 0x10) ? (~(b) & ~(c)) : 0))

#endif
