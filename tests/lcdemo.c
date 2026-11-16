/* SPDX-License-Identifier: MIT */
/*
 * The LC card on the board: a picture drawn by graphics.library through
 * lc.gfxdriver, the time of the costed cases by the E-clock (clear, a
 * text line, scrolls, window moves, pointer moves, opening a window on
 * the Workbench screen), no stray NVI, and the console back once the
 * display is given up.  Each picture stays up for 3 seconds.  Without an
 * LC card it only says so.
 */
#include <exec/exec.h>
#include <exec/interrupts.h>
#include <devices/timer.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/view.h>
#include <graphics/sprite.h>
#include <graphics/hrmodeid.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/alib.h>
#include <resources/framebuf.h>

struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Device *TimerBase;
APTR FrameBufBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL lcdemo: %s\n", what);
		fails++;
	}
}

#define EXTTAGS	(BMF_RTGTAGS | BMF_RTGCHECK | BMF_FRIENDISTAG)

static struct EClockVal t0;
static ULONG rate;

static void start(void)
{
	rate = ReadEClock(&t0);
}

/* Microseconds since start(), beside the expected figure */
static void took(char *what, ULONG plan)
{
	struct EClockVal t1;
	ULONG us;

	ReadEClock(&t1);
	us = (t1.ev_lo - t0.ev_lo) * 1000 / (rate / 1000);
	xKPrintf("lcdemo: %s: %ld us (plan %ld)\n", what, us, plan);
}

/* Pens 0 and 1: Workbench 1.x blue and white */
static ULONG palette[] = {
	2L << 16 | 0,
	0, 0x55555555, 0xAAAAAAAA,  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0
};

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

static UWORD checks[2] = { 0xAAAA, 0x5555 };

static void graphics(void)
{
	static struct TagItem old[] = {
		{ SPRITEA_OldDataFormat, TRUE }, { TAG_DONE, 0 }
	};
	struct TagItem t[2];
	struct View v;
	struct ViewPort vp;
	struct RasInfo ri;
	struct RastPort rp;
	struct BitMap *bm;
	struct ColorMap *cm;
	struct ExtSprite *es;
	struct TextAttr topaz8;
	struct TextFont *tf;
	WORD i;

	t[0].ti_Tag = BMATags_DisplayID;
	t[0].ti_Data = LCHIRES_ID;
	t[1].ti_Tag = TAG_DONE;
	bm = AllocBitMap(640, 200, 1, BMF_CLEAR | BMF_DISPLAYABLE | EXTTAGS,
	    (struct BitMap *)t);
	cm = GetColorMap(32);
	check(bm != NULL && cm != NULL, "the display bitmap");
	if (bm == NULL || cm == NULL)
		return;
	InitView(&v);
	InitVPort(&vp);
	ri.Next = NULL;
	ri.BitMap = bm;
	ri.RxOffset = ri.RyOffset = 0;
	vp.RasInfo = &ri;
	vp.ColorMap = cm;
	vp.DWidth = 640;
	vp.DHeight = 200;
	cm->VPModeID = LCHIRES_ID;
	v.ViewPort = &vp;
	LoadRGB32(&vp, palette);
	MakeVPort(&v, &vp);
	MrgCop(&v);
	LoadView(&v);

	InitRastPort(&rp);
	rp.BitMap = bm;
	topaz8.ta_Name = "topaz.font";
	topaz8.ta_YSize = 8;
	topaz8.ta_Style = 0;
	topaz8.ta_Flags = 0;
	tf = OpenFont(&topaz8);
	if (tf)
		SetFont(&rp, tf);

	start();
	SetRast(&rp, 1);
	took("clear", 17000);
	SetRast(&rp, 0);
	SetAPen(&rp, 1);
	SetBPen(&rp, 0);
	SetDrMd(&rp, JAM2);
	start();
	Move(&rp, 0, 4 + rp.TxBaseline);
	Text(&rp, line80, 80);
	took("text line, 80 chars", 10000);
	for (i = 1; i < 24; i++) {
		Move(&rp, 0, 4 + 8 * i + rp.TxBaseline);
		Text(&rp, line80 + i, 80 - i);
	}
	start();
	ScrollRaster(&rp, 0, 8, 0, 0, 639, 199);
	took("scroll, full width, 190 rows", 32000);
	start();
	ScrollRaster(&rp, 0, 8, 160, 10, 479, 189);
	took("scroll, 40-byte window, 180 rows", 30000);
	start();
	BltBitMap(bm, 100, 10, bm, 203, 90, 300, 100, 0xC0, 0xFF, NULL);
	took("window move 300x100, unaligned", 60000);
	start();
	BltBitMap(bm, 203, 90, bm, 107, 50, 300, 100, 0xC0, 0xFF, NULL);
	took("window move 300x100, aligned", 8000);
	SetAfPt(&rp, checks, 1);
	RectFill(&rp, 400, 150, 620, 190);
	SetAfPt(&rp, NULL, 0);
	Move(&rp, 0, 199);
	Draw(&rp, 639, 0);

	makearrow();
	es = AllocSpriteDataA((struct BitMap *)arrow, old);
	if (es && ChangeExtSpriteA(&vp, NULL, es, NULL)) {
		start();
		for (i = 0; i < 20; i++)
			MoveSprite(&vp, &es->es_SimpleSprite, 300 + 3 * i, 100 + i);
		took("20 pointer moves", 20 * 1100);
		TimeDelay(UNIT_VBLANK, 3, 0);
		ChangeExtSpriteA(&vp, es, NULL, NULL);
	} else
		check(FALSE, "the pointer");
	if (es)
		FreeSpriteData(es);
	LoadView(NULL);
	FreeBitMap(bm);
	FreeColorMap(cm);
	if (tf)
		CloseFont(tf);
}

