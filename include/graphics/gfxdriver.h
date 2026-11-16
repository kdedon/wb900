/* SPDX-License-Identifier: MIT */
/*
 * graphics.library's driver table: the operations of one bitmap format,
 * and for a display card also its modes, palette and display hook.
 *
 * graphics.library clips every operation to the bitmap and to the layer's
 * ClipRects before it calls a driver, so a driver never clips: every
 * rectangle is inclusive and inside the bitmap.  Pens are pixel values
 * (graphics.library maps them through gb_PixTab first).
 *
 * The pixel data of plane 0 is a byte array of up to 128 KB in two
 * halves: offsets 0-0xFFFF from Planes[0] and 0x10000-0x1FFFF from
 * gb_Half1, as framebuf.resource's fbm_Half.  A row may cross between
 * the halves, even in the middle (GD_ADDR).  Planes 1-7 of a
 * planar bitmap fit 64 KB each.
 */
#ifndef GRAPHICS_GFXDRIVER_H
#define GRAPHICS_GFXDRIVER_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef EXEC_NODES_H
#include <exec/nodes.h>
#endif
#ifndef GRAPHICS_GFX_H
#include <graphics/gfx.h>
#endif
#ifndef UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif

/* gd_Format */
#define GDF_PLANAR	1	/* Amiga planes, bit 7 leftmost, 1-8 deep */
#define GDF_CHUNKY4	2	/* 2 pixels a byte, high nibble leftmost */
#define GDF_COUNT	2

/*
 * AllocBitMap() tag (BMF_RTGTAGS etc. in flags): a RAM bitmap of this
 * GDF_ format with no friend.
 */
#define BMATags_GfxFormat	(TAG_USER + 0x60)

/* What a drawing operation writes, from the RastPort. */
struct GfxPens
{
    UBYTE	 gp_APen;	/* pixel for 1 bits of the pattern */
    UBYTE	 gp_BPen;	/* pixel for 0 bits, in JAM2 */
    UBYTE	 gp_DrMd;	/* JAM1, JAM2 or COMPLEMENT, | INVERSVID */
    UBYTE	 gp_Mask;	/* pixel bits written */
    const UWORD *gp_Ptrn;	/* area pattern, rows by bitmap y; NULL: solid */
    UWORD	 gp_PtrnMask;	/* pattern rows - 1 (a power of 2, less 1) */
};

struct BitMap;
struct Rectangle;

struct GfxMode
{
    ULONG	gm_ModeID;
    UWORD	gm_Width, gm_Height;
    UWORD	gm_Depth;
    UWORD	gm_Format;	/* GDF_ */
    /* A pixel's size in ticks; 0: square on a 5:4 screen */
    UWORD	gm_ResX, gm_ResY;
    /* NameInfo's name; NULL: "<DRIVER>:<W>x<H> Mono" (or RGBI) */
    const char	*gm_Name;
};

struct GfxDriver
{
    struct Node	 gd_Node;	/* ln_Name: the driver; graphics' list */
    UWORD	 gd_Format;	/* GDF_ */

    /* r with the pattern (or solid): 1 -> APen, 0 -> BPen (JAM2) */
    void	(*gd_Fill)(struct BitMap *bm, const struct Rectangle *r,
		    const struct GfxPens *p);
    /*
     * r with a 1-bit template: bit srcx (bit 7 of byte 0 is 0) of each
     * source row, rows srcmod bytes apart, goes to r->MinX.  As gd_Fill.
     */
    void	(*gd_Template)(struct BitMap *bm, const struct Rectangle *r,
		    const UBYTE *src, UWORD srcx, UWORD srcmod,
		    const struct GfxPens *p);
    /*
     * Copy from (sx, sy) of src, a bitmap of this format, to r of dst:
     * each pixel bit by the minterm's upper nibble (B source, C
     * destination), only the bits of mask.  src may be dst; any overlap.
     */
    void	(*gd_Copy)(struct BitMap *src, WORD sx, WORD sy,
		    struct BitMap *dst, const struct Rectangle *r,
		    UBYTE minterm, UBYTE mask);
    ULONG	(*gd_ReadPixel)(struct BitMap *bm, WORD x, WORD y);
    /* APen in JAM1/JAM2, the complement in COMPLEMENT */
    void	(*gd_WritePixel)(struct BitMap *bm, WORD x, WORD y,
		    const struct GfxPens *p);
    /*
     * A line, both ends included, only the pixels inside clip: pattern
     * bit 15 at (x0, y0), then down one bit a pixel; 1 -> APen,
     * 0 -> BPen (JAM2).
     */
    void	(*gd_Line)(struct BitMap *bm, const struct Rectangle *clip,
		    WORD x0, WORD y0, WORD x1, WORD y1, UWORD ptrn,
		    const struct GfxPens *p);
    /* n pixels of row y from x, one byte each, to and from a buffer */
    void	(*gd_ReadRow)(struct BitMap *bm, WORD x, WORD y, UWORD n,
		    UBYTE *pix);
    void	(*gd_WriteRow)(struct BitMap *bm, WORD x, WORD y, UWORD n,
		    const UBYTE *pix, UBYTE mask);

