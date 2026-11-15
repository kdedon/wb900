/*
 * lc.gfxdriver's port layer against a model of the 8568's ports (the
 * emulator has no LC card): register writes, the update address, pushes
 * through R31, block fill and copy with R30 = 0 as 256, time-outs, and
 * that the model catches a write while busy, an access with the NVI
 * enabled and Enable() with the chip able to interrupt.
 */
#include <exec/exec.h>
#include <proto/exec.h>

#include "lc/vdcmodel.h"

int lcv_wr(int reg, int val);
int lcv_seek(UWORD addr);
int lcv_push(const UBYTE *src, UWORD n);
int lcv_ready(void);

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL lcvdc: %s\n", what);
		fails++;
	}
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

int main(void)
{
	static UBYTE buf[300];
	UWORD i, ok;

	if (!lcm_init(1, 0x55)) {
		KPutStr("FAIL lcvdc: no memory\n");
		return 1;
	}
	lcm_watch(TRUE);
	for (i = 0; i < 300; i++)
		buf[i] = i * 7 + 3;

	/* registers, 64 KB addressing first */
	Disable();
	ok = lcv_wr(28, 0x10) && lcv_wr(26, 0xF2) && lcv_wr(25, 0x87);
	ok &= lcv_ready();
	Enable();
	check(ok, "register writes");
	check(lcm_reg[28] == 0x10 && lcm_reg[26] == 0xF2 && lcm_reg[25] == 0x87,
	    "register values");

	/* a push in two runs, the address carried over */
	Disable();
	ok = lcv_seek(0x1234) && lcv_push(buf, 64) && lcv_ready();
	Enable();
	Disable();
	ok &= lcv_push(buf + 64, 236) && lcv_ready();
	Enable();
	check(ok, "push");
	for (i = 0; i < 300 && lcm_peek(0x1234 + i) == buf[i]; i++)
		;
	check(i == 300, "push bytes");
	check(lcm_peek(0x1233) == 0x55 && lcm_peek(0x1234 + 300) == 0x55,
	    "push bounds");
	check(lcm_stats.ls_Data[31] == 300 && lcm_stats.ls_Data[18] == 1 &&
	    lcm_stats.ls_Data[19] == 1, "push writes");

	/* fill: R31 writes the first byte, R30 the rest; 0 is 256 */
	Disable();
	ok = lcv_seek(0x0100) && lcv_wr(24, 0x00) && lcv_wr(31, 0xA5) &&
	    lcv_wr(30, 99) && lcv_ready();
	Enable();
	check(ok && ramis(0x0100, 100, 0xA5) && lcm_peek(0x0164) == 0x55,
	    "fill 100");
	Disable();
	ok = lcv_seek(0x0200) && lcv_wr(31, 0x3C) && lcv_wr(30, 0) &&
	    lcv_ready();
	Enable();
	check(ok && ramis(0x0200, 257, 0x3C) && lcm_peek(0x0301) == 0x55,
	    "fill 257");

	/* copy from R32/R33 to the update address */
	Disable();
	ok = lcv_wr(24, 0x80) && lcv_wr(32, 0x12) && lcv_wr(33, 0x34) &&
	    lcv_seek(0x2000) && lcv_wr(30, 200) && lcv_ready();
	Enable();
	for (i = 0; i < 200 && lcm_peek(0x2000 + i) == buf[i]; i++)
		;
	check(ok && i == 200 && lcm_peek(0x2000 + 200) == 0x55, "copy");
	check(lcm_stats.ls_Fill == 99 + 256 && lcm_stats.ls_Copy == 200,
	    "block counts");
	check(lcm_stats.ls_Busy > 0, "busy polls");
	check(faultless(), "faults");

	/* the model's checks */
	Disable();
	lcv_seek(0x3000);
	lcv_ready();
	lcm_select(31, 0);
	lcm_data(1, 0);
	lcm_data(2, 0);
	lcv_ready();
	Enable();
	check(lcm_stats.ls_Lost == 1 && lcm_peek(0x3000) == 2, "lost write");
	lcm_stats.ls_Lost = 0;

	lcv_wr(26, 0x0F);
	lcv_ready();
	check(lcm_stats.ls_NVIOn > 0, "access with NVI on");
	lcm_stats.ls_NVIOn = 0;

	Disable();
	lcv_seek(0x3000);
	Enable();
	check(lcm_stats.ls_Stray == 1, "Enable while busy");
	Disable();
	lcv_ready();
	Enable();
	lcm_stats.ls_Stray = 0;

	/* a chip that stays busy times out */
	Disable();
	lcv_wr(31, 0);
	lcm_hang(TRUE);
	ok = lcv_wr(31, 1);
	ok |= lcv_ready();
	ok |= lcv_push(buf, 4);
	lcm_hang(FALSE);
	lcv_ready();
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
