/* SPDX-License-Identifier: MIT */
/*
 * lc.gfxdriver's port layer: the 8568's update address, pushes from the
 * shadow, block fills and copies over rows, and the dirty bytes a flush
 * pushes: a span a row, or one bit a byte for a row with more.  Rows are
 * the shadow's; row y is at VRAM lp_Base + (y / 2) * (lp_Step[0] +
 * lp_Step[1]) + (y & 1) * lp_Step[0], so an interlaced bitmap keeps its
 * even rows, then its odd ones:
 *
 *   progressive  lp_Step = { bpr, bpr }
 *   interlaced   lp_Step = { rows / 2 * bpr, bpr - rows / 2 * bpr }
 *
 * The
 * caller holds interrupts off with lp_Budget set to LCP_PIECE; when a
 * piece's estimated time runs out the layer waits for the chip, lets
 * interrupts in and starts the next.  Once a poll times out every call
 * returns FALSE and the chip is left alone.
 */
#ifndef LC_LCPORT_H
#define LC_LCPORT_H

#include <exec/types.h>

#define LCP_ROWS	480	/* the most rows of a mode */
#define LCP_PIECE	1000	/* microseconds of a piece */
#define LCP_NOADDR	0xFFFF	/* an update address never used */
#define LCP_SCRATCH	0xFFB0	/* a row of scratch VRAM, to the end */

/* The assembler's offsets must match. */
struct LCPort {
	UBYTE	*lp_Shadow;	/* VRAM 0x0000, rows of lp_BPR bytes */
	UWORD	lp_UA;		/* update address, or LCP_NOADDR */
	UWORD	lp_BA;		/* copy source address, or LCP_NOADDR */
	WORD	lp_Budget;	/* microseconds left in the piece */
	UWORD	lp_OK;		/* the chip answers */
	UWORD	lp_R24;		/* as written: bit 7 copy, else fill */
	WORD	lp_Y0, lp_Y1;	/* rows that may be dirty */
	UWORD	lp_BPR;		/* bytes a row: 80, or 40 at double width */
	ULONG	lp_SP;
	UWORD	lp_Base;	/* VRAM address of row 0 */
	UWORD	lp_Step[2];	/* VRAM from an even row to the next, an odd row */
	UBYTE	lp_Lo[LCP_ROWS];	/* each row's dirty bytes: 0xFF, 0 when clean */
	UBYTE	lp_Hi[LCP_ROWS];
	UBYTE	lp_Bits[LCP_ROWS * 10];	/* bit i of byte k: byte 8k + i */
	UBYTE	lp_Runs[82];
	UBYTE	lp_Cx[LCP_ROWS];	/* the row's bytes are in lp_Bits */
};

int lcv_wr(struct LCPort *lp, int reg, int val);
int lcv_ready(struct LCPort *lp);
/*
 * n bytes of row y from byte x.  Here and in lcv_fill and lcv_copy, n
 * past the row's end goes on in VRAM: the next row, or the next of its
 * field.
 */
int lcv_push(struct LCPort *lp, UWORD y, UWORD x, UWORD n);
/*
 * rows rows from y, n bytes of v each from byte x; edges bit 0 also
 * pushes the byte before each row, bit 1 the byte after it
 */
int lcv_fill(struct LCPort *lp, UWORD y, UWORD x, int v, UWORD n,
    UWORD rows, int edges);
/*
 * rows rows of n bytes from (sx, sy) to (x, y), both rows stepping by
 * step (1 or -1); edges as for lcv_fill, and bit 2 copies each row
 * through scratch VRAM, for a source that overlaps the row on its left
 */
int lcv_copy(struct LCPort *lp, UWORD y, UWORD x, UWORD sy, UWORD sx,
    UWORD n, UWORD rows, WORD step, int edges);
/* The dirty bytes, runs closer than 2 clean bytes as one */
int lcv_flush(struct LCPort *lp);
/* Bytes b0-b1 of rows y0-y1 dirty (set) or on the display (!set) */
void lcv_mark(struct LCPort *lp, WORD y0, WORD y1, UWORD b0, UWORD b1,
    int set);

#endif
