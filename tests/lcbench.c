/* SPDX-License-Identifier: MIT */
/*
 * The LC card's costs on the board, by the E-clock, at whatever clock
 * the CPU runs: a DJNZ loop gives the clock, then each case prints its
 * time and clocks per operation beside the clocks its loop takes from
 * the Z8000's instruction timings ("est"), and the polls per operation
 * that found the chip busy.  A gap with no busy polls is the CPU's; busy
 * polls are the chip's.  Cases: register selects, seeks, R31 writes with
 * and without a poll (lost bytes read back), R30 fills and R32/R33
 * copies of 16-256 bytes, each with the display on and with most columns
 * free (R1 8, R6 2), lc.gfxdriver's port loops, and graphics' drawing
 * into a RAM bitmap, the CPU part of lcdemo's cases.  The port loops'
 * rows, and a scroll and window moves drawn through lc.gfxdriver, are
 * read back through R31 and checked against the shadow.  The registers
 * it found are put back before it gives up the card.  Without an LC card
 * (the C emulator has none) it only says so.
 */
#include <exec/exec.h>
#include <devices/timer.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <graphics/sprite.h>
#include <graphics/hrmodeid.h>
#include <proto/graphics.h>
#include <proto/alib.h>
#include <resources/framebuf.h>

#include "../libs/lc/lcport.h"

int lcb_djnz(UWORD n);
int lcb_sel(UWORD n);
int lcb_selp(UWORD n);
int lcb_r31(UWORD n, UWORD v);
int lcb_r31np(const UBYTE *src, UWORD n);
int lcb_otir(const UBYTE *src, UWORD n);
int lcb_read(UBYTE *dst, UWORD n);
int lcb_seek(UWORD n, UWORD a);
int lcb_block(UWORD n, UWORD count);
int lcb_wr(UWORD reg, UWORD val);
int lcb_ready(void);
int lcb_rd(UWORD reg);

struct GfxBase *GfxBase;
struct Device *TimerBase;
APTR FrameBufBase;

#define REPS	8

/* lc.gfxdriver's bitmap mode */
static const UBYTE regs[37] = {
	0x7E, 0x50, 0x66, 0x49, 0x20, 0x00, 0x19, 0x1D,
	0x00, 0x07, 0x20, 0x07, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x78, 0x08,
	0x00, 0x87, 0x00, 0x00, 0x10, 0x07, 0x00, 0x00,
	0x00, 0x00, 0x7D, 0x64, 0x05
};

static struct EClockVal ea, eb;
static ULONG emhz;		/* E-clock counts a microsecond */
static ULONG over;		/* counts of an empty sample */
static ULONG nsdjnz;		/* ns of a DJNZ pass, 11 clocks */
static ULONG sum, busy;
static UBYTE buf[256], back[256];
static struct LCPort lp;
static int fails;
static UBYTE found[37];

static void t0(void)
{
	Disable();
	lp.lp_Budget = LCP_PIECE;
	ReadEClock(&ea);
}

static void t1(void)
{
	ReadEClock(&eb);
	lcb_ready();
	Enable();
	sum += eb.ev_lo - ea.ev_lo - over;
}

/*
 * sum over n operations of what, in mode; est the loop's clocks, 0 for a
 * case the chip's time decides
 */
static void show(char *mode, char *what, ULONG n, ULONG est)
{
	ULONG ns = sum * 1000 / emhz / n, c10 = ns * 110 / nsdjnz;

	xKPrintf("lcbench: %s%s: %ld ns, %ld.%ld clocks (est %ld), busy polls %ld/100\n",
	    mode, what, ns, c10 / 10, c10 % 10, est, busy * 100 / n);
	sum = busy = 0;
}

static void seek(UWORD a)
{
	lcb_wr(18, a >> 8);
	lcb_wr(19, a & 0xFF);
}

/* R0-R36 but the light pen, address, count and data registers */
static BOOL dreg(UWORD i)
{
	return (i < 16 || i > 19) && (i < 30 || i > 33);
}

/* The display registers, R28 first */
static void setregs(const UBYTE *r)
{
	UWORD i;

	Disable();
	lcb_wr(28, r[28]);
	for (i = 0; i < 37; i++)
		if (dreg(i) && i != 28)
			lcb_wr(i, r[i]);
	lcb_ready();
	Enable();
}

