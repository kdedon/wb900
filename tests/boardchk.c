/* SPDX-License-Identifier: MIT */
/*
 * A board check, each step held 4 s for a screenshot: the Workbench
 * screen with a window in each disk font, a second screen flipped with
 * it, double buffering on that screen, and an alert that a left click
 * answers or that times out after 10 s.  Prints "boardchk: STEP ..."
 * lines with times from timer.device.
 */
/* EMUFLAGS: --hr --ram=08-1F */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/diskfont.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct DosLibrary *DOSBase;
struct Library *DiskfontBase;
struct Device *TimerBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL boardchk: %s\n", what);
		fails++;
	}
}

static struct timerequest *tio;
static struct IOStdReq *iio;

static void wait20ms(void)
{
	tio->tr_node.io_Command = TR_ADDREQUEST;
	tio->tr_time.tv_secs = 0;
	tio->tr_time.tv_micro = 20000;
	DoIO((struct IORequest *)tio);
}

static void hold(void)
{
	tio->tr_node.io_Command = TR_ADDREQUEST;
	tio->tr_time.tv_secs = 4;
	tio->tr_time.tv_micro = 0;
	DoIO((struct IORequest *)tio);
}

static LONG ms(struct timeval *t0)
{
	struct timeval t;

	GetSysTime(&t);
	return (t.tv_secs - t0->tv_secs) * 1000 +
	    ((LONG)t.tv_micro - (LONG)t0->tv_micro) / 1000;
}

#define NFONTS	4
static struct TextAttr attrs[NFONTS] = {
	{ "fixed.font", 8, 0, 0 }, { "fixed.font", 11, 0, 0 },
	{ "fixed.font", 16, 0, 0 }, { "sans.font", 9, 0, 0 }
};
static struct TextFont *fonts[NFONTS];

/* OpenDiskFont needs a process */
static struct Task *roottask;

static LONG loader(char *args, LONG len)
{
	struct timeval t0;
	WORD i;

	for (i = 0; i < NFONTS; i++) {
		GetSysTime(&t0);
		fonts[i] = OpenDiskFont(&attrs[i]);
		xKPrintf("boardchk: font %s %d %ld ms\n", attrs[i].ta_Name,
		    attrs[i].ta_YSize, ms(&t0));
	}
	Signal(roottask, SIGBREAKF_CTRL_E);
	return 0;
}

static BOOL loadfonts(void)
{
	struct TagItem t[4];
	WORD i;

	roottask = FindTask(NULL);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)loader;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)"boardchk fonts";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 8192;
	t[3].ti_Tag = TAG_DONE;
	if (CreateNewProc(t) == NULL)
		return FALSE;
	Wait(SIGBREAKF_CTRL_E);
	for (i = 0; i < NFONTS; i++)
		if (fonts[i] == NULL)
			return FALSE;
	return TRUE;
}

static char *lines[NFONTS] = {
	"fixed 8: The quick brown fox", "fixed 11: The quick brown fox",
	"fixed 16: The quick brown fox", "sans 9: The quick brown fox"
};

static WORD len(char *s)
{
	WORD n = 0;

	while (s[n])
		n++;
	return n;
}

static struct Window *fontwindow(struct Screen *wb)
{
	struct TagItem t[7];
	struct Window *w;
	struct RastPort *rp;
	WORD i, y;

	t[0].ti_Tag = WA_Left;		t[0].ti_Data = 20;
	t[1].ti_Tag = WA_Top;		t[1].ti_Data = wb->BarHeight + 10;
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = 320;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = 90;
	t[4].ti_Tag = WA_Title;		t[4].ti_Data = (IPTR)"boardchk";
	t[5].ti_Tag = WA_PubScreen;	t[5].ti_Data = (IPTR)wb;
	t[6].ti_Tag = TAG_DONE;
	w = OpenWindowTagList(NULL, t);
	if (w == NULL)
		return NULL;
	rp = w->RPort;
	SetAPen(rp, 1);
	SetDrMd(rp, JAM1);
	y = w->BorderTop + 2;
	for (i = 0; i < NFONTS; i++) {
		SetFont(rp, fonts[i]);
		Move(rp, w->BorderLeft + 6, y + fonts[i]->tf_Baseline);
		Text(rp, lines[i], len(lines[i]));
		y += fonts[i]->tf_YSize + 2;
	}
	return w;
}

