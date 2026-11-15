/* SPDX-License-Identifier: MIT */
/*
 * intuition.library on the display: the Workbench screen from
 * OpenWorkBench(), in the display's bitmap when there is one, and two
 * overlapping windows with titles and system gadgets, the front one with
 * a string gadget and a button, then fills, text, lines, scrolls and
 * blits in the back window.  Each phase prints its time from
 * timer.device ("wbdemo: t PHASE N ms").  The screen stays up when the
 * test ends.
 */
#include <exec/exec.h>
#include <devices/timer.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxdriver.h>
#include <graphics/modeid.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Device *TimerBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL wbdemo: %s\n", what);
		fails++;
	}
}

static struct timeval t0;

/* The time since the last phase */
static void lap(char *what)
{
	struct timeval t;

	if (TimerBase == NULL)
		return;
	GetSysTime(&t);
	xKPrintf("wbdemo: t %s %ld ms\n", what,
	    (t.tv_secs - t0.tv_secs) * 1000 +
	    ((LONG)t.tv_micro - (LONG)t0.tv_micro) / 1000);
	GetSysTime(&t0);
}

static void opentimer(void)
{
	struct MsgPort *port = CreateMsgPort();
	struct timerequest *tio;

	tio = (struct timerequest *)CreateIORequest(port,
	    sizeof(struct timerequest));
	if (tio != NULL && OpenDevice(TIMERNAME, UNIT_VBLANK,
	    (struct IORequest *)tio, 0) == 0) {
		TimerBase = tio->tr_node.io_Device;
		GetSysTime(&t0);
	}
}

static char line[] = "wb900 graphics.library text, topaz 8";

/* Drawing in the window's inner area */
static void drawing(struct Window *w)
{
	struct RastPort *rp = w->RPort;
	WORD x0 = w->BorderLeft, y0 = w->BorderTop;
	WORD x1 = w->Width - w->BorderRight - 1;
	WORD y1 = w->Height - w->BorderBottom - 1;
	WORD i;

	SetAPen(rp, 1);
	SetDrMd(rp, JAM2);
	for (i = 0; i < 4; i++) {
		SetAPen(rp, i & 1);
		RectFill(rp, x0, y0, x1, y1);
	}
	lap("rectfill");
	SetAPen(rp, 1);
	SetBPen(rp, 0);
	for (i = 0; y0 + 8 + i * 9 < y1 && i < 20; i++) {
		Move(rp, x0 + 4 + (i & 3), y0 + 7 + i * 9);
		Text(rp, line, sizeof(line) - 1);
	}
	lap("text");
	for (i = 0; i < 40; i++) {
		Move(rp, x0 + i * 4, y0);
		Draw(rp, x1 - i * 4, y1);
	}
	for (i = 0; i < 20; i++) {
		Move(rp, x0, y0 + i * 6);
		Draw(rp, x1, y0 + i * 6);
	}
	lap("draw");
	for (i = 0; i < 8; i++)
		ScrollRaster(rp, 0, 9, x0, y0, x1, y1);
	lap("scroll");
	for (i = 0; i < 8; i++)
		ClipBlit(rp, x0, y0, rp, x0 + 3 + i, y0 + 20 + i,
		    (x1 - x0) / 2, (y1 - y0) / 2, 0xC0);
	lap("blit");
	SetAPen(rp, 0);
	RectFill(rp, x0, y0, x1, y1);
	SetAPen(rp, 1);
	Move(rp, x0 + 8, y0 + 12);
	Text(rp, line, sizeof(line) - 1);
}

static UBYTE strbuf[40] = "wb900 string gadget";
static UBYTE undobuf[40];
static struct StringInfo strinfo = { strbuf, undobuf, 0, 40 };
static WORD strxy[] = { -2, -2, 201, -2, 201, 9, -2, 9, -2, -2 };
static struct Border strborder = { 0, 0, 1, 0, JAM1, 5, strxy, NULL };
static struct Gadget strgad = {
	NULL, 20, 30, 200, 8, GFLG_GADGHCOMP, GACT_RELVERIFY,
	GTYP_STRGADGET, (APTR)&strborder, NULL, NULL, 0, (APTR)&strinfo, 2
};
static WORD butxy[] = { 0, 0, 71, 0, 71, 13, 0, 13, 0, 0 };
static struct Border butborder = { 0, 0, 1, 0, JAM1, 5, butxy, NULL };
static struct IntuiText buttext = { 1, 0, JAM1, 12, 3, NULL, "Button", NULL };
static struct Gadget butgad = {
	&strgad, 20, 50, 72, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
	GTYP_BOOLGADGET, (APTR)&butborder, NULL, &buttext, 0, NULL, 1
};