/*
 * VRAM from a is p's n bytes, read through R31 a block at a time; the
 * update address is put back, as the port layer keeps it
 */
static BOOL same(UWORD a, const UBYTE *p, UWORD n, char *what)
{
	UWORD ua, i, k;

	Disable();
	lcb_ready();
	ua = lcb_rd(18) << 8 | lcb_rd(19);
	seek(a);
	lcb_ready();
	Enable();
	for (; n > 0; n -= k, a += k, p += k) {
		k = n < 256 ? n : 256;
		Disable();
		lcb_read(back, k);
		lcb_ready();
		Enable();
		for (i = 0; i < k && back[i] == p[i]; i++)
			;
		if (i < k)
			break;
	}
	Disable();
	seek(ua);
	lcb_ready();
	Enable();
	if (n == 0)
		return TRUE;
	xKPrintf("FAIL lcbench: %s: VRAM %x is %x, shadow %x\n", what, a + i,
	    back[i], p[i]);
	fails++;
	return FALSE;
}

/* Lost bytes of n written unpolled from 0x4000 */
static UWORD lost(UWORD n)
{
	UWORD i, k = 0;

	Disable();
	seek(0x4000);
	lcb_read(back, n);
	lcb_ready();
	Enable();
	for (i = 0; i < n; i++)
		k += back[i] != buf[i];
	return k;
}

/* The registers the cases depend on, unused bits masked */
static void regs24(char *mode)
{
	xKPrintf("lcbench: %sR1 %x R6 %x R24 %x R25 %x R28 %x R36 %x\n", mode,
	    lcb_rd(1), lcb_rd(6), lcb_rd(24), lcb_rd(25), lcb_rd(28) & 0xF0,
	    lcb_rd(36) & 0x0F);
}

static void chip(char *mode)
{
	regs24(mode);
	UWORD i, k;

	for (i = 0; i < REPS; i++) {
		Disable();
		seek(0x4000);
		lcb_ready();
		Enable();
		t0();
		busy += lcb_r31(256, 0x55);
		t1();
	}
	show(mode, "R31 write, polled", REPS * 256L, 41);

	for (i = k = 0; i < REPS; i++) {
		buf[0] = i;
		Disable();
		seek(0x4000);
		lcb_ready();
		Enable();
		t0();
		lcb_r31np(buf, 256);
		t1();
		k += lost(256);
	}
	show(mode, "R31 write, outib unpolled", REPS * 256L, 27);
	xKPrintf("lcbench: %slost %d of %d\n", mode, k, REPS * 256);

	for (i = k = 0; i < REPS; i++) {
		buf[0] = i + 100;
		Disable();
		seek(0x4000);
		lcb_ready();
		Enable();
		t0();
		lcb_otir(buf, 256);
		t1();
		k += lost(256);
	}
	show(mode, "R31 write, otirb", REPS * 256L, 10);
	xKPrintf("lcbench: %slost %d of %d\n", mode, k, REPS * 256);

	for (k = 256; k >= 16; k >>= 2) {
		for (i = 0; i < REPS; i++) {
			Disable();
			lcb_wr(24, 0x00);
			seek(0x4000);
			lcb_wr(31, 0xAA);
			lcb_ready();
			Enable();
			t0();
			busy += lcb_block(4096 / k, k & 0xFF);
			t1();
		}
		xKPrintf("lcbench: %sR30 fill of %d bytes, a block:\n", mode, k);
		show(mode, "  fill", REPS * (4096L / k), 0);
	}
	/* the copies' source: buf at 0x6000 */
	Disable();
	lcb_wr(24, 0x00);
	seek(0x6000);
	for (i = 0; i < 256; i++)
		lcb_r31(1, buf[i]);
	lcb_ready();
	Enable();
	for (k = 256; k >= 16; k >>= 2) {
		for (i = 0; i < REPS; i++) {
			Disable();
			lcb_wr(24, 0x80);
			lcb_wr(32, 0x60);
			lcb_wr(33, 0x00);
			seek(0x4000);
			lcb_ready();
			Enable();
			t0();
			busy += lcb_block(4096 / k, k & 0xFF);
			t1();
		}
		xKPrintf("lcbench: %sR32/R33 copy of %d bytes, a block:\n", mode, k);
		show(mode, "  copy", REPS * (4096L / k), 0);
	}
	regs24(mode);
	k = lost(256);
	xKPrintf("lcbench: %scopied bytes wrong: %d of 256\n", mode, k);
	if (k != 0) {
		xKPrintf("FAIL lcbench: %sR30 copies are not copies\n", mode);
		fails++;
	}
	Disable();
	lcb_wr(24, 0x00);
	lcb_ready();
	Enable();
}