/* The window's inner pixels, as a sum */
static ULONG winsum(struct Window *w)
{
	ULONG sum = 0;
	WORD x, y;

	for (y = w->BorderTop; y < w->Height - w->BorderBottom; y += 2)
		for (x = w->BorderLeft; x < w->Width - w->BorderRight; x += 3)
			sum = sum * 3 + ReadPixel(w->RPort, x, y);
	return sum;
}

static void label(struct RastPort *rp, struct TextFont *tf, WORD x, WORD y,
    char *s)
{
	WORD n = len(s);

	SetFont(rp, tf);
	SetAPen(rp, 0);
	RectFill(rp, x, y, x + TextLength(rp, s, n) + 15, y + tf->tf_YSize + 7);
	SetAPen(rp, 1);
	SetDrMd(rp, JAM1);
	Move(rp, x + 8, y + 4 + tf->tf_Baseline);
	Text(rp, s, n);
}

/* Squares of pens 1 and 0, and the label */
static void checkerboard(struct Screen *s)
{
	struct RastPort *rp = &s->RastPort;
	WORD q = s->Width / 16, x, y;

	for (y = s->BarHeight + 1; y < s->Height; y += q)
		for (x = 0; x < s->Width; x += q) {
			SetAPen(rp, ((x + y) / q) & 1);
			RectFill(rp, x, y, x + q - 1, y + q - 1 < s->Height ?
			    y + q - 1 : s->Height - 1);
		}
	label(rp, fonts[2], q, s->BarHeight + q, "SCREEN B");
}

/* The input handler does a depth change: an event, then up to 1 s */
static void flip(int n, struct Screen *b, BOOL front, struct Screen *want)
{
	static struct InputEvent ev;
	struct timeval t0;
	WORD i;

	GetSysTime(&t0);
	if (front)
		ScreenToFront(b);
	else
		ScreenToBack(b);
	ev.ie_Class = IECLASS_NULL;
	iio->io_Command = IND_WRITEEVENT;
	iio->io_Data = (APTR)&ev;
	iio->io_Length = sizeof(ev);
	DoIO((struct IORequest *)iio);
	for (i = 0; i < 50 && IntuitionBase->FirstScreen != want; i++)
		wait20ms();
	xKPrintf("boardchk: switch %d %s %ld ms\n", n,
	    front ? "ScreenToFront" : "ScreenToBack", ms(&t0));
	check(IntuitionBase->FirstScreen == want, "screen depth");
}

static struct Screen *openb(struct Screen *wb)
{
	struct TagItem t[4];

	t[0].ti_Tag = SA_DisplayID;	t[0].ti_Data = GetVPModeID(&wb->ViewPort);
	t[1].ti_Tag = SA_Depth;
	t[1].ti_Data = GetBitMapAttr(wb->RastPort.BitMap, BMA_DEPTH);
	t[2].ti_Tag = SA_Title;		t[2].ti_Data = (IPTR)"SCREEN B";
	t[3].ti_Tag = TAG_DONE;
	return OpenScreenTagList(NULL, t);
}

#define FRAMES	20

/* A box across a band, drawn in the buffer not shown, then shown */
static void dbuf(struct Screen *s)
{
	struct ScreenBuffer *sb[2];
	struct RastPort rp;
	struct timeval t0;
	WORD q = s->Height / 8, y = s->Height / 2 - q / 2, f, x;
	LONG t;

	sb[0] = AllocScreenBuffer(s, NULL, SB_SCREEN_BITMAP);
	sb[1] = AllocScreenBuffer(s, NULL, SB_COPY_BITMAP);
	check(sb[0] != NULL && sb[1] != NULL, "AllocScreenBuffer");
	if (sb[0] == NULL || sb[1] == NULL)
		goto out;
	InitRastPort(&rp);
	GetSysTime(&t0);
	for (f = 1; f <= FRAMES; f++) {
		x = (LONG)(s->Width - q) * f / FRAMES;
		rp.BitMap = sb[f & 1]->sb_BitMap;
		SetAPen(&rp, 0);
		RectFill(&rp, 0, y - 4, s->Width - 1, y + q + 3);
		SetAPen(&rp, 1);
		RectFill(&rp, x, y, x + q - 1, y + q - 1);
		check(ChangeScreenBuffer(s, sb[f & 1]) != 0, "ChangeScreenBuffer");
	}
	t = ms(&t0);
	xKPrintf("boardchk: dbuf %d frames %ld ms, %ld.%ld fps\n", FRAMES, t,
	    FRAMES * 1000L / (t ? t : 1), FRAMES * 10000L / (t ? t : 1) % 10);
	hold();
	ChangeScreenBuffer(s, sb[0]);
out:
	if (sb[1])
		FreeScreenBuffer(s, sb[1]);
	if (sb[0])
		FreeScreenBuffer(s, sb[0]);
}

