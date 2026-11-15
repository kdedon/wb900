/* SPDX-License-Identifier: MIT */
/*
 * The LC card's modes on the board: each mode of graphics' database for
 * 4 seconds, with its name in topaz 8 and a test pattern: the border,
 * ticks every 8 pixels (long every 64), a square by the mode's ticks
 * (square on a 4:3 screen if they are right), a 1-pixel checkerboard and
 * stripes.  Prints, per mode, the time to open the display, draw the
 * pattern and clear it, and the frame rate read from the chip's vertical
 * blank (status bit 5).  Without an LC card (the C emulator has none) it
 * only says so.
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

/* Frames a second times 100: 30 starts of the vertical blank, timed */
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

/* Pens 0 and 1: Workbench 1.x blue and white */
static ULONG palette[] = {
	2L << 16 | 0,
	0, 0x55555555, 0xAAAAAAAA,  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
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

static void mode(ULONG id, struct TextFont *tf)
{
	struct TagItem t[2];
	struct DimensionInfo dims;
	struct DisplayInfo di;
	struct NameInfo ni;
	struct View v;
	struct ViewPort vp;
	struct RasInfo ri;
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
	InitView(&v);
	InitVPort(&vp);
	ri.Next = NULL;
	ri.BitMap = bm;
	ri.RxOffset = ri.RyOffset = 0;
	vp.RasInfo = &ri;
	vp.ColorMap = cm;
	vp.DWidth = w;
	vp.DHeight = h;
	cm->VPModeID = id;
	v.ViewPort = &vp;
	LoadRGB32(&vp, palette);
	MakeVPort(&v, &vp);
	MrgCop(&v);
	LoadView(&v);

	InitRastPort(&rp);
	rp.BitMap = bm;
	if (tf)
		SetFont(&rp, tf);
	start();
	pattern(&rp, w, h, &di.Resolution, (char *)ni.Name);
	tdraw = us();
	hz = framerate();
	xKPrintf("lcmodes: %s: %d x %d, ticks %d x %d, open %ld us, pattern"
	    " %ld us, %ld.%02ld Hz\n", ni.Name, w, h, di.Resolution.x,
	    di.Resolution.y, topen, tdraw, hz / 100, hz % 100);
	check(hz != 0, "the vertical blank");
	TimeDelay(UNIT_VBLANK, 4, 0);
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