static void port(void)
{
	UWORD i;

	lp.lp_Shadow = AllocMem(16000, MEMF_CLEAR);
	if (lp.lp_Shadow == NULL)
		return;
	lp.lp_BPR = 80;
	lp.lp_OK = TRUE;
	lp.lp_UA = lp.lp_BA = LCP_NOADDR;
	lp.lp_R24 = 0;
	lp.lp_Y0 = LCP_ROWS;
	lp.lp_Y1 = -1;
	for (i = 0; i < LCP_ROWS; i++) {
		lp.lp_Lo[i] = 0xFF;
		lp.lp_Hi[i] = 0;
		lp.lp_Cx[i] = 0;
	}
	for (i = 0; i < REPS; i++) {
		t0();
		lcv_push(&lp, i * 80, 80);
		t1();
	}
	show("", "lcv_push, a byte of 80", REPS * 80L, 47);
	/* as lc.gfxdriver, the shadow drawn first: rows 1-20 filled, then
	 * copied to rows 41-60 */
	for (i = 80; i < 21 * 80; i++)
		if (i % 80 != 0 && i % 80 != 79)
			lp.lp_Shadow[i] = 0xAA;
	for (i = 0; i < REPS; i++) {
		t0();
		lcv_fill(&lp, 81, 0xAA, 78, 20, 3);
		t1();
	}
	show("", "lcv_fill, a row of 78 and 2 edges", REPS * 20L, 0);
	same(80, lp.lp_Shadow + 80, 20 * 80, "lcv_fill's rows");
	CopyMem(lp.lp_Shadow + 80, lp.lp_Shadow + 41 * 80, 20 * 80);
	for (i = 0; i < REPS; i++) {
		t0();
		lcv_copy(&lp, 81 + 40 * 80, 81, 78, 20, 80, 3);
		t1();
	}
	show("", "lcv_copy, a row of 78 and 2 edges", REPS * 20L, 0);
	same(41 * 80, lp.lp_Shadow + 41 * 80, 20 * 80, "lcv_copy's rows");
	for (i = 0; i < REPS; i++) {
		t0();
		lcv_mark(&lp, 0, 99, 10, 13, TRUE);
		t1();
		t0();
		lcv_flush(&lp);
		t1();
	}
	show("", "lcv_mark and lcv_flush, a row of 4", REPS * 100L, 0);
	for (i = 0; i < REPS; i++) {
		t0();
		lcv_mark(&lp, 0, 99, 10, 13, TRUE);
		lcv_mark(&lp, 0, 99, 40, 43, TRUE);
		lcv_flush(&lp);
		t1();
	}
	show("", "lcv_mark and lcv_flush, a row of 2 x 4", REPS * 100L, 0);
	FreeMem(lp.lp_Shadow, 16000);
}

static UWORD arrow[2 + 2 * 11 + 2];

static void makearrow(void)
{
	UWORD r, edge, in;

	for (r = 0; r < 11; r++) {
		if (r == 10) {
			edge = 0xFFE0;
			in = 0;
		} else {
			edge = 0x8000 | (0x8000 >> r);
			in = r > 1 ? (UWORD)(0xFFFF << (16 - r)) >> 1 &
			    ~(0x8000 >> r) : 0;
		}
		arrow[2 + 2 * r] = edge;
		arrow[3 + 2 * r] = in;
	}
}

static char line80[] =
    "wb900 on the LC card: 640x200 mono, a RAM shadow pushed through R31  0123456789";

/* VRAM is the display bitmap after what */
static void vram(struct BitMap *bm, char *what)
{
	if (same(0, bm->Planes[0], bm->BytesPerRow * bm->Rows, what))
		xKPrintf("lcbench: %s: VRAM is the shadow\n", what);
}