static UBYTE alerttext[] =
    "\000\050\024boardchk: click left to answer (10 s)\000";

static void alert(void)
{
	struct timeval t0;
	BOOL ret;
	LONG t;

	KPutStr("boardchk: alert up, click left within 10 s\n");
	GetSysTime(&t0);
	ret = TimedDisplayAlert(RECOVERY_ALERT, alerttext, 40, 500);
	t = ms(&t0);
	if (!ret && t >= 9900)
		xKPrintf("boardchk: alert timed out after %ld ms\n", t);
	else
		xKPrintf("boardchk: alert %s after %ld ms\n",
		    ret ? "TRUE" : "FALSE", t);
}

static BOOL setup(void)
{
	struct MsgPort *port = CreateMsgPort();

	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	DiskfontBase = OpenLibrary("diskfont.library", 0L);
	tio = (struct timerequest *)CreateIORequest(port, sizeof(*tio));
	iio = (struct IOStdReq *)CreateIORequest(port, sizeof(*iio));
	if (GfxBase == NULL || IntuitionBase == NULL || DOSBase == NULL ||
	    DiskfontBase == NULL || tio == NULL || iio == NULL ||
	    OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)tio, 0) ||
	    OpenDevice("input.device", 0, (struct IORequest *)iio, 0))
		return FALSE;
	TimerBase = tio->tr_node.io_Device;
	return TRUE;
}

int main(void)
{
	struct Screen *wb, *b;
	struct Window *w;
	struct timeval t0;
	ULONG sum;
	WORD i, x, y;

	if (!setup()) {
		KPutStr("FAIL boardchk: setup\n");
		return 1;
	}

	KPutStr("boardchk: workbench\n");
	GetSysTime(&t0);
	check(OpenWorkBench() != 0, "OpenWorkBench");
	wb = LockPubScreen("Workbench");
	if (wb == NULL || !loadfonts()) {
		KPutStr("FAIL boardchk: Workbench or fonts\n");
		return 1;
	}
	w = fontwindow(wb);
	if (w == NULL) {
		KPutStr("FAIL boardchk: window\n");
		return 1;
	}
	xKPrintf("boardchk: workbench %dx%dx%d mode %lx, window up %ld ms\n",
	    wb->Width, wb->Height,
	    (int)GetBitMapAttr(wb->RastPort.BitMap, BMA_DEPTH),
	    GetVPModeID(&wb->ViewPort), ms(&t0));
	sum = winsum(w);
	hold();

	KPutStr("boardchk: screens\n");
	GetSysTime(&t0);
	b = openb(wb);
	if (b == NULL) {
		KPutStr("FAIL boardchk: OpenScreen\n");
		return 1;
	}
	checkerboard(b);
	xKPrintf("boardchk: screen B up %ld ms\n", ms(&t0));
	hold();
	flip(1, b, FALSE, wb);
	hold();
	/* drawn while behind, shown by the next switch */
	x = b->Width / 2;
	y = b->Height / 2;
	label(&b->RastPort, fonts[2], x, y, "DRAWN BEHIND");
	flip(2, b, TRUE, b);
	check(ReadPixel(&b->RastPort, x + 1, y + 1) == 0, "drawn behind");
	hold();
	flip(3, b, FALSE, wb);
	if (winsum(w) == sum)
		KPutStr("boardchk: wb intact\n");
	else
		check(FALSE, "wb window changed");
	hold();
	flip(4, b, TRUE, b);
	hold();

	KPutStr("boardchk: dbuf\n");
	dbuf(b);

	KPutStr("boardchk: alert\n");
	alert();

	KPutStr("boardchk: close\n");
	CloseScreen(b);
	CloseWindow(w);
	UnlockPubScreen(NULL, wb);
	for (i = 0; i < NFONTS; i++)
		CloseFont(fonts[i]);
	check(CloseWorkBench() != 0, "CloseWorkBench");
	if (fails != 0)
		return 1;
	KPutStr("PASS boardchk\n");
	return 0;
}
