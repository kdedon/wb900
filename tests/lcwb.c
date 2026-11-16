/*
 * intuition's Workbench screen on lc.gfxdriver and a model of the LC
 * card's 8568, in the 1.x style of
 * DEVS:system-configuration: 640x200 mono, white on blue; then a screen
 * in each of the other modes.  After opening the screen and a window,
 * moving and closing it and moving the pointer, the model's bitmap must
 * be the shadow, the registers must show it, and the chip be idle, with
 * each step's port actions and their time printed as a cost regression.
 * A second 640x200 screen takes turns in front with the Workbench, each
 * drawn into while behind and shown in its own colours, each switch a
 * register write.  Three screens take turns at 640x400, two bitmaps of
 * which fit VRAM, and two at 640x480, one of which does.
 */
#include <exec/exec.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxdriver.h>
#include <graphics/hrmodeid.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/alib.h>

#include "lc/vdcmodel.h"
#include "lc/lctest.h"

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static int fails;
static struct BitMap *bm;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL lcwb: %s\n", what);
		fails++;
	}
}

static struct MsgPort *port;
static struct IOStdReq *io;
static struct InputEvent ev;

static void event(UBYTE class, UWORD code, WORD x, WORD y)
{
	ev.ie_NextEvent = NULL;
	ev.ie_Class = class;
	ev.ie_SubClass = 0;
	ev.ie_Code = code;
	ev.ie_Qualifier = class == IECLASS_RAWMOUSE ? IEQUALIFIER_RELATIVEMOUSE : 0;
	ev.ie_X = x;
	ev.ie_Y = y;
	io->io_Command = IND_WRITEEVENT;
	io->io_Data = &ev;
	io->io_Length = sizeof(ev);
	DoIO((struct IORequest *)io);
}

/* Let the input handler do the work, then check the step */
static void step(char *what, ULONG limit)
{
	ULONG us;

	event(IECLASS_NULL, 0, 0, 0);
	TimeDelay(UNIT_VBLANK, 0, 40000);
	check(lct_step(bm, what, &us), what);
	if (limit && us > limit) {
		xKPrintf("FAIL lcwb: %s takes %ld us, over %ld\n", what, us,
		    limit);
		fails++;
	}
}

/* A window on s: open, draw, move, size, the pointer, close */
static void windows(struct Screen *s)
{
	struct Window *w;
	struct TagItem t[12];
	WORD i;

	t[0].ti_Tag = WA_Left;		t[0].ti_Data = 40;
	t[1].ti_Tag = WA_Top;		t[1].ti_Data = 30;
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = s->Width / 2;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = 120;
	t[4].ti_Tag = WA_Title;		t[4].ti_Data = (ULONG)"An LC window";
	t[5].ti_Tag = WA_DragBar;	t[5].ti_Data = TRUE;
	t[6].ti_Tag = WA_DepthGadget;	t[6].ti_Data = TRUE;
	t[7].ti_Tag = WA_CloseGadget;	t[7].ti_Data = TRUE;
	t[8].ti_Tag = WA_SizeGadget;	t[8].ti_Data = TRUE;
	t[9].ti_Tag = WA_SimpleRefresh;	t[9].ti_Data = TRUE;
	t[10].ti_Tag = WA_CustomScreen;	t[10].ti_Data = (ULONG)s;
	t[11].ti_Tag = TAG_DONE;
	w = OpenWindowTagList(NULL, t);
	check(w != NULL, "OpenWindow");
	step("window open", s->Width == 640 ? 75000 : 0);
	if (w == NULL)
		return;
	SetAPen(w->RPort, 1);
	Move(w->RPort, 20, 30);
	Text(w->RPort, "Hello, LC", 9);
	step("text in the window", 0);
	MoveWindow(w, 80, 16);
	step("window move by 80, 16", 0);
	MoveWindow(w, -3, 5);
	step("window move by -3, 5", 0);
	SizeWindow(w, 40, 20);
	step("window size", 0);
	for (i = 0; i < 4; i++) {
		event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, 4, 2);
		step("pointer move", 2000);
	}
	CloseWindow(w);
	step("window close", 0);
}

static BOOL isdisplay(struct Screen *s)
{
	return (GFXBM(s->RastPort.BitMap)->gb_Flags & GBF_DISPLAY) != 0;
}

/* Pen 1 in a box of s's bitmap, and whether its pixels read back */
static void drawbox(struct Screen *s, WORD x, WORD y)
{
	SetAPen(&s->RastPort, 1);
	RectFill(&s->RastPort, x, y, x + 29, y + 19);
}

