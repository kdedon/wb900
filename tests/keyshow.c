/* SPDX-License-Identifier: MIT */
/*
 * KeyShow from SYS:Utilities, on the Workbench screen in RAM.  The root
 * program presses keys through input.device and finds the key KeyShow
 * highlights from the pixels that change: A, S beside it, Q above, the
 * C900's W1 and keypad 00 to the right.  A release restores the
 * picture, and Shift turns the a key's label into A.  Then a Shell runs
 * it.  It reports KeyShow's stack use.
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
		xKPrintf("FAIL keyshow: %s\n", what);
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
static struct Task *ks;

static void idle(void)
{
	int i;

	for (i = 0; i < 500; i++) {
		Delay(1);
		if (ks->tc_State == TS_WAIT &&
		    win->UserPort->mp_MsgList.lh_Head->ln_Succ == NULL)
			return;
	}
	check(FALSE, "KeyShow idle");
}

static void down(UWORD code, UWORD qual)
{
	event(IECLASS_RAWKEY, code, qual, 0, 0);
	idle();
}

static void up(UWORD code, UWORD qual)
{
	event(IECLASS_RAWKEY, code | IECODE_UP_PREFIX, qual, 0, 0);
	idle();
}

static struct Window *kswin(void)
{
	struct Window *w;
	char *t, *a;

	for (w = wb->FirstWindow; w; w = w->NextWindow) {
		if ((t = w->Title) == NULL)
			continue;
		for (a = "KeyShow"; *a && *a == *t; a++, t++)
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
		w = kswin();
		if ((w != NULL) == open)
			break;
		Delay(1);
	}
	return w;
}

/* The window's inside, every other pixel each way */
#define SW	320
#define SH	80
static UBYTE base[SW * SH], shot[SW * SH];
static WORD sw, sh;

static void snap(UBYTE *buf)
{
	WORD x, y;

	sw = (win->Width - win->BorderLeft - win->BorderRight) / 2;
	sh = (win->Height - win->BorderTop - win->BorderBottom) / 2;
	if (sw > SW)
		sw = SW;
	if (sh > SH)
		sh = SH;
	for (y = 0; y < sh; y++)
		for (x = 0; x < sw; x++)
			buf[y * SW + x] = ReadPixel(win->RPort,
			    win->BorderLeft + 2 * x, win->BorderTop + 2 * y);
}

struct box {
	WORD x0, y0, x1, y1;
};

/* The box of the pixels that differ from base, in window pixels */
static BOOL changed(struct box *b)
{
	WORD x, y;
	BOOL any = FALSE;

	snap(shot);
	b->x0 = b->y0 = 32767;
	b->x1 = b->y1 = -1;
	for (y = 0; y < sh; y++)
		for (x = 0; x < sw; x++)
			if (shot[y * SW + x] != base[y * SW + x]) {
				any = TRUE;
				if (x < b->x0) b->x0 = x;
				if (x > b->x1) b->x1 = x;
				if (y < b->y0) b->y0 = y;
				if (y > b->y1) b->y1 = y;
			}
	b->x0 = win->BorderLeft + 2 * b->x0;
	b->x1 = win->BorderLeft + 2 * b->x1;
	b->y0 = win->BorderTop + 2 * b->y0;
	b->y1 = win->BorderTop + 2 * b->y1;
	return any;
}

#define CX(b)	(((b)->x0 + (b)->x1) / 2)
#define CY(b)	(((b)->y0 + (b)->y1) / 2)

/* The key code highlights while down: its box, and the picture back
 * after the release */
static BOOL key(UWORD code, struct box *b, char *what)
{
	struct box r;
	BOOL ok;

	down(code, 0);
	ok = changed(b);
	check(ok, what);
	check(!ok || (b->x1 - b->x0 < 80 && b->y1 - b->y0 < 40), what);
	up(code, 0);
	check(!changed(&r), "released");
	return ok;
}

static struct BitMap *refbm;
static struct RastPort ref;