static struct Window *openwin(struct Screen *s, WORD x, WORD y, WORD w,
    WORD h, char *title, struct Gadget *g)
{
	struct TagItem t[13];

	t[0].ti_Tag = WA_Left;		t[0].ti_Data = x;
	t[1].ti_Tag = WA_Top;		t[1].ti_Data = y;
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = w;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = h;
	t[4].ti_Tag = WA_Title;		t[4].ti_Data = (IPTR)title;
	t[5].ti_Tag = WA_PubScreen;	t[5].ti_Data = (IPTR)s;
	t[6].ti_Tag = WA_CloseGadget;	t[6].ti_Data = TRUE;
	t[7].ti_Tag = WA_DepthGadget;	t[7].ti_Data = TRUE;
	t[8].ti_Tag = WA_SizeGadget;	t[8].ti_Data = TRUE;
	t[9].ti_Tag = WA_DragBar;	t[9].ti_Data = TRUE;
	t[10].ti_Tag = WA_Activate;	t[10].ti_Data = TRUE;
	t[11].ti_Tag = WA_Gadgets;	t[11].ti_Data = (IPTR)g;
	t[12].ti_Tag = TAG_DONE;
	return OpenWindowTagList(NULL, t);
}

/* The DrawInfo pens and the pointer's, with the pixels they show as, and
   the first four colours */
static void pens(struct Screen *s)
{
	static char *names[] = { "background", "text", "shine", "shadow",
		"fill", "filltext", "bar" };
	static UWORD which[] = { BACKGROUNDPEN, TEXTPEN, SHINEPEN, SHADOWPEN,
		FILLPEN, FILLTEXTPEN, BARBLOCKPEN };
	struct BitMap *bm = s->RastPort.BitMap;
	UBYTE *pix = IS_GFXBM(bm) ? GFXBM(bm)->gb_PixTab : NULL;
	struct DrawInfo *dri = GetScreenDrawInfo(s);
	UWORD i, p;

	KPutStr("wbdemo: pen/pixel");
	for (i = 0; i < 7; i++) {
		p = dri->dri_Pens[which[i]];
		xKPrintf(" %s %d/%d", names[i], p, pix ? pix[p] : p);
	}
	for (p = 17; p < 20; p++)
		xKPrintf(" pointer %d/%d", p, pix ? pix[p] : p);
	KPutStr("\nwbdemo: colours");
	for (p = 0; p < 4; p++)
		xKPrintf(" %03lx", GetRGB4(s->ViewPort.ColorMap, p));
	KPutStr((dri->dri_Flags & DRIF_NEWLOOK) ? ", new look\n" : ", old look\n");
	FreeScreenDrawInfo(s, dri);
}

int main(void)
{
	struct Screen *wb;
	struct Window *w1, *w2;
	struct BitMap *bm;
	ULONG id;

	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	if (GfxBase == NULL || IntuitionBase == NULL) {
		KPutStr("FAIL wbdemo: OpenLibrary\n");
		return 1;
	}
	KPutStr("wbdemo: libraries open\n");
	opentimer();
	check(OpenWorkBench() != 0, "OpenWorkBench");
	lap("screen-open");
	wb = LockPubScreen("Workbench");
	check(wb != NULL, "LockPubScreen");
	if (wb == NULL)
		return 1;
	bm = wb->RastPort.BitMap;
	id = GetVPModeID(&wb->ViewPort);
	xKPrintf("wbdemo: Workbench %dx%dx%d, mode %lx, %s bitmap\n",
	    wb->Width, wb->Height, (int)GetBitMapAttr(bm, BMA_DEPTH), id,
	    (IS_GFXBM(bm) && (GFXBM(bm)->gb_Flags & GBF_DISPLAY)) ?
	    "display" : "RAM");
	/* with a display, the screen is on it */
	check(NextDisplayInfo(INVALID_ID) == INVALID_ID ||
	    (id != INVALID_ID && IS_GFXBM(bm) &&
	    (GFXBM(bm)->gb_Flags & GBF_DISPLAY)), "the Workbench in VRAM");
	pens(wb);

	w1 = openwin(wb, 40, 40, wb->Width / 2, wb->Height / 2,
	    "wb900 back window", NULL);
	lap("win1-open");
	w2 = openwin(wb, wb->Width / 4, wb->Height / 4, 300, 100,
	    "wb900 front window", &butgad);
	lap("win2-open");
	check(w1 != NULL && w2 != NULL, "OpenWindowTagList");
	if (w1 != NULL && w2 != NULL) {
		xKPrintf("wbdemo: back window %d,%d %dx%d, front %d,%d %dx%d\n",
		    w1->LeftEdge, w1->TopEdge, w1->Width, w1->Height,
		    w2->LeftEdge, w2->TopEdge, w2->Width, w2->Height);
		drawing(w1);
		CloseWindow(w2);
		lap("win-close");
		w2 = openwin(wb, wb->Width / 4, wb->Height / 4, 300, 100,
		    "wb900 front window", &butgad);
		lap("win2-reopen");
		check(w2 != NULL, "OpenWindowTagList again");
	}
	UnlockPubScreen(NULL, wb);

	if (fails != 0)
		return 1;
	KPutStr("PASS wbdemo\n");
	return 0;
}
