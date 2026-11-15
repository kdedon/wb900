/* SPDX-License-Identifier: MIT */
/*
 * Clock from SYS:Utilities, on the Workbench screen in RAM.  The root
 * program sets the system time and checks the hands' pixels and the
 * digits, that a hand that moved was erased, and that a tick redraws
 * only the hands: a dot it draws on the face stays.  Then a Shell runs
 * Clock without the seconds hand.  It reports Clock's stack use.
 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/workbench.h>
#include <proto/timer.h>

struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Library *WorkbenchBase;
struct Device *TimerBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL clock: %s\n", what);
		fails++;
	}
}

static struct MsgPort *ioport;
static struct IOStdReq *io;
static struct timerequest *tr;
static struct InputEvent ev;

static void event(UBYTE class, UWORD code, UWORD qual, WORD x, WORD y)
{
	ev.ie_NextEvent = NULL;
	ev.ie_Class = class;
	ev.ie_SubClass = 0;
	ev.ie_Code = code;
	ev.ie_Qualifier = qual;
	ev.ie_X = x;
	ev.ie_Y = y;
	io->io_Command = IND_WRITEEVENT;
	io->io_Data = &ev;
	io->io_Length = sizeof(ev);
	DoIO((struct IORequest *)io);
}

static struct Screen *wb;
static struct Window *win;
static struct DrawInfo *dri;
static struct Task *clock;

static struct Window *clockwin(void)
{
	struct Window *w;
	char *t, *a;

	for (w = wb->FirstWindow; w; w = w->NextWindow) {
		if ((t = w->Title) == NULL)
			continue;
		for (a = "Clock"; *a && *a == *t; a++, t++)
			;
		if (*a == 0 && *t == 0)
			return w;
	}
	return NULL;
}

static struct Window *waitwin(BOOL open)
{
	struct Window *w = NULL;
	int i;

	for (i = 0; i < 500; i++) {
		w = clockwin();
		if ((w != NULL) == open)
			break;
		Delay(1);
	}
	return w;
}

#define DAY	86400L

static void settime(ULONG secs)
{
	tr->tr_node.io_Command = TR_SETSYSTIME;
	tr->tr_time.tv_secs = 100 * DAY + secs;
	tr->tr_time.tv_micro = 0;
	DoIO((struct IORequest *)tr);
}

/* Clock's layout */
static WORD cx, cy, radius, textx, texty;
static BOOL seconds;

static void layout(void)
{
	struct RastPort *rp = win->RPort;
	WORD w = win->Width - win->BorderLeft - win->BorderRight;
	WORD h = win->Height - win->BorderTop - win->BorderBottom;
	WORD th = rp->TxHeight + 4;

	cx = win->BorderLeft + w / 2;
	cy = win->BorderTop + (h - th) / 2;
	radius = ((w < h - th ? w : h - th) / 2) - 2;
	textx = cx - TextLength(rp, "00:00:00", seconds ? 8 : 5) / 2;
	texty = win->BorderTop + h - 2 - rp->TxHeight;
}

/* sin(i * 6 degrees) * 1024 */
static WORD sintab[16] = {
	0, 107, 213, 316, 416, 512, 602, 685, 761, 828, 887, 935, 974, 1002,
	1018, 1024
};

static WORD sin60(WORD p)
{
	p %= 60;
	if (p < 15)
		return sintab[p];
	if (p < 30)
		return sintab[30 - p];
	if (p < 45)
		return -sintab[p - 30];
	return -sintab[60 - p];
}

/* Whether the pixel at sixtieth pos, sixteenths len of the radius, is
 * ink */
static BOOL ink(WORD pos, WORD len)
{
	LONG l = (LONG)radius * len / 16;
	WORD x = cx + (WORD)(l * sin60(pos) / 1024);
	WORD y = cy - (WORD)(l * sin60(pos + 15) / 1024);

	return ReadPixel(win->RPort, x, y) != dri->dri_Pens[BACKGROUNDPEN];
}

