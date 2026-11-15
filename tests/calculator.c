/* SPDX-License-Identifier: MIT */
/*
 * Calculator, SYS:Utilities/Calc, on the Workbench screen in RAM.
 * Workbench starts it; the root program types key sequences and clicks a
 * button through input.device and compares the display gadget with the
 * expected text drawn in a bitmap of the same font and pens.  Then a
 * Shell runs it.  It reports Calculator's stack use.
 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <devices/input.h>
#include <devices/inputevent.h>
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

struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Library *WorkbenchBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL calculator: %s\n", what);
		fails++;
	}
}

static struct MsgPort *ioport;
static struct IOStdReq *io;
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
static struct Task *calc;

/* Until Calculator waits with its messages read */
static void idle(void)
{
	int i;

	for (i = 0; i < 500; i++) {
		Delay(1);
		if (calc->tc_State == TS_WAIT &&
		    win->UserPort->mp_MsgList.lh_Head->ln_Succ == NULL)
			return;
	}
	check(FALSE, "Calculator idle");
}

static void key(UWORD code, UWORD qual)
{
	event(IECLASS_RAWKEY, code, qual, 0, 0);
	event(IECLASS_RAWKEY, code | IECODE_UP_PREFIX, qual, 0, 0);
	idle();
}

/* The raw keys: the main block's digits and letters, the keypad's
 * operators */
static UBYTE rows[] = "qwertyuiop\0\0\0\0\0\0asdfghjkl\0\0\0\0\0\0\0\0zxcvbnm";

static void type(char *s)
{
	char c, *r;

	for (; (c = *s) != 0; s++) {
		if (c >= '1' && c <= '9')
			key(c - '0', 0);
		else if (c == '0')
			key(0x0A, 0);
		else if (c == '.')
			key(0x39, 0);
		else if (c == '+')
			key(0x5E, IEQUALIFIER_NUMERICPAD);
		else if (c == '-')
			key(0x4A, IEQUALIFIER_NUMERICPAD);
		else if (c == '*')
			key(0x5D, IEQUALIFIER_NUMERICPAD);
		else if (c == '/')
			key(0x3A, 0);
		else if (c == '=')
			key(0x0C, 0);
		else if (c == '\r')
			key(0x44, 0);
		else if (c == '\b')
			key(0x41, 0);
		else {
			for (r = (char *)rows; *r != c; r++)
				;
			key(0x10 + (r - (char *)rows), 0);
		}
	}
}

static struct Window *calcwin(void)
{
	struct Window *w;
	char *t, *a;

	for (w = wb->FirstWindow; w; w = w->NextWindow) {
		if ((t = w->Title) == NULL)
			continue;
		for (a = "Calculator"; *a && *a == *t; a++, t++)
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
		w = calcwin();
		if ((w != NULL) == open)
			break;
		Delay(1);
	}
	return w;
}

static struct Gadget *gadget(UWORD id)
{
	struct Gadget *g;

	for (g = win->FirstGadget; g; g = g->NextGadget)
		if ((g->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET &&
		    g->GadgetID == id)
			return g;
	return NULL;
}

static void click(struct Gadget *g)
{
	WORD x = win->LeftEdge + g->LeftEdge + g->Width / 2;
	WORD y = win->TopEdge + g->TopEdge + g->Height / 2;

	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, 0, x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON, 0, x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, 0, x, y);
	idle();
}

#define DISPLAY_ID	100
/* The display's text: the memory mark, then the value right-aligned,
 * filling the gadget */
#define DISPLAY_MAX	40
/* gadtools' text frame and the space inside it */
#define FRAME		3

static struct BitMap *refbm;
static struct RastPort ref;

/* Whether the display shows s, and the memory mark when mem */
static BOOL shows(char *s, BOOL mem)
{
	struct Gadget *g = gadget(DISPLAY_ID);
	char buf[DISPLAY_MAX];
	WORD i, n, x, y, w, h, tx, len;

	if (g == NULL)
		return FALSE;
	w = g->Width - 2 * FRAME;
	h = g->Height - 2 * FRAME;
	len = w / ref.TxWidth;
	for (n = 0; s[n]; n++)
		;
	for (i = 0; i < len; i++)
		buf[i] = i >= len - n ? s[i - (len - n)] : ' ';
	if (mem)
		buf[0] = 'M';
	tx = w - TextLength(&ref, buf, len);
	SetAPen(&ref, dri->dri_Pens[BACKGROUNDPEN]);
	RectFill(&ref, 0, 0, w - 1, h - 1);
	SetAPen(&ref, dri->dri_Pens[TEXTPEN]);
	SetDrMd(&ref, JAM1);
	Move(&ref, tx, (h - ref.TxHeight) / 2 + ref.TxBaseline);
	Text(&ref, buf, len);
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++)
			if (ReadPixel(&ref, x, y) != ReadPixel(win->RPort,
			    g->LeftEdge + FRAME + x, g->TopEdge + FRAME + y)) {
				xKPrintf("calculator: \"%s\": pixel %d,%d\n", s, x, y);
				return FALSE;
			}
	return TRUE;
}