static BOOL hasbox(struct Screen *s, WORD x, WORD y)
{
	return ReadPixel(&s->RastPort, x, y) == 1 &&
	    ReadPixel(&s->RastPort, x + 29, y + 19) == 1;
}

/* s to the front: the model shows its bitmap in its colours */
static void tofront(struct Screen *s, struct Screen *back, UBYTE colours)
{
	ScreenToFront(s);
	bm = s->RastPort.BitMap;
	step("screen to the front", 2000);
	check(IntuitionBase->FirstScreen == s && isdisplay(s) && !isdisplay(back),
	    "the front screen has the display");
	check(lcm_reg[26] == colours, "the front screen's colours");
}

/* A second screen on the Workbench's display, in turn in front */
static void flips(struct Screen *wb)
{
	struct TagItem t[5];
	struct Screen *s;
	UBYTE wbcolours = lcm_reg[26];
	UWORD allocs = lct_allocs;
	WORD k;

	t[0].ti_Tag = SA_DisplayID;	t[0].ti_Data = LCHIRES_ID;
	t[1].ti_Tag = SA_Depth;		t[1].ti_Data = 1;
	t[2].ti_Tag = SA_Title;		t[2].ti_Data = (ULONG)"Black on white";
	t[3].ti_Tag = SA_SharePens;	t[3].ti_Data = TRUE;
	t[4].ti_Tag = TAG_DONE;
	s = OpenScreenTagList(NULL, t);
	check(s != NULL, "a second screen");
	if (s == NULL)
		return;
	SetRGB4(&s->ViewPort, 0, 15, 15, 15);
	SetRGB4(&s->ViewPort, 1, 0, 0, 0);
	bm = s->RastPort.BitMap;
	step("a second screen", 0);
	check(lct_allocs == allocs && isdisplay(s) && !isdisplay(wb) &&
	    lcm_reg[26] == 0x0F, "the second screen has the display");
	drawbox(s, 100, 50);
	step("a box in front", 0);
	for (k = 0; k < 3; k++) {
		drawbox(wb, 300 + 40 * k, 120);
		step("a box behind", 0);
		tofront(wb, s, wbcolours);
		check(hasbox(wb, 300 + 40 * k, 120) && !hasbox(wb, 100, 50),
		    "the Workbench's pixels");
		drawbox(s, 100 + 40 * k, 150);
		step("a box behind", 0);
		tofront(s, wb, 0x0F);
		check(hasbox(s, 100, 50) && hasbox(s, 100 + 40 * k, 150) &&
		    !hasbox(s, 300, 120), "the second screen's pixels");
	}
	CloseScreen(s);
	bm = wb->RastPort.BitMap;
	step("the second screen closed", 0);
	check(isdisplay(wb) && lcm_reg[26] == wbcolours,
	    "the Workbench gets the display back");
}

/*
 * n screens of mode id, w x h, each with its box, brought to the front
 * in order; bit k of fast: the kth switch is a register write
 */
static void turns(ULONG id, WORD w, WORD h, WORD n, const char *order,
    UWORD fast)
{
	struct TagItem t[6];
	struct Screen *s[3];
	ULONG us;
	WORD i, k;

	t[0].ti_Tag = SA_DisplayID;	t[0].ti_Data = id;
	t[1].ti_Tag = SA_Width;		t[1].ti_Data = w;
	t[2].ti_Tag = SA_Height;	t[2].ti_Data = h;
	t[3].ti_Tag = SA_Depth;		t[3].ti_Data = 1;
	t[4].ti_Tag = SA_SharePens;	t[4].ti_Data = TRUE;
	t[5].ti_Tag = TAG_DONE;
	for (i = 0; i < n; i++) {
		s[i] = OpenScreenTagList(NULL, t);
		check(s[i] != NULL, "a screen to take turns");
		if (s[i] == NULL)
			goto out;
		drawbox(s[i], 100 + 40 * i, h - 40);
	}
	bm = s[n - 1]->RastPort.BitMap;
	step("the screens open", 0);
	for (k = 0; order[k]; k++) {
		i = order[k] - '0';
		ScreenToFront(s[i]);
		event(IECLASS_NULL, 0, 0, 0);
		TimeDelay(UNIT_VBLANK, 0, 40000);
		bm = s[i]->RastPort.BitMap;
		check(lct_step(bm, "turn in front", &us), "turn in front");
		check(IntuitionBase->FirstScreen == s[i] && isdisplay(s[i]) &&
		    hasbox(s[i], 100 + 40 * i, h - 40) &&
		    !hasbox(s[i], 100 + 40 * ((i + 1) % n), h - 40),
		    "the screen in front");
		check(!(fast >> k & 1) || us < 2000, "a register write");
	}
	i = n;
out:
	while (i > 0)
		CloseScreen(s[--i]);
}