static struct BitMap *refbm;
static struct RastPort ref;

static BOOL digits(char *s, WORD n)
{
	WORD x, y, w = TextLength(&ref, s, n);

	SetABPenDrMd(&ref, dri->dri_Pens[TEXTPEN], dri->dri_Pens[BACKGROUNDPEN], JAM2);
	Move(&ref, 0, ref.TxBaseline);
	Text(&ref, s, n);
	for (y = 0; y < ref.TxHeight; y++)
		for (x = 0; x < w; x++)
			if (ReadPixel(&ref, x, y) !=
			    ReadPixel(win->RPort, textx + x, texty + y))
				return FALSE;
	return TRUE;
}

/* Whether Clock shows time t (seconds of the day) */
static BOOL shows(ULONG t)
{
	char d[8];
	WORD h = t / 3600 % 12 * 5 + t / 60 % 60 / 12, m = t / 60 % 60;
	WORD s = t % 60;

	d[0] = '0' + t / 36000;
	d[1] = '0' + t / 3600 % 10;
	d[2] = ':';
	d[3] = '0' + m / 10;
	d[4] = '0' + m % 10;
	d[5] = ':';
	d[6] = '0' + s / 10;
	d[7] = '0' + s % 10;
	return ink(h, 6) && ink(m, 9) && (!seconds || ink(s, 11)) &&
	    digits(d, seconds ? 8 : 5);
}

/* Whether Clock shows the time, read before and after the checks */
static BOOL showsnow(void)
{
	struct timeval a, b;
	int i;
	BOOL ok;

	for (i = 0; i < 100; i++) {
		GetSysTime(&a);
		ok = shows(a.tv_secs % DAY);
		GetSysTime(&b);
		if (ok && a.tv_secs == b.tv_secs)
			return TRUE;
		Delay(1);
	}
	return FALSE;
}

static void stackuse(struct Task *t)
{
	UBYTE *p, *top;

	p = (UBYTE *)t->tc_SPLower + 8;
	top = (UBYTE *)t->tc_SPUpper;
	while (p < top && *p == 0)
		p++;
	xKPrintf("clock: %s stack %d of %d bytes used\n",
	    t->tc_Node.ln_Name, (int)(top - p),
	    (int)(top - (UBYTE *)t->tc_SPLower));
}

static struct Gadget *closegadget(void)
{
	struct Gadget *g;

	for (g = win->FirstGadget; g; g = g->NextGadget)
		if ((g->GadgetType & GTYP_SYSTYPEMASK) == GTYP_CLOSE &&
		    (g->GadgetType & GTYP_SYSGADGET))
			return g;
	return NULL;
}

static void workbench(void)
{
	struct Gadget *g;
	WORD x, y, dot;

	check(OpenWorkbenchObjectA("SYS:Utilities/Clock", NULL), "launch");
	win = waitwin(TRUE);
	check(win != NULL, "Clock's window");
	if (win == NULL)
		return;
	clock = win->UserPort->mp_SigTask;
	seconds = TRUE;
	layout();
	check(radius > 30, "radius");

	settime(3 * 3600L + 30);
	Delay(75);
	check(showsnow(), "03:00:30");
	check(!ink(45, 6), "nothing at 9 o'clock");

	/* A dot on the face outside the dial, which a full redraw clears */
	dot = radius * 7 / 10;
	SetAPen(win->RPort, dri->dri_Pens[FILLPEN]);
	WritePixel(win->RPort, cx - dot, cy - dot);

	/* The minute hand moves from 59 to 0 and the hour hand to 4 */
	settime(3 * 3600L + 59 * 60 + 58);
	Delay(75);
	check(showsnow(), "03:59:58");
	Delay(150);
	check(showsnow(), "04:00:01");
	check(!ink(59, 9), "the old minute hand erased");
	check(ReadPixel(win->RPort, cx - dot, cy - dot) ==
	    dri->dri_Pens[FILLPEN], "ticks redraw only the hands");
	stackuse(clock);

	g = closegadget();
	check(g != NULL, "close gadget");
	if (g != NULL) {
		x = win->LeftEdge + g->LeftEdge + g->Width / 2;
		y = win->TopEdge + g->TopEdge + g->Height / 2;
		event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, 0, x, y);
		event(IECLASS_RAWMOUSE, IECODE_LBUTTON, 0, x, y);
		event(IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, 0, x, y);
		check(waitwin(FALSE) == NULL, "closed");
	}
}