    /* Displays only; NULL in a bitmap format's table. */
    const struct GfxMode *gd_Modes;
    UWORD	 gd_NumModes;
    /* Pen n to r, g, b (8 bits each); NULL: a fixed palette */
    void	(*gd_SetColor)(struct GfxDriver *gd, UWORD n, UBYTE r,
		    UBYTE g, UBYTE b);
    /* The colour of pixel n of a fixed palette, 0x00RRGGBB */
    ULONG	(*gd_GetColor)(struct GfxDriver *gd, UWORD n);
    /*
     * Fill in bm, graphics' GfxBitMap, as the display in mode m, but for
     * gb_PixTab, which graphics sets; or FALSE
     */
    BOOL	(*gd_AllocDisplay)(struct GfxDriver *gd,
		    const struct GfxMode *m, struct BitMap *bm);
    void	(*gd_FreeDisplay)(struct GfxDriver *gd, struct BitMap *bm);
    /* Show bm (from gd_AllocDisplay), or give the display up (NULL) */
    BOOL	(*gd_Show)(struct GfxDriver *gd, struct BitMap *bm);
    /*
     * A display drawn in RAM: put on the display what drawing into bm
     * changed, once a drawing call and any pointer update are done.
     * NULL: drawing shows at once.
     */
    void	(*gd_Flush)(struct GfxDriver *gd, struct BitMap *bm);
    /*
     * A card whose mode is a jumper: the mode it is taken to be set to,
     * the only one available.  gd_AllocDisplay() of another of gd_Modes
     * takes the jumper to be that one.  NULL: every mode is available.
     */
    const struct GfxMode *gd_Jumper;
    /*
     * Show bm, a RAM bitmap of the display bitmap's format and size, from
     * its own planes, which become the display's; the display bitmap's
     * planes become RAM.  NULL: the display's planes are fixed, so
     * graphics trades their pixels and pointers with bm's.
     */
    void	(*gd_Adopt)(struct GfxDriver *gd, struct BitMap *bm);
};

/*
 * A bitmap from AllocBitMap() or a display: struct BitMap with pad set
 * to GFXBM_MAGIC, followed by this.
 */
#define GFXBM_MAGIC	0x4778		/* 'Gx' */

struct GfxBitMap
{
    struct BitMap	 gb_BitMap;
    struct GfxDriver	*gb_Driver;	/* the format's table */
    UBYTE		*gb_Half1;	/* offset 0x10000 of plane 0, or NULL */
    UBYTE		*gb_PixTab;	/* pen -> pixel, or NULL: the same */
    UWORD		 gb_Flags;	/* GBF_ */
    ULONG		 gb_PlaneSize;	/* bytes allocated for each plane */
    ULONG		 gb_ModeID;	/* display mode, or INVALID_ID */
};

#define GBF_OWNPLANES	1	/* FreeBitMap() frees the planes */
#define GBF_DISPLAY	2	/* the display's planes: gd_FreeDisplay() frees them */

#define GFXBM(bm)	((struct GfxBitMap *)(bm))
#define IS_GFXBM(bm)	((bm)->pad == GFXBM_MAGIC)

/* Byte o of plane 0 of bm. */
#define GD_ADDR(bm, o) \
	((o) < 0x10000L ? (bm)->Planes[0] + (UWORD)(o) \
	    : GFXBM(bm)->gb_Half1 + (UWORD)(o))

/*
 * A display driver is a library, LIBS:<card>.gfxdriver, whose first
 * function GetGfxDriver(fmts) (LVO -30, the base first) returns its
 * table, or NULL when its card is not there.  fmts[f - 1] is
 * graphics.library's table for format f (GDF_): the driver's display
 * bitmaps draw with its gd_Fill to gd_WriteRow.  gd_Modes are the
 * display's modes.  gd_AllocDisplay() takes the display from the
 * console, blanked until gd_Show(); it goes back to the console once
 * its bitmap is freed and not shown.  A display bitmap's planes may move
 * to another bitmap of its mode (gd_Adopt), which then has
 * GBF_DISPLAY and the driver's table.
 */
#define GFX_GETDRIVER(base, fmts) \
	(*(struct GfxDriver *(*)())((char *)(base) - 30))((base), (fmts))

#endif