/* Whether text s is drawn somewhere in box b */
static BOOL label(char *s, struct box *b)
{
	WORD n, w, x, y, i, j;
	UBYTE bg = dri->dri_Pens[BACKGROUNDPEN];

	for (n = 0; s[n]; n++)
		;
	w = TextLength(&ref, s, n);
	SetABPenDrMd(&ref, dri->dri_Pens[TEXTPEN], bg, JAM2);
	Move(&ref, 0, ref.TxBaseline);
	Text(&ref, s, n);
	for (y = b->y0; y + ref.TxHeight <= b->y1 + 2; y++)
		for (x = b->x0; x + w <= b->x1 + 2; x++) {
			for (j = 0; j < ref.TxHeight; j++)
				for (i = 0; i < w; i++)
					if (ReadPixel(&ref, i, j) !=
					    ReadPixel(win->RPort, x + i, y + j))
						goto next;
			return TRUE;
next:			;
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
	xKPrintf("keyshow: %s stack %d of %d bytes used\n",
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
	struct box a, s, q, w1, kp, sh;
	struct Gadget *g;
	WORD x, y;

	check(OpenWorkbenchObjectA("SYS:Utilities/KeyShow", NULL), "launch");
	win = waitwin(TRUE);
	check(win != NULL, "KeyShow's window");
	if (win == NULL)
		return;
	ks = win->UserPort->mp_SigTask;
	idle();
	snap(base);

	if (key(0x20, &a, "A highlighted") && key(0x21, &s, "S highlighted") &&
	    key(0x10, &q, "Q highlighted") && key(0x2B, &w1, "W1 highlighted") &&
	    key(0x0E, &kp, "keypad 00 highlighted")) {
		check(CY(&s) == CY(&a) && CX(&s) > CX(&a) &&
		    CX(&s) - CX(&a) < 40, "S beside A");
		check(CY(&q) < CY(&a) && CX(&q) < CX(&a), "Q above A");
		check(CX(&w1) > CX(&a) + 200, "W1 right of the main keys");
		check(CX(&kp) > CX(&w1) && CY(&kp) > CY(&w1), "keypad 00");

		check(label("a", &a), "a's label");
		down(0x60, IEQUALIFIER_LSHIFT);
		check(changed(&sh), "Shift highlighted");
		check(label("A", &a), "Shift: A's label");
		up(0x60, 0);
		check(label("a", &a), "a's label again");
		check(!changed(&sh), "Shift released");
	}
	stackuse(ks);

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
	static char cmd[] = "SYS:Utilities/KeyShow\n";
	BPTR fh;

	if ((fh = Open("SYS:KeyScript", MODE_NEWFILE)) == NULL)
		return FALSE;
	Write(fh, cmd, sizeof(cmd) - 1);
	Close(fh);
	return TRUE;
}

static LONG shell(char *args, LONG len)
{
	BPTR in;
	LONG rc;

	if ((in = Open("SYS:KeyScript", MODE_OLDFILE)) == NULL)
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
	struct box b;

	check(script(), "script");
	s.s_Task = FindTask(NULL);
	s.s_RC = -1;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (IPTR)shell;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (IPTR)"KeyShell";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 4096;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (IPTR)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (IPTR)&s;
	t[5].ti_Tag = TAG_DONE;
	check(CreateNewProc(t) != NULL, "Shell process");
	win = waitwin(TRUE);
	check(win != NULL, "KeyShow from the Shell");
	if (win == NULL)
		return;
	ks = win->UserPort->mp_SigTask;
	idle();
	snap(base);
	key(0x45, &b, "Shell: Esc highlighted");
	Signal(ks, SIGBREAKF_CTRL_C);
	Wait(SIGBREAKF_CTRL_F);
	check(s.s_RC == 0, "Shell: return code");
	check(kswin() == NULL, "Shell: window closed");
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
		KPutStr("FAIL keyshow: OpenLibrary\n");
		return 1;
	}
	check(OpenWorkBench() != 0, "OpenWorkBench");
	wb = LockPubScreen("Workbench");
	dri = GetScreenDrawInfo(wb);
	refbm = AllocBitMap(64, 16, GetBitMapAttr(wb->RastPort.BitMap,
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
	KPutStr("PASS keyshow\n");
	return 0;
}