struct sync {
	struct Task *s_Task;
	LONG s_RC;
};

static struct sync s;

static BOOL script(void)
{
	static char cmd[] = "SYS:Utilities/Clock NOSECONDS\n";
	BPTR fh;

	if ((fh = Open("SYS:ClockScript", MODE_NEWFILE)) == NULL)
		return FALSE;
	Write(fh, cmd, sizeof(cmd) - 1);
	Close(fh);
	return TRUE;
}

/* A Shell's commands get 1600 bytes of stack unless Stack sets more */
static LONG shell(char *args, LONG len)
{
	BPTR in;
	LONG rc;

	if ((in = Open("SYS:ClockScript", MODE_OLDFILE)) == NULL)
		return -1;
	rc = Execute("Stack 4096", in, NULL);
	Close(in);
	return rc ? RETURN_OK : RETURN_FAIL;
}

static void done(LONG rc, struct sync *sy)
{
	sy->s_RC = rc;
	Signal(sy->s_Task, SIGBREAKF_CTRL_F);
}

static void cli(void)
{
	struct TagItem t[6];

	check(script(), "script");
	settime(10 * 3600L + 10 * 60 + 30);
	s.s_Task = FindTask(NULL);
	s.s_RC = -1;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (IPTR)shell;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (IPTR)"ClockShell";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 4096;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (IPTR)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (IPTR)&s;
	t[5].ti_Tag = TAG_DONE;
	check(CreateNewProc(t) != NULL, "Shell process");
	win = waitwin(TRUE);
	check(win != NULL, "Clock from the Shell");
	if (win == NULL)
		return;
	clock = win->UserPort->mp_SigTask;
	seconds = FALSE;
	layout();
	Delay(25);
	check(showsnow(), "Shell: 10:10");
	check(!ink(30, 11), "Shell: no seconds hand");
	Signal(clock, SIGBREAKF_CTRL_C);
	Wait(SIGBREAKF_CTRL_F);
	check(s.s_RC == 0, "Shell: return code");
	check(clockwin() == NULL, "Shell: window closed");
}

int main(void)
{
	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	WorkbenchBase = OpenLibrary(WORKBENCH_NAME, 45L);
	ioport = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(ioport, sizeof(struct IOStdReq));
	tr = (struct timerequest *)CreateIORequest(ioport, sizeof(*tr));
	if (DOSBase == NULL || GfxBase == NULL || IntuitionBase == NULL ||
	    WorkbenchBase == NULL || tr == NULL ||
	    OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0 ||
	    OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)tr, 0) != 0) {
		KPutStr("FAIL clock: OpenLibrary\n");
		return 1;
	}
	TimerBase = tr->tr_node.io_Device;
	check(OpenWorkBench() != 0, "OpenWorkBench");
	wb = LockPubScreen("Workbench");
	dri = GetScreenDrawInfo(wb);
	refbm = AllocBitMap(128, 32, GetBitMapAttr(wb->RastPort.BitMap,
	    BMA_DEPTH), BMF_CLEAR, wb->RastPort.BitMap);
	InitRastPort(&ref);
	ref.BitMap = refbm;
	SetFont(&ref, dri->dri_Font);
	check(refbm != NULL, "bitmap");

	if (refbm != NULL) {
		workbench();
		cli();
	}

	FreeBitMap(refbm);
	FreeScreenDrawInfo(wb, dri);
	UnlockPubScreen(NULL, wb);
	if (fails != 0)
		return 1;
	KPutStr("PASS clock\n");
	return 0;
}
