/*
 * lc.gfxdriver's port layer against a model of the 8568's ports:
 * register writes, pushes from the shadow,
 * row fills and copies with their edge bytes and scratch, block fill and
 * copy with R30 = 0 as 256, dirty bits and their flush, an interlaced
 * bitmap's rows in VRAM, pieces that end
 * with the chip idle, time-outs, and that the model catches a write while
 * busy, an access with the NVI enabled and Enable() with the chip able
 * to interrupt.
 */
#include <exec/exec.h>
#include <proto/exec.h>

#include "lc/vdcmodel.h"
#include "../libs/lc/lcport.h"

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL lcvdc: %s\n", what);
		fails++;
	}
}

static UBYTE shadow[16000];
static struct LCPort lp;

/* Model RAM a to a + n - 1 is the shadow's */
static BOOL same(UWORD a, UWORD n)
{
	for (; n > 0; n--, a++)
		if (lcm_peek(a) != shadow[a])
			return FALSE;
	return TRUE;
}

/* Bytes a to a + n - 1 of the model's RAM are v */
static BOOL ramis(UWORD a, UWORD n, UBYTE v)
{
	for (; n > 0; n--, a++)
		if (lcm_peek(a) != v)
			return FALSE;
	return TRUE;
}

static BOOL faultless(void)
{
	return lcm_stats.ls_Lost == 0 && lcm_stats.ls_NVIOn == 0 &&
	    lcm_stats.ls_Small == 0 && lcm_stats.ls_Stray == 0;
}

static void enter(void)
{
	Disable();
	lp.lp_Budget = LCP_PIECE;
}

static int leave(int ok)
{
	ok &= lcv_ready(&lp);
	Enable();
	return ok;
}

/* Row y's VRAM address, of 200 rows of 80 interlaced at 0x4000 */
#define LACEROW(y)	(0x4000 + ((y) & 1) * 8000 + (y) / 2 * 80)

/* Model RAM of row y's bytes x to x + n - 1, interlaced, is the shadow's */
static BOOL lsame(UWORD y, UWORD x, UWORD n)
{
	UWORD i;

	for (i = 0; i < n; i++)
		if (lcm_peek(LACEROW(y) + x + i) != shadow[y * 80 + x + i])
			return FALSE;
	return TRUE;
}

/* 200 rows, the even ones then the odd ones, from 0x4000 */
static void interlaced(void)
{
	UWORD i, k, ok;

	lp.lp_Base = 0x4000;
	lp.lp_Step[0] = 8000;
	lp.lp_Step[1] = 80 - 8000;
	for (i = 0; i < 16000; i++)
		shadow[i] = i * 5 + 1;
	enter();
	ok = lcv_push(&lp, 3, 2, 10) && lcv_push(&lp, 4, 0, 80);
	check(leave(ok) && lsame(3, 2, 10) && lsame(4, 0, 80) &&
	    lcm_peek(LACEROW(3) + 12) == 0x55, "interlaced push");
	/* odd rows 1, 3, 5 as one block */
	enter();
	ok = lcv_fill(&lp, 1, 0, 0x33, 3 * 80, 1, 0) &&
	    lcv_fill(&lp, 7, 10, 0x0F, 20, 4, 3);
	ok = leave(ok) && ramis(LACEROW(1), 240, 0x33) &&
	    lcm_peek(LACEROW(7) + 80) == 0x55;
	for (i = 7; i < 11; i++)
		ok &= ramis(LACEROW(i) + 10, 20, 0x0F) && lsame(i, 9, 1) &&
		    lsame(i, 30, 1) && lcm_peek(LACEROW(i) + 31) == 0x55;
	check(ok, "interlaced fills");
	/* up a row, each row from the other field, top down */
	enter();
	for (i = 20; i < 27; i++)
		lcv_push(&lp, i, 0, 80);
	ok = lcv_copy(&lp, 20, 4, 21, 4, 30, 6, 1, 0);
	ok = leave(ok);
	for (i = 20; i < 26; i++)
		for (k = 0; k < 30; k++)
			ok &= lcm_peek(LACEROW(i) + 4 + k) ==
			    shadow[(i + 1) * 80 + 4 + k];
	check(ok && lsame(26, 0, 80), "interlaced copy up a row");
	/* the flush of rows 40-43 and 101 */
	lcv_mark(&lp, 40, 43, 5, 9, TRUE);
	lcv_mark(&lp, 101, 101, 0, 79, TRUE);
	enter();
	ok = lcv_flush(&lp);
	check(leave(ok) && lsame(40, 5, 5) && lsame(41, 5, 5) &&
	    lsame(42, 5, 5) && lsame(43, 5, 5) && lsame(101, 0, 80) &&
	    lcm_peek(LACEROW(40) + 4) == 0x55, "interlaced flush");
	check(faultless(), "faults, interlaced");
	lp.lp_Base = 0;
	lp.lp_Step[0] = lp.lp_Step[1] = 80;
}

