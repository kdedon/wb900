/* SPDX-License-Identifier: MIT */
/*
 * The LC card's modes on the board: each mode of graphics' database for
 * 4 seconds, with its name in topaz 8 and a test pattern: the border,
 * ticks every 8 pixels (long every 64), a square by the mode's ticks
 * (square on a 4:3 screen if they are right), a 1-pixel checkerboard and
 * stripes.  Then a second bitmap of the mode, black on white with a
 * box, and the two take turns on the display, 1 second each.  Prints,
 * per mode, the time to open the display, draw the pattern and clear it,
 * the field rate read from the chip's vertical blank (status bit 5), and
 * each turn's time: a register write once both bitmaps are in VRAM.
 * Each bitmap shown is read back through R31 from where the registers
 * fetch it and summed against the shadow.
 * Without an LC card it only says so.
 */
#include <exec/exec.h>
#include <devices/timer.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <proto/graphics.h>
#include <proto/alib.h>
#include <resources/framebuf.h>

struct GfxBase *GfxBase;
struct Device *TimerBase;
APTR FrameBufBase;

extern int inb();
int lcb_wr(UWORD reg, UWORD val);
int lcb_ready(void);
int lcb_rd(UWORD reg);
int lcb_read(UBYTE *dst, UWORD n);

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL lcmodes: %s\n", what);
		fails++;
	}
}

#define EXTTAGS	(BMF_RTGTAGS | BMF_RTGCHECK | BMF_FRIENDISTAG)
#define LCSTATUS	0x181

static struct EClockVal t0;
static ULONG rate;

static void start(void)
{
	rate = ReadEClock(&t0);
}

static ULONG us(void)
{
	struct EClockVal t1;

	ReadEClock(&t1);
	return (t1.ev_lo - t0.ev_lo) * 1000 / (rate / 1000);
}

/* Fields a second times 100: 30 starts of the vertical blank, timed */
static ULONG framerate(void)
{
	UWORD n, k;
	ULONG t;

	Forbid();
	for (n = 0; n <= 30; n++) {
		for (k = 0; k < 60000 && (inb(LCSTATUS) & 0x20); k++)
			;
		for (k = 0; k < 60000 && !(inb(LCSTATUS) & 0x20); k++)
			;
		if (k == 60000)
			break;
		if (n == 0)
			start();
	}
	t = us();
	Permit();
	return n > 30 ? 30L * 10000000L / (t / 10) : 0;
}

/* A sum that shifted or swapped bytes change */
static UWORD sum(UWORD s, const UBYTE *p, UWORD n)
{
	for (; n > 0; n--)
		s = (s << 1 | s >> 15) + *p++;
	return s;
}

/*
 * The lines the registers show, read back a line at a time, against bm:
 * rows above VSYNC (shown rows count from 1), interlaced fields one after
 * the other.  The update address is put back, as the port layer keeps it.
 */
static void vram(struct BitMap *bm, char *name, char *what)
{
	static UBYTE back[80];
	UWORD ua, a, start, bpr, rows, lines, even, n, k, vs = 0, ss;
	static const UBYTE regs[] = { 1, 6, 7, 8, 9, 12, 13, 18, 19, 27 };
	UBYTE r[28];
	BOOL lace;

	Forbid();
	Disable();
	lcb_ready();
	for (n = 0; n < sizeof(regs); n++)
		r[regs[n]] = lcb_rd(regs[n]);
	ua = r[18] << 8 | r[19];
	Enable();
	start = r[12] << 8 | r[13];
	bpr = r[1] < 80 ? r[1] : 80;
	rows = r[7] != 0 && r[7] <= r[6] ? r[7] - 1 : r[6];
	lines = rows * ((r[9] & 0x1F) + 1);
	even = r[6] * ((r[9] & 0x1F) + 1) / 2;
	lace = (r[8] & 3) == 3;
	for (n = 0; n < lines; n++) {
		a = lace ? (n & 1 ? even + n / 2 : n / 2) : n;
		a = start + a * (r[1] + r[27]);
		/* 40 bytes at a time, about 1 ms with interrupts off */
		for (k = 0; k < bpr; k += 40) {
			Disable();
			if (k == 0) {
				lcb_wr(18, a >> 8);
				lcb_wr(19, a & 0xFF);
			}
			lcb_read(back + k, bpr - k < 40 ? bpr - k : 40);
			lcb_ready();
			Enable();
		}
		vs = sum(vs, back, bpr);
	}
	Disable();
	lcb_wr(18, ua >> 8);
	lcb_wr(19, ua & 0xFF);
	lcb_ready();
	Enable();
	Permit();
	ss = sum(0, bm->Planes[0], bm->BytesPerRow * bm->Rows);
	xKPrintf("lcmodes: %s: %s: VRAM %u x %u sum %04x, shadow %u x %u"
	    " sum %04x\n", name, what, bpr, lines, vs, bm->BytesPerRow,
	    bm->Rows, ss);
	check(bpr == bm->BytesPerRow && lines == bm->Rows && vs == ss,
	    "VRAM is not the shadow");
}

