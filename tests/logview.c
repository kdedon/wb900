/* SPDX-License-Identifier: MIT */
/*
 * LogView from SYS:Utilities, on the Workbench screen in RAM.  Workbench
 * opens a log with it, as it opens a project; the root program compares
 * the list's rows with the expected lines drawn in a bitmap of the same
 * font: the log's end, after the log grew and Refresh was clicked or R
 * pressed, and another log typed in the path gadget.  Then a Shell runs
 * it with a file.  It reports LogView's stack use.
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
		xKPrintf("FAIL logview: %s\n", what);
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
static struct Task *lg;

static struct Window *lgwin(void)
{
	struct Window *w;
	char *t, *a;

	for (w = wb->FirstWindow; w; w = w->NextWindow) {
		if ((t = w->Title) == NULL)
			continue;
		for (a = "LogView"; *a && *a == *t; a++, t++)
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
		w = lgwin();
		if ((w != NULL) == open)
			break;
		Delay(1);
	}
	return w;
}

/* The widest gadget of an ID: a list view's scroller and arrows share it */
static struct Gadget *gadget(UWORD id)
{
	struct Gadget *g, *w = NULL;

	for (g = win->FirstGadget; g; g = g->NextGadget)
		if ((g->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET &&
		    g->GadgetID == id && (w == NULL || g->Width > w->Width))
			w = g;
	return w;
}

/* Until LogView waits with its messages read */
static void idle(void)
{
	int i;

	for (i = 0; i < 500; i++) {
		Delay(1);
		if (lg->tc_State == TS_WAIT &&
		    win->UserPort->mp_MsgList.lh_Head->ln_Succ == NULL)
			return;
	}
	check(FALSE, "LogView idle");
}

static void key(UWORD code, UWORD qual)
{
	event(IECLASS_RAWKEY, code, qual, 0, 0);
	event(IECLASS_RAWKEY, code | IECODE_UP_PREFIX, qual, 0, 0);
	idle();
}

/* The raw keys of a US keyboard */
static UBYTE keys[] = "qwertyuiop\0\0\0\0\0\0asdfghjkl\0\0\0\0\0\0\0\0zxcvbnm";

static void type(char *s)
{
	UWORD qual, code;
	char c, *r;

	for (; (c = *s) != 0; s++) {
		qual = 0;
		if (c >= 'A' && c <= 'Z') {
			c += 'a' - 'A';
			qual = IEQUALIFIER_LSHIFT;
		}
		if (c >= '1' && c <= '9')
			code = c - '0';
		else if (c == '0')
			code = 0x0A;
		else if (c == ':') {
			code = 0x29;
			qual = IEQUALIFIER_LSHIFT;
		} else {
			for (r = (char *)keys; *r != c; r++)
				;
			code = 0x10 + (r - (char *)keys);
		}
		key(code, qual);
	}
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

/* gadtools' list view frame */
#define LVX	4
#define LVY	3

static struct BitMap *refbm;
static struct RastPort ref;

static char line[20];

/* Log line n, or the empty row for 0 */
static char *text(WORD n)
{
	char *p = line;

	if (n > 0) {
		*p++ = 'L'; *p++ = 'o'; *p++ = 'g'; *p++ = ' ';
		*p++ = 'l'; *p++ = 'i'; *p++ = 'n'; *p++ = 'e'; *p++ = ' ';
		*p++ = '0' + n / 10;
		*p++ = '0' + n % 10;
	}
	*p = 0;
	return line;
}

/* Whether row r shows log line n, its text then blanks */
static BOOL row(WORD r, WORD n)
{
	struct Gadget *lv = gadget(1);
	WORD x, y, w = 16 * ref.TxWidth, len;
	char *s = text(n);

	if (lv == NULL)
		return FALSE;
	for (len = 0; s[len]; len++)
		;
	SetAPen(&ref, dri->dri_Pens[BACKGROUNDPEN]);
	RectFill(&ref, 0, 0, w - 1, ref.TxHeight - 1);
	SetABPenDrMd(&ref, dri->dri_Pens[TEXTPEN],
	    dri->dri_Pens[BACKGROUNDPEN], JAM2);
	Move(&ref, 0, ref.TxBaseline);
	Text(&ref, s, len);
	for (y = 0; y < ref.TxHeight; y++)
		for (x = 0; x < w; x++)
			if (ReadPixel(&ref, x, y) != ReadPixel(win->RPort,
			    lv->LeftEdge + LVX + x,
			    lv->TopEdge + LVY + r * ref.TxHeight + y)) {
				xKPrintf("logview: row %d, line %d: pixel %d,%d\n",
				    r, n, x, y);
				return FALSE;
			}
	return TRUE;
}

static WORD rows(void)
{
	struct Gadget *lv = gadget(1);

	return lv ? (lv->Height - 2 * LVY) / ref.TxHeight : 0;
}

/* The list shows the last lines of an n-line log */
static BOOL tail(WORD n)
{
	WORD v = rows();

	return v > 2 && row(0, n - v + 1) && row(v - 1, n);
}

static BOOL writelog(char *name, WORD n)
{
	BPTR fh;
	WORD i, len;

	if ((fh = Open(name, MODE_NEWFILE)) == NULL)
		return FALSE;
	for (i = 1; i <= n; i++) {
		text(i);
		for (len = 0; line[len]; len++)
			;
		line[len++] = '\n';
		Write(fh, line, len);
	}
	Close(fh);
	return TRUE;
}

static void stackuse(struct Task *t)
{
	UBYTE *p, *top;

	p = (UBYTE *)t->tc_SPLower + 8;
	top = (UBYTE *)t->tc_SPUpper;
	while (p < top && *p == 0)
		p++;
	xKPrintf("logview: %s stack %d of %d bytes used\n",
	    t->tc_Node.ln_Name, (int)(top - p),
	    (int)(top - (UBYTE *)t->tc_SPLower));
}

static void workbench(void)
{
	struct TagItem t[3];
	struct Gadget *g;
	BPTR sys;

	check(writelog("SYS:LogTest", 30) && writelog("SYS:LogTest2", 3),
	    "test logs");
	sys = Lock("SYS:", SHARED_LOCK);
	t[0].ti_Tag = WBOPENA_ArgLock;	t[0].ti_Data = (IPTR)sys;
	t[1].ti_Tag = WBOPENA_ArgName;	t[1].ti_Data = (IPTR)"LogTest";
	t[2].ti_Tag = TAG_DONE;
	check(OpenWorkbenchObjectA("SYS:Utilities/LogView", t), "launch");
	UnLock(sys);
	win = waitwin(TRUE);
	check(win != NULL, "LogView's window");
	if (win == NULL)
		return;
	lg = win->UserPort->mp_SigTask;
	idle();
	check(tail(30), "the log's end");

	writelog("SYS:LogTest", 35);
	check((g = gadget(3)) != NULL, "Refresh");
	if (g != NULL)
		click(g);
	check(tail(35), "Refresh clicked");

	writelog("SYS:LogTest", 40);
	key(0x13, 0);
	check(tail(40), "R pressed");

	/* Another log */
	if ((g = gadget(2)) != NULL) {
		click(g);
		key(0x32, IEQUALIFIER_CONTROL);	/* Ctrl-X clears */
		type("SYS:LogTest2");
		key(0x44, 0);
	}
	check(row(0, 1) && row(2, 3) && row(3, 0), "a typed path");
	stackuse(lg);

	event(IECLASS_RAWKEY, 0x45, 0, 0, 0);
	event(IECLASS_RAWKEY, 0x45 | IECODE_UP_PREFIX, 0, 0, 0);
	check(waitwin(FALSE) == NULL, "Esc quits");
}

struct sync {
	struct Task *s_Task;
	LONG s_RC;
};

static struct sync s;

static BOOL script(void)
{
	static char cmd[] = "SYS:Utilities/LogView SYS:LogTest\n";
	BPTR fh;

	if ((fh = Open("SYS:LogScript", MODE_NEWFILE)) == NULL)
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

	if ((in = Open("SYS:LogScript", MODE_OLDFILE)) == NULL)
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
	s.s_Task = FindTask(NULL);
	s.s_RC = -1;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (IPTR)shell;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (IPTR)"LogShell";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 4096;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (IPTR)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (IPTR)&s;
	t[5].ti_Tag = TAG_DONE;
	check(CreateNewProc(t) != NULL, "Shell process");
	win = waitwin(TRUE);
	check(win != NULL, "LogView from the Shell");
	if (win == NULL)
		return;
	lg = win->UserPort->mp_SigTask;
	idle();
	check(tail(40), "Shell: the log's end");
	Signal(lg, SIGBREAKF_CTRL_C);
	Wait(SIGBREAKF_CTRL_F);
	check(s.s_RC == 0, "Shell: return code");
	check(lgwin() == NULL, "Shell: window closed");
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
		KPutStr("FAIL logview: OpenLibrary\n");
		return 1;
	}
	check(OpenWorkBench() != 0, "OpenWorkBench");
	wb = LockPubScreen("Workbench");
	dri = GetScreenDrawInfo(wb);
	refbm = AllocBitMap(160, 16, GetBitMapAttr(wb->RastPort.BitMap,
	    BMA_DEPTH), BMF_CLEAR, wb->RastPort.BitMap);
	InitRastPort(&ref);
	ref.BitMap = refbm;
	SetFont(&ref, GfxBase->DefaultFont);
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
	KPutStr("PASS logview\n");
	return 0;
}