/* A screen in mode i of the list after 640x200 */
static void othermode(UWORD i)
{
	static const ULONG ids[7] = {
		LCLORES_ID, LCHIRESPAL_ID, LCLORESPAL_ID, LCHIRESLACE_ID,
		LCLORESLACE_ID, LCHIRESLACE480_ID, LCLORESLACE480_ID
	};
	static const UWORD size[7][3] = {
		{ 320, 200, 60 }, { 640, 256, 50 }, { 320, 256, 50 },
		{ 640, 400, 60 }, { 320, 400, 60 }, { 640, 480, 60 },
		{ 320, 480, 60 }
	};
	struct TagItem t[7];
	struct Screen *s;
	UWORD allocs = lct_allocs, frees = lct_frees;

	t[0].ti_Tag = SA_DisplayID;	t[0].ti_Data = ids[i];
	t[1].ti_Tag = SA_Width;		t[1].ti_Data = size[i][0];
	t[2].ti_Tag = SA_Height;	t[2].ti_Data = size[i][1];
	t[3].ti_Tag = SA_Depth;		t[3].ti_Data = 1;
	t[4].ti_Tag = SA_Title;		t[4].ti_Data = (ULONG)"An LC screen";
	t[5].ti_Tag = SA_SharePens;	t[5].ti_Data = TRUE;
	t[6].ti_Tag = TAG_DONE;
	s = OpenScreenTagList(NULL, t);
	check(s != NULL, "OpenScreen");
	if (s == NULL)
		return;
	bm = s->RastPort.BitMap;
	check(s->Width == size[i][0] && s->Height == size[i][1] &&
	    GetVPModeID(&s->ViewPort) == ids[i] && lct_allocs == allocs + 1,
	    "the mode's display");
	check(lct_mode(bm, size[i][2]), "the mode's registers");
	step("OpenScreen", 0);
	/* windows on 640x400 stand for the other interlaced modes, to save time */
	if (i < 4)
		windows(s);
	CloseScreen(s);
	check(lct_frees == frees + 1, "the display goes back");
}

int main(void)
{
	struct Screen *wb;
	UWORD i;

	if (!lct_setup("lcwb", 0xAA)) {
		KPutStr("FAIL lcwb: setup\n");
		return 1;
	}
	port = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
	if (OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL lcwb: input.device\n");
		return 1;
	}
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	if (GfxBase == NULL || IntuitionBase == NULL) {
		KPutStr("FAIL lcwb: OpenLibrary\n");
		return 1;
	}

	check(OpenWorkBench() != 0, "OpenWorkBench");
	wb = LockPubScreen("Workbench");
	check(wb != NULL, "Workbench screen");
	if (wb == NULL)
		goto done;
	bm = wb->RastPort.BitMap;
	check(wb->Width == 640 && wb->Height == 200 &&
	    GetBitMapAttr(bm, BMA_DEPTH) == 1 &&
	    GetVPModeID(&wb->ViewPort) == LCHIRES_ID, "640x200 mono");
	check(lct_allocs == 1 && bm->Planes[0] != NULL, "the display's bitmap");
	check(lct_mode(bm, 60), "640x200 registers");
	step("OpenWorkBench", 0);
	check(lcm_reg[26] == 0xF2, "white on blue");
	windows(wb);
	flips(wb);
	UnlockPubScreen(NULL, wb);
	check(CloseWorkBench(), "CloseWorkBench");
	check(lct_frees == 1, "the display goes back");
	for (i = 0; i < 7; i++)
		othermode(i);
	/* 2 slots: s0 gives its slot to s2, then s1 to s0 */
	turns(LCHIRESLACE_ID, 640, 400, 3, "12020", 0x1B);
	/* 1 slot: each switch pushes the bitmap */
	turns(LCHIRESLACE480_ID, 640, 480, 2, "01", 0);

done:
	lct_cleanup();
	CloseLibrary((struct Library *)IntuitionBase);
	CloseLibrary((struct Library *)GfxBase);
	CloseDevice((struct IORequest *)io);
	if (fails == 0)
		KPutStr("PASS lcwb\n");
	return fails != 0;
}