static void stackuse(struct Task *t)
{
	UBYTE *p, *top;

	p = (UBYTE *)t->tc_SPLower + 8;
	top = (UBYTE *)t->tc_SPUpper;
	while (p < top && *p == 0)
		p++;
	xKPrintf("calculator: %s stack %d of %d bytes used\n",
	    t->tc_Node.ln_Name, (int)(top - p),
	    (int)(top - (UBYTE *)t->tc_SPLower));
}

static void sums(void)
{
	struct Gadget *g;

	check(shows("0", FALSE), "start");
	type("12+34=");
	check(shows("46", FALSE), "12+34=");
	type("a2+3*4=");
	check(shows("20", FALSE), "chained operators");
	type("a7/2=");
	check(shows("3.5", FALSE), "7/2=");
	type("=");
	check(shows("1.75", FALSE), "repeated =");
	type("a1/3\r");
	check(shows("0.3333333333", FALSE), "1/3, Return");
	type("a5-8=");
	check(shows("-3", FALSE), "5-8=");
	type("s");
	check(shows("3", FALSE), "sign");
	type("*2=");
	check(shows("6", FALSE), "sign then *2");
	type("a2*-");
	check(shows("2", FALSE), "operator replaced");
	type("5=");
	check(shows("-3", FALSE), "2-5=");
	type("a.5*.5=");
	check(shows("0.25", FALSE), ".5*.5=");
	type("a123\b");
	check(shows("12", FALSE), "backspace");
	type("e");
	check(shows("0", FALSE), "CE");
	type("a1/0=");
	check(shows("Error", FALSE), "1/0");
	type("5");
	check(shows("Error", FALSE), "keys ignored after an error");
	type("e");
	check(shows("0", FALSE), "CE clears an error");
	type("9999999999999+1=");
	check(shows("Error", FALSE), "overflow");
	type("a1234567*1234567=");
	check(shows("1524155677489", FALSE), "13 digits");

	/* Memory */
	type("a25m");
	check(shows("25", TRUE), "M+");
	type("4*r=");
	check(shows("100", TRUE), "MR");
	type("3n");
	check(shows("3", TRUE), "M-");
	type("ar");
	check(shows("22", TRUE), "MR after M-");
	type("c");
	check(shows("22", FALSE), "MC");

	/* A button */
	type("a");
	check((g = gadget(4)) != NULL, "button 7");
	if (g != NULL) {
		click(g);
		check(shows("7", FALSE), "button 7 clicked");
	}
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
	WORD x, y;

	check(OpenWorkbenchObjectA("SYS:Utilities/Calc", NULL), "launch");
	win = waitwin(TRUE);
	check(win != NULL, "Calculator's window");
	if (win == NULL)
		return;
	calc = win->UserPort->mp_SigTask;
	idle();
	sums();
	stackuse(calc);

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
	static char cmd[] = "SYS:Utilities/Calc\n";
	BPTR fh;

	if ((fh = Open("SYS:CalcScript", MODE_NEWFILE)) == NULL)
		return FALSE;
	Write(fh, cmd, sizeof(cmd) - 1);
	Close(fh);
	return TRUE;
}

static LONG shell(char *args, LONG len)
{
	BPTR in;
	LONG rc;

	if ((in = Open("SYS:CalcScript", MODE_OLDFILE)) == NULL)
		return -1;
	rc = Execute("", in, NULL);
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
	s.s_Task = FindTask(NULL);
	s.s_RC = -1;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (IPTR)shell;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (IPTR)"CalcShell";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 4096;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (IPTR)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (IPTR)&s;
	t[5].ti_Tag = TAG_DONE;
	check(CreateNewProc(t) != NULL, "Shell process");
	win = waitwin(TRUE);
	check(win != NULL, "Calculator from the Shell");
	if (win == NULL)
		return;
	calc = win->UserPort->mp_SigTask;
	idle();
	type("6*7=");
	check(shows("42", FALSE), "Shell: 6*7=");
	Signal(calc, SIGBREAKF_CTRL_C);
	Wait(SIGBREAKF_CTRL_F);
	check(s.s_RC == 0, "Shell: return code");
	check(calcwin() == NULL, "Shell: window closed");
}

int main(void)
{
	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	WorkbenchBase = OpenLibrary(WORKBENCH_NAME, 45L);
	ioport = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(ioport, sizeof(struct IOStdReq));
	if (DOSBase == NULL || GfxBase == NULL || IntuitionBase == NULL ||
	    WorkbenchBase == NULL ||
	    OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL calculator: OpenLibrary\n");
		return 1;
	}
	check(OpenWorkBench() != 0, "OpenWorkBench");
	wb = LockPubScreen("Workbench");
	dri = GetScreenDrawInfo(wb);
	refbm = AllocBitMap(256, 32, GetBitMapAttr(wb->RastPort.BitMap,
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
	KPutStr("PASS calculator\n");
	return 0;
}
