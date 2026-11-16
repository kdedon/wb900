/* SPDX-License-Identifier: MIT */
/*
 * lc.gfxdriver's port layer: the 8568's update address, pushes from the
 * shadow, block fills and copies over rows, and the dirty bytes a flush
 * pushes: a span a row, or one bit a byte for a row with more.  The
 * caller holds interrupts off with lp_Budget set to LCP_PIECE; when a
 * piece's estimated time runs out the layer waits for the chip, lets
 * interrupts in and starts the next.  Once a poll times out every call
 * returns FALSE and the chip is left alone.
 */
#ifndef LC_LCPORT_H
#define LC_LCPORT_H

#include <exec/types.h>

#define LCP_ROWS	256	/* the most rows of a mode */
#define LCP_PIECE	1000	/* microseconds of a piece */
#define LCP_NOADDR	0xFFFF	/* an update address never used */

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
	UBYTE	lp_Lo[LCP_ROWS];	/* each row's dirty bytes: 0xFF, 0 when clean */
	UBYTE	lp_Hi[LCP_ROWS];
	UBYTE	lp_Bits[LCP_ROWS * 10];	/* bit i of byte k: byte 8k + i */
	UBYTE	lp_Runs[82];
	UBYTE	lp_Cx[LCP_ROWS];	/* the row's bytes are in lp_Bits */
};

int lcv_wr(struct LCPort *lp, int reg, int val);
int lcv_ready(struct LCPort *lp);
/* n bytes from the shadow at a */
int lcv_push(struct LCPort *lp, UWORD a, UWORD n);
/*
 * rows rows lp_BPR bytes apart from a, n bytes of v each; edges bit 0
 * also pushes the byte before each row, bit 1 the byte after it
 */
int lcv_fill(struct LCPort *lp, UWORD a, int v, UWORD n, UWORD rows,
    int edges);
/*
 * rows rows of n bytes from s to d, both stepping by step; edges as for
 * lcv_fill, and bit 2 copies each row through scratch VRAM, for a source
 * that overlaps the row on its left
 */
int lcv_copy(struct LCPort *lp, UWORD d, UWORD s, UWORD n, UWORD rows,
    WORD step, int edges);
/* The dirty bytes, runs closer than 2 clean bytes as one */
int lcv_flush(struct LCPort *lp);
/* Bytes b0-b1 of rows y0-y1 dirty (set) or on the display (!set) */
void lcv_mark(struct LCPort *lp, WORD y0, WORD y1, UWORD b0, UWORD b1,
    int set);

#endif