/* Pens 0 and 1: Workbench 1.x blue and white */
static ULONG palette[] = {
	2L << 16 | 0,
	0, 0x55555555, 0xAAAAAAAA,  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0
};

/* Black on white */
static ULONG paper[] = {
	2L << 16 | 0,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,  0, 0, 0,
	0
};

static UWORD checks[2] = { 0xAAAA, 0x5555 };

static void pattern(struct RastPort *rp, WORD w, WORD h, Point *res,
    char *name)
{
	WORD x, y, sw, sh;

	SetAPen(rp, 1);
	Move(rp, 0, 0);
	Draw(rp, w - 1, 0);
	Draw(rp, w - 1, h - 1);
	Draw(rp, 0, h - 1);
	Draw(rp, 0, 0);
	for (x = 8; x < w; x += 8) {
		Move(rp, x, 1);
		Draw(rp, x, x % 64 ? 3 : 7);
	}
	for (y = 8; y < h; y += 8) {
		Move(rp, 1, y);
		Draw(rp, y % 64 ? 3 : 7, y);
	}
	sh = h / 2;
	sw = (LONG)sh * res->y / res->x;
	x = (w - sw) / 2;
	y = (h - sh) / 2;
	Move(rp, x, y);
	Draw(rp, x + sw - 1, y);
	Draw(rp, x + sw - 1, y + sh - 1);
	Draw(rp, x, y + sh - 1);
	Draw(rp, x, y);
	Move(rp, x, y);
	Draw(rp, x + sw - 1, y + sh - 1);
	Move(rp, x + sw - 1, y);
	Draw(rp, x, y + sh - 1);
	SetAfPt(rp, checks, 1);
	RectFill(rp, 16, h - 40, 79, h - 17);
	SetAfPt(rp, NULL, 0);
	for (x = w - 80; x < w - 16; x += 2) {
		Move(rp, x, h - 40);
		Draw(rp, x, h - 17);
	}
	SetBPen(rp, 0);
	SetDrMd(rp, JAM2);
	for (x = 0; name[x]; x++)
		;
	Move(rp, (w - 8 * x) / 2, 16 + rp->TxBaseline);
	Text(rp, name, x);
}

static struct View v[2];
static struct ViewPort vp[2];
static struct RasInfo ri[2];

/* View k showing bm in the colours of pal */
static void view(UWORD k, struct BitMap *bm, struct ColorMap *cm, ULONG id,
    ULONG *pal)
{
	InitView(&v[k]);
	InitVPort(&vp[k]);
	ri[k].Next = NULL;
	ri[k].BitMap = bm;
	ri[k].RxOffset = ri[k].RyOffset = 0;
	vp[k].RasInfo = &ri[k];
	vp[k].ColorMap = cm;
	vp[k].DWidth = GetBitMapAttr(bm, BMA_WIDTH);
	vp[k].DHeight = bm->Rows;
	cm->VPModeID = id;
	v[k].ViewPort = &vp[k];
	LoadRGB32(&vp[k], pal);
	MakeVPort(&v[k], &vp[k]);
	MrgCop(&v[k]);
}

/* LoadView() of view k, timed, then held 1 second */
static ULONG turn(UWORD k)
{
	ULONG t;

	start();
	LoadView(&v[k]);
	t = us();
	TimeDelay(UNIT_VBLANK, 1, 0);
	return t;
}

/* A second bitmap of the mode, and turns with the first on the display */
static void turns(ULONG id, struct BitMap *bm, char *name)
{
	struct TagItem t[2];
	struct RastPort rp;
	struct ColorMap *cm;
	struct BitMap *b2;
	WORD w = GetBitMapAttr(bm, BMA_WIDTH), h = bm->Rows;
	ULONG t1, t2, t3;

	t[0].ti_Tag = BMATags_DisplayID;
	t[0].ti_Data = id;
	t[1].ti_Tag = TAG_DONE;
	cm = GetColorMap(32);
	b2 = AllocBitMap(w, h, 1, BMF_CLEAR | BMF_DISPLAYABLE | EXTTAGS,
	    (struct BitMap *)t);
	check(b2 != NULL && cm != NULL, "a second bitmap");
	if (b2 && cm) {
		InitRastPort(&rp);
		rp.BitMap = b2;
		SetAPen(&rp, 1);
		RectFill(&rp, w / 4, h / 4, w / 2, h / 2);
		view(1, b2, cm, id, paper);
		t1 = turn(1);
		t2 = turn(0);
		t3 = turn(1);
		vram(b2, name, "box");
		xKPrintf("lcmodes: %s: turns %ld us, %ld us, %ld us\n", name, t1,
		    t2, t3);
		turn(0);
	}
	if (b2)
		FreeBitMap(b2);
	if (cm)
		FreeColorMap(cm);
}