static void intuition(void)
{
	struct Screen *wb;
	struct Window *w;
	struct TagItem t[10];

	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	check(IntuitionBase != NULL, "intuition.library");
	if (IntuitionBase == NULL)
		return;
	start();
	check(OpenWorkBench() != 0, "OpenWorkBench");
	took("OpenWorkBench", 0);
	wb = LockPubScreen("Workbench");
	if (wb) {
		t[0].ti_Tag = WA_Left;		t[0].ti_Data = 40;
		t[1].ti_Tag = WA_Top;		t[1].ti_Data = 30;
		t[2].ti_Tag = WA_Width;		t[2].ti_Data = 320;
		t[3].ti_Tag = WA_Height;	t[3].ti_Data = 120;
		t[4].ti_Tag = WA_Title;		t[4].ti_Data = (ULONG)"lcdemo";
		t[5].ti_Tag = WA_DragBar;	t[5].ti_Data = TRUE;
		t[6].ti_Tag = WA_DepthGadget;	t[6].ti_Data = TRUE;
		t[7].ti_Tag = WA_CloseGadget;	t[7].ti_Data = TRUE;
		t[8].ti_Tag = WA_PubScreen;	t[8].ti_Data = (ULONG)wb;
		t[9].ti_Tag = TAG_DONE;
		start();
		w = OpenWindowTagList(NULL, t);
		took("window open 320x120", 50000);
		check(w != NULL, "OpenWindow");
		TimeDelay(UNIT_VBLANK, 3, 0);
		if (w)
			CloseWindow(w);
		UnlockPubScreen(NULL, wb);
	}
	check(CloseWorkBench(), "CloseWorkBench");
	CloseLibrary((struct Library *)IntuitionBase);
}

int main(void)
{
	struct timerequest *tr;
	struct MsgPort *port;
	ULONG stray;

	FrameBufBase = OpenResource(FRAMEBUFNAME);
	if (FrameBufBase == NULL || GetFBInfo()->fbi_Card != FBC_LC) {
		KPutStr("lcdemo: no LC card\nPASS lcdemo\n");
		return 0;
	}
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 45L);
	port = CreateMsgPort();
	tr = (struct timerequest *)CreateIORequest(port, sizeof(*tr));
	if (GfxBase == NULL || tr == NULL ||
	    OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)tr, 0) != 0) {
		KPutStr("FAIL lcdemo: setup\n");
		return 1;
	}
	TimerBase = tr->tr_node.io_Device;
	stray = IntStray(INTV_NVI);
	graphics();
	intuition();
	check(IntStray(INTV_NVI) == stray, "no stray NVI");
	check(AllocFB("lcdemo") == NULL, "the display is back");
	FreeFB();
	xKPrintf("lcdemo: stray NVIs %ld\n", IntStray(INTV_NVI) - stray);
	CloseDevice((struct IORequest *)tr);
	if (fails != 0)
		return 1;
	KPutStr("PASS lcdemo\n");
	return 0;
}