int main(void)
{
	UWORD i, k, ok, a;
	ULONG seeks;

	if (!lcm_init(1, 0x55)) {
		KPutStr("FAIL lcvdc: no memory\n");
		return 1;
	}
	check((UBYTE *)&lp.lp_Bits - (UBYTE *)&lp == 990 &&
	    (UBYTE *)&lp.lp_BPR - (UBYTE *)&lp == 18 &&
	    (UBYTE *)&lp.lp_SP - (UBYTE *)&lp == 20 &&
	    (UBYTE *)&lp.lp_Step - (UBYTE *)&lp == 26 &&
	    (UBYTE *)&lp.lp_Cx - (UBYTE *)&lp == 5872, "struct LCPort offsets");
	lcm_watch(TRUE);
	for (i = 0; i < 16000; i++)
		shadow[i] = i * 7 + 3;
	lp.lp_Shadow = shadow;
	lp.lp_BPR = 80;
	lp.lp_Step[0] = lp.lp_Step[1] = 80;
	lp.lp_OK = TRUE;
	lp.lp_UA = lp.lp_BA = LCP_NOADDR;
	lp.lp_Y0 = LCP_ROWS;
	lp.lp_Y1 = -1;
	for (i = 0; i < LCP_ROWS; i++) {
		lp.lp_Lo[i] = 0xFF;
		lp.lp_Hi[i] = 0;
		lp.lp_Cx[i] = 0;
	}

	/* registers, 64 KB addressing first */
	enter();
	ok = lcv_wr(&lp, 28, 0x10) && lcv_wr(&lp, 26, 0xF2) &&
	    lcv_wr(&lp, 25, 0x87);
	check(leave(ok), "register writes");
	check(lcm_reg[28] == 0x1F && lcm_reg[26] == 0xF2 && lcm_reg[25] == 0x87,
	    "register values, unused bits 1");

	/* a push in two runs, the address carried over, then a long one */
	enter();
	ok = lcv_push(&lp, 58, 20, 64) && lcv_push(&lp, 58, 20 + 64, 236);
	check(leave(ok) && same(0x1234, 300), "push");
	check(lcm_peek(0x1233) == 0x55 && lcm_peek(0x1234 + 300) == 0x55,
	    "push bounds");
	check(lcm_stats.ls_Data[31] == 300 && lcm_stats.ls_Data[18] == 1 &&
	    lcm_stats.ls_Data[19] == 1, "push writes");
	enter();
	ok = lcv_push(&lp, 60, 64, 3000);
	check(leave(ok) && same(0x1300, 3000), "push in pieces");
	check(lcm_stats.ls_Data[18] == 1 && lcm_stats.ls_Data[19] == 2,
	    "R19 alone in the same 256 bytes");

	/* fill: R31 writes the first byte, R30 the rest; 0 is 256 */
	enter();
	ok = lcv_fill(&lp, 3, 16, 0xA5, 100, 1, 0);
	check(leave(ok) && ramis(0x0100, 100, 0xA5) && lcm_peek(0x0164) == 0x55,
	    "fill 100");
	enter();
	ok = lcv_fill(&lp, 6, 32, 0x3C, 257, 1, 0);
	check(leave(ok) && ramis(0x0200, 257, 0x3C) && lcm_peek(0x0301) == 0x55,
	    "fill 257");
	/* 10 rows of 20 at byte 5, with both edges from the shadow */
	enter();
	ok = lcv_fill(&lp, 30, 5, 0x0F, 20, 10, 3);
	ok = leave(ok);
	for (i = 0; i < 10; i++) {
		a = 2400 + i * 80;
		ok &= ramis(a + 5, 20, 0x0F) && same(a + 4, 1) && same(a + 25, 1) &&
		    lcm_peek(a + 3) == 0x55 && lcm_peek(a + 26) == 0x55;
	}
	check(ok, "fill rows");

	/* copy from R32/R33 to the update address */
	seeks = lcm_stats.ls_Data[18] + lcm_stats.ls_Data[19];
	enter();
	ok = lcv_copy(&lp, 102, 32, 58, 20, 200, 1, 1, 0);
	ok = leave(ok);
	for (i = 0; i < 200 && lcm_peek(0x2000 + i) == shadow[0x1234 + i]; i++)
		;
	check(ok && i == 200 && lcm_peek(0x2000 + 200) == 0x55, "copy");
	check(lcm_stats.ls_Fill == 99 + 256 + 200 - 10 &&
	    lcm_stats.ls_Copy == 200, "block counts");
	/* 10 rows of 20 moved right by 4 bytes through scratch, bottom up */
	enter();
	for (i = 0; i < 10; i++)
		lcv_push(&lp, 30 + i, 4, 22);
	ok = lcv_copy(&lp, 39, 9, 39, 5, 20, 10, -1, 7);
	ok = leave(ok);
	for (i = 0; i < 10; i++) {
		a = 2400 + i * 80;
		for (k = 0; k < 20; k++)
			ok &= lcm_peek(a + 9 + k) == shadow[a + 5 + k];
		ok &= same(a + 8, 1) && same(a + 29, 1) && same(a + 4, 4);
	}
	check(ok, "copy rows through scratch");
	check(lcm_stats.ls_Busy > 0, "busy polls");
	check(lcm_stats.ls_Data[18] + lcm_stats.ls_Data[19] > seeks, "seeks");
	check(faultless(), "faults");

	/* dirty bits: spans 2 bytes apart push apart, 1 byte apart as one */
	for (i = 0; i < 16000; i++)
		shadow[i] ^= 0xFF;
	lcv_mark(&lp, 10, 12, 3, 17, TRUE);
	lcv_mark(&lp, 11, 11, 19, 19, TRUE);
	lcv_mark(&lp, 11, 11, 22, 22, TRUE);
	lcv_mark(&lp, 150, 199, 0, 79, TRUE);
	lcv_mark(&lp, 160, 160, 8, 70, FALSE);
	check(lp.lp_Y0 == 10 && lp.lp_Y1 == 199 && lp.lp_Lo[11] == 3 &&
	    lp.lp_Hi[11] == 22, "dirty rows");
	seeks = lcm_stats.ls_Data[19];
	enter();
	ok = lcv_flush(&lp);
	check(leave(ok), "flush");
	check(same(10 * 80 + 3, 15) && same(11 * 80 + 3, 17) &&
	    same(11 * 80 + 22, 1) && same(12 * 80 + 3, 15) &&
	    lcm_peek(11 * 80 + 20) != shadow[11 * 80 + 20] &&
	    lcm_peek(11 * 80 + 21) != shadow[11 * 80 + 21] &&
	    lcm_peek(13 * 80 + 3) != shadow[13 * 80 + 3], "flushed spans");
	check(same(150 * 80, 10 * 80 + 8) && same(160 * 80 + 71, 39 * 80 + 9) &&
	    lcm_peek(160 * 80 + 8) != shadow[160 * 80 + 8] &&
	    lcm_peek(160 * 80 + 70) != shadow[160 * 80 + 70], "flushed rows");
	check(lcm_stats.ls_Data[19] - seeks == 6, "flush seeks");
	check(lp.lp_Y1 == -1 && lp.lp_Lo[11] == 0xFF && lp.lp_Bits[111] == 0,
	    "clean after the flush");
	check(faultless(), "faults after the flush");

	/* rows of 40 bytes, as at double width */
	lp.lp_BPR = lp.lp_Step[0] = lp.lp_Step[1] = 40;
	for (i = 0; i < 16000; i++)
		shadow[i] ^= 0xFF;
	lcv_mark(&lp, 5, 6, 30, 39, TRUE);
	lcv_mark(&lp, 255, 255, 0, 3, TRUE);
	enter();
	ok = lcv_flush(&lp) && lcv_fill(&lp, 100, 2, 0x81, 30, 3, 3);
	check(leave(ok) && same(5 * 40 + 30, 10) && same(6 * 40 + 30, 10) &&
	    same(255 * 40, 4) && lcm_peek(5 * 40 + 29) != shadow[5 * 40 + 29] &&
	    lcm_peek(7 * 40 + 30) != shadow[7 * 40 + 30], "flush, 40 a row");
	check(ramis(102 * 40 + 2, 30, 0x81) && same(102 * 40 + 1, 1) &&
	    same(102 * 40 + 32, 1) && same(100 * 40 + 1, 1),
	    "fill, 40 a row");
	lp.lp_BPR = lp.lp_Step[0] = lp.lp_Step[1] = 80;

	interlaced();

	/* the model's checks */
	Disable();
	lcv_push(&lp, 153, 48, 1);
	lcv_ready(&lp);
	lcm_select(31, 0);
	lcm_data(1, 0);
	lcm_data(2, 0);
	lcv_ready(&lp);
	Enable();
	check(lcm_stats.ls_Lost == 1 && lcm_peek(0x3001) == 2, "lost write");
	lcm_stats.ls_Lost = 0;

	lcv_wr(&lp, 26, 0x0F);
	lcv_ready(&lp);
	check(lcm_stats.ls_NVIOn > 0, "access with NVI on");
	lcm_stats.ls_NVIOn = 0;

	Disable();
	lcm_select(31, 0);
	lcm_data(1, 0);
	Enable();
	check(lcm_stats.ls_Stray == 1, "Enable while busy");
	Disable();
	lcv_ready(&lp);
	Enable();
	lcm_stats.ls_Stray = 0;

	/* a chip that stays busy times out, and is left alone after */
	enter();
	lcv_wr(&lp, 31, 0);
	lcm_hang(TRUE);
	ok = lcv_wr(&lp, 31, 1);
	lcm_hang(FALSE);
	ok |= lcv_ready(&lp) | lcv_push(&lp, 0, 0, 4) | lcv_flush(&lp);
	seeks = lcm_stats.ls_Status;
	ok |= lcv_fill(&lp, 0, 0, 0, 10, 1, 0);
	ok |= seeks != lcm_stats.ls_Status;
	lp.lp_OK = TRUE;
	lcv_ready(&lp);
	Enable();
	check(!ok, "time-out");
	check(faultless(), "faults after the checks");

	lcm_watch(FALSE);
	xKPrintf("lcvdc: %ld status reads, %ld busy, %ld selects\n",
	    lcm_stats.ls_Status, lcm_stats.ls_Busy, lcm_stats.ls_Select);
	lcm_free();
	if (fails == 0)
		KPutStr("PASS lcvdc\n");
	return fails != 0;
}