static void mode(ULONG id, struct TextFont *tf)
{
	struct TagItem t[2];
	struct DimensionInfo dims;
	struct DisplayInfo di;
	struct NameInfo ni;
	struct RastPort rp;
	struct BitMap *bm;
	struct ColorMap *cm;
	ULONG topen, tdraw, tclear, hz;
	WORD w, h;

	if (GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS,
	    id) == 0 || GetDisplayInfoData(NULL, (UBYTE *)&di, sizeof(di),
	    DTAG_DISP, id) == 0 || GetDisplayInfoData(NULL, (UBYTE *)&ni,
	    sizeof(ni), DTAG_NAME, id) == 0) {
		check(FALSE, "GetDisplayInfoData");
		return;
	}
	w = dims.Nominal.MaxX + 1;
	h = dims.Nominal.MaxY + 1;
	t[0].ti_Tag = BMATags_DisplayID;
	t[0].ti_Data = id;
	t[1].ti_Tag = TAG_DONE;
	cm = GetColorMap(32);
	start();
	bm = AllocBitMap(w, h, 1, BMF_CLEAR | BMF_DISPLAYABLE | EXTTAGS,
	    (struct BitMap *)t);
	topen = us();
	check(bm != NULL && cm != NULL, ni.Name);
	if (bm == NULL || cm == NULL) {
		if (cm)
			FreeColorMap(cm);
		return;
	}
	view(0, bm, cm, id, palette);
	LoadView(&v[0]);

	InitRastPort(&rp);
	rp.BitMap = bm;
	if (tf)
		SetFont(&rp, tf);
	start();
	pattern(&rp, w, h, &di.Resolution, (char *)ni.Name);
	tdraw = us();
	vram(bm, (char *)ni.Name, "pattern");
	hz = framerate();
	xKPrintf("lcmodes: %s: %d x %d, ticks %d x %d, open %ld us, pattern"
	    " %ld us, %ld.%02ld Hz\n", ni.Name, w, h, di.Resolution.x,
	    di.Resolution.y, topen, tdraw, hz / 100, hz % 100);
	check(hz != 0, "the vertical blank");
	TimeDelay(UNIT_VBLANK, 4, 0);
	turns(id, bm, (char *)ni.Name);
	start();
	SetRast(&rp, 0);
	tclear = us();
	xKPrintf("lcmodes: %s: clear %ld us\n", ni.Name, tclear);
	LoadView(NULL);
	FreeBitMap(bm);
	FreeColorMap(cm);
}

int main(void)
{
	struct timerequest *tr;
	struct MsgPort *port;
	struct TextAttr topaz8;
	struct TextFont *tf;
	ULONG id, stray;

	FrameBufBase = OpenResource(FRAMEBUFNAME);
	if (FrameBufBase == NULL || GetFBInfo()->fbi_Card != FBC_LC) {
		KPutStr("lcmodes: no LC card\nPASS lcmodes\n");
		return 0;
	}
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 45L);
	port = CreateMsgPort();
	tr = (struct timerequest *)CreateIORequest(port, sizeof(*tr));
	if (GfxBase == NULL || tr == NULL ||
	    OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)tr, 0) != 0) {
		KPutStr("FAIL lcmodes: setup\n");
		return 1;
	}
	TimerBase = tr->tr_node.io_Device;
	topaz8.ta_Name = "topaz.font";
	topaz8.ta_YSize = 8;
	topaz8.ta_Style = 0;
	topaz8.ta_Flags = 0;
	tf = OpenFont(&topaz8);
	stray = IntStray(INTV_NVI);
	for (id = NextDisplayInfo(INVALID_ID); id != INVALID_ID;
	    id = NextDisplayInfo(id))
		mode(id, tf);
	check(IntStray(INTV_NVI) == stray, "no stray NVI");
	check(AllocFB("lcmodes") == NULL, "the display is back");
	FreeFB();
	if (tf)
		CloseFont(tf);
	CloseDevice((struct IORequest *)tr);
	if (fails != 0)
		return 1;
	KPutStr("PASS lcmodes\n");
	return 0;
}