/*
 * lc.gfxdriver's block copies drawn by graphics: scrolls up and down,
 * full width and in a window, and aligned window moves, one right within its
 * rows (through scratch VRAM)
 */
static void display(void)
{
	struct TagItem t[2];
	struct BitMap *bm;
	struct RastPort rp;
	struct TextAttr ta;
	struct TextFont *tf;
	WORD i;

	t[0].ti_Tag = BMATags_DisplayID;
	t[0].ti_Data = LCHIRES_ID;
	t[1].ti_Tag = TAG_DONE;
	bm = AllocBitMap(640, 200, 1, BMF_CLEAR | BMF_DISPLAYABLE |
	    BMF_RTGTAGS | BMF_RTGCHECK | BMF_FRIENDISTAG, (struct BitMap *)t);
	ta.ta_Name = "topaz.font";
	ta.ta_YSize = 8;
	ta.ta_Style = ta.ta_Flags = 0;
	tf = OpenFont(&ta);
	if (bm == NULL || tf == NULL) {
		KPutStr("FAIL lcbench: the display bitmap\n");
		fails++;
		if (bm)
			FreeBitMap(bm);
		return;
	}
	InitRastPort(&rp);
	rp.BitMap = bm;
	SetFont(&rp, tf);
	SetAPen(&rp, 1);
	SetBPen(&rp, 0);
	SetDrMd(&rp, JAM2);
	for (i = 0; i < 25; i++) {
		Move(&rp, i, 8 * i + rp.TxBaseline);
		Text(&rp, line80 + i, 78 - i);
	}
	vram(bm, "text");
	ScrollRaster(&rp, 0, 8, 0, 0, 639, 199);
	vram(bm, "scroll up, full width");
	ScrollRaster(&rp, 0, -24, 0, 0, 639, 199);
	vram(bm, "scroll down, full width");
	ScrollRaster(&rp, 0, 8, 160, 10, 479, 189);
	vram(bm, "scroll up, 40-byte window");
	ScrollRaster(&rp, 0, -8, 163, 10, 470, 189);
	vram(bm, "scroll down, window with edges");
	BltBitMap(bm, 203, 90, bm, 107, 50, 300, 100, 0xC0, 0xFF, NULL);
	vram(bm, "window move up left");
	BltBitMap(bm, 107, 50, bm, 131, 50, 300, 100, 0xC0, 0xFF, NULL);
	vram(bm, "window move right");
	CloseFont(tf);
	FreeBitMap(bm);
}

static void ram(char *what)
{
	ReadEClock(&eb);
	xKPrintf("lcbench: RAM bitmap, %s: %ld us\n", what,
	    (eb.ev_lo - ea.ev_lo) / emhz);
	ReadEClock(&ea);
}

/* graphics' share of lcdemo's cases, drawing a RAM bitmap */
static void drawing(void)
{
	static struct TagItem old[] = {
		{ SPRITEA_OldDataFormat, TRUE }, { TAG_DONE, 0 }
	};
	struct BitMap *bm;
	struct RastPort rp;
	struct TextAttr ta;
	struct TextFont *tf;
	struct ViewPort vp;
	struct RasInfo ri;
	struct ExtSprite *es;
	WORD i;

	bm = AllocBitMap(640, 200, 1, BMF_CLEAR, NULL);
	ta.ta_Name = "topaz.font";
	ta.ta_YSize = 8;
	ta.ta_Style = ta.ta_Flags = 0;
	tf = OpenFont(&ta);
	if (bm == NULL || tf == NULL)
		return;
	InitRastPort(&rp);
	rp.BitMap = bm;
	SetFont(&rp, tf);
	ReadEClock(&ea);
	SetRast(&rp, 1);
	ram("clear");
	SetRast(&rp, 0);
	SetAPen(&rp, 1);
	SetBPen(&rp, 0);
	SetDrMd(&rp, JAM2);
	ReadEClock(&ea);
	Move(&rp, 0, 4 + rp.TxBaseline);
	Text(&rp, line80, 80);
	ram("text line, 80 chars");
	ScrollRaster(&rp, 0, 8, 0, 0, 639, 199);
	ram("scroll, full width, 190 rows");
	ScrollRaster(&rp, 0, 8, 160, 10, 479, 189);
	ram("scroll, 40-byte window, 180 rows");
	BltBitMap(bm, 100, 10, bm, 203, 90, 300, 100, 0xC0, 0xFF, NULL);
	ram("window move 300x100, unaligned");
	BltBitMap(bm, 203, 90, bm, 107, 50, 300, 100, 0xC0, 0xFF, NULL);
	ram("window move 300x100, aligned");
	InitVPort(&vp);
	ri.Next = NULL;
	ri.BitMap = bm;
	ri.RxOffset = ri.RyOffset = 0;
	vp.RasInfo = &ri;
	makearrow();
	es = AllocSpriteDataA((struct BitMap *)arrow, old);
	if (es && ChangeExtSpriteA(&vp, NULL, es, NULL)) {
		ReadEClock(&ea);
		for (i = 0; i < 20; i++)
			MoveSprite(&vp, &es->es_SimpleSprite, 300 + 3 * i, 100 + i);
		ram("20 pointer moves");
		ChangeExtSpriteA(&vp, es, NULL, NULL);
	}
	if (es)
		FreeSpriteData(es);
	CloseFont(tf);
	FreeBitMap(bm);
}

int main(void)
{
	struct timerequest *tr;
	struct MsgPort *mp;
	ULONG rate;
	UWORD i;

	FrameBufBase = OpenResource(FRAMEBUFNAME);
	if (FrameBufBase == NULL || GetFBInfo()->fbi_Card != FBC_LC) {
		KPutStr("lcbench: no LC card\nPASS lcbench\n");
		return 0;
	}
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 45L);
	mp = CreateMsgPort();
	tr = (struct timerequest *)CreateIORequest(mp, sizeof(*tr));
	if (GfxBase == NULL || tr == NULL ||
	    OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)tr, 0) != 0 ||
	    AllocFB("lcbench") != NULL) {
		KPutStr("FAIL lcbench: setup\n");
		return 1;
	}
	TimerBase = tr->tr_node.io_Device;
	rate = ReadEClock(&ea);
	emhz = rate / 1000000;

	Disable();
	lcb_ready();
	for (i = 0; i < 37; i++)
		if (dreg(i))
			found[i] = lcb_rd(i);
	Enable();
	setregs(regs);
	Disable();
	lcb_wr(26, 0xF2);
	lcb_ready();
	Enable();
	for (i = 0; i < 256; i++)
		buf[i] = i * 7 + 3;

	for (i = 0; i < REPS; i++) {
		t0();
		t1();
	}
	over = sum / REPS;
	sum = 0;
	for (i = 0; i < REPS; i++) {
		t0();
		lcb_djnz(2000);
		t1();
	}
	nsdjnz = sum * 1000 / emhz / (REPS * 2000L);
	xKPrintf("lcbench: E-clock %ld Hz, an empty sample %ld counts\n", rate,
	    over);
	xKPrintf("lcbench: CPU %ld kHz (DJNZ, 11 clocks: %ld ns)\n",
	    11000000L / nsdjnz, nsdjnz);
	sum = 0;

	for (i = 0; i < REPS; i++) {
		t0();
		busy += lcb_sel(200);
		t1();
	}
	show("", "register select", REPS * 200L, 21);
	for (i = 0; i < REPS; i++) {
		t0();
		busy += lcb_selp(200);
		t1();
	}
	show("", "register select and poll", REPS * 200L, 41);
	for (i = 0; i < REPS; i++) {
		t0();
		busy += lcb_seek(100, 0x4000);
		t1();
	}
	show("", "seek, R18 and R19", REPS * 100L, 151);

	chip("display on: ");
	Disable();
	lcb_wr(1, 8);
	lcb_wr(6, 2);
	Enable();
	chip("R1 8, R6 2: ");
	Disable();
	lcb_wr(1, regs[1]);
	lcb_wr(6, regs[6]);
	Enable();

	port();
	setregs(found);
	FreeFB();
	display();
	drawing();

	CloseDevice((struct IORequest *)tr);
	if (fails != 0)
		return 1;
	KPutStr("PASS lcbench\n");
	return 0;
}
