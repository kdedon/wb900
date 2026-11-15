/* SPDX-License-Identifier: MIT */
/*
 * SysMon from SYS:Utilities, on the Workbench screen in RAM.  The root
 * program compares the list's rows with the expected text drawn in a
 * bitmap of the same font: input.device and SysMon itself, a process it
 * starts, the process's new priority, and the process gone after it
 * ends.  A dot drawn on an unchanged row stays, as only changed rows
 * are redrawn.  Then a Shell runs SysMon.  It reports SysMon's stack
 * use.
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
		xKPrintf("FAIL sysmon: %s\n", what);
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
static struct Task *sm;

static struct Window *smwin(void)
{
	struct Window *w;
	char *t, *a;

	for (w = wb->FirstWindow; w; w = w->NextWindow) {
		if ((t = w->Title) == NULL)
			continue;
		for (a = "SysMon"; *a && *a == *t; a++, t++)
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
		w = smwin();
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

/* gadtools' list view frame, and the rows SysMon shows */
#define LVX	4
#define LVY	3
#define ROWS	14
#define NAMELEN	16

static struct BitMap *refbm;
static struct RastPort ref;

/* Whether the text at x, y is s */
static BOOL drawn(WORD x0, WORD y0, char *s, WORD n)
{
	WORD x, y, w = TextLength(&ref, s, n);

	SetABPenDrMd(&ref, dri->dri_Pens[TEXTPEN],
	    dri->dri_Pens[BACKGROUNDPEN], JAM2);
	Move(&ref, 0, ref.TxBaseline);
	Text(&ref, s, n);
	for (y = 0; y < ref.TxHeight; y++)
		for (x = 0; x < w; x++)
			if (ReadPixel(&ref, x, y) !=
			    ReadPixel(win->RPort, x0 + x, y0 + y))
				return FALSE;
	return TRUE;
}

/* A task's row up to its state: name, priority, state */
static char row[NAMELEN + 11];

static void expect(char *name, LONG pri, char *state)
{
	WORD i;
	LONG n;

	for (i = 0; i < NAMELEN + 11; i++)
		row[i] = ' ';
	for (i = 0; name[i] && i < NAMELEN; i++)
		row[i] = name[i];
	i = NAMELEN + 5;
	n = pri < 0 ? -pri : pri;
	do
		row[--i] = '0' + n % 10;
	while ((n /= 10) != 0);
	if (pri < 0)
		row[--i] = '-';
	for (i = 0; state[i]; i++)
		row[NAMELEN + 6 + i] = state[i];
}

/* The visible row that shows row[], or -1 */
static WORD findrow(void)
{
	struct Gadget *lv = gadget(1);
	WORD r;

	if (lv == NULL)
		return -1;
	for (r = 0; r < ROWS; r++)
		if (drawn(lv->LeftEdge + LVX, lv->TopEdge + LVY + r * ref.TxHeight,
		    row, NAMELEN + 11))
			return r;
	return -1;
}

static WORD shown(char *name, LONG pri, char *state)
{
	expect(name, pri, state);
	return findrow();
}

/* A number gadget's text, right-aligned in its frame */
static BOOL number(UWORD id, char *s)
{
	struct Gadget *g = gadget(id);
	WORD n;

	if (g == NULL)
		return FALSE;
	for (n = 0; s[n]; n++)
		;
	return drawn(g->LeftEdge + g->Width - 3 - TextLength(&ref, s, n),
	    g->TopEdge + (g->Height - ref.TxHeight) / 2, s, n);
}

static void stackuse(struct Task *t)
{
	UBYTE *p, *top;

	p = (UBYTE *)t->tc_SPLower + 8;
	top = (UBYTE *)t->tc_SPUpper;
	while (p < top && *p == 0)
		p++;
	xKPrintf("sysmon: %s stack %d of %d bytes used\n",
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

static LONG probe(char *args, LONG len)
{
	Wait(SIGBREAKF_CTRL_C);
	return 0;
}

static void workbench(void)
{
	struct TagItem t[5];
	struct Task *input, *p;
	struct Gadget *g, *lv;
	WORD x, y, r;

	check(OpenWorkbenchObjectA("SYS:Utilities/SysMon", NULL), "launch");
	win = waitwin(TRUE);
	check(win != NULL, "SysMon's window");
	if (win == NULL)
		return;
	sm = win->UserPort->mp_SigTask;
	Delay(100);
	input = FindTask("input.device");
	check(input != NULL && shown("input.device", input->tc_Node.ln_Pri,
	    "wait") >= 0, "input.device listed");
	check(shown("SysMon", sm->tc_Node.ln_Pri, "run") >= 0, "SysMon listed");
	check(number(10, "0 K") && number(12, "0 K"), "no chip memory");

	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (IPTR)probe;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (IPTR)"SysMonProbe";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 2048;
	t[3].ti_Tag = NP_Priority;	t[3].ti_Data = -3;
	t[4].ti_Tag = TAG_DONE;
	p = (struct Task *)CreateNewProc(t);
	check(p != NULL, "probe");
	if (p == NULL)
		return;
	Delay(150);
	check(shown("SysMonProbe", -3, "wait") >= 0, "a new process listed");

	/* A dot in the blanks after input.device's name */
	lv = gadget(1);
	r = shown("input.device", input->tc_Node.ln_Pri, "wait");
	x = lv->LeftEdge + LVX + 13 * ref.TxWidth + 2;
	y = lv->TopEdge + LVY + r * ref.TxHeight + 2;
	SetAPen(win->RPort, dri->dri_Pens[FILLPEN]);
	WritePixel(win->RPort, x, y);
	SetTaskPri(p, 7);
	Delay(150);
	check(shown("SysMonProbe", 7, "wait") >= 0, "the new priority");
	check(ReadPixel(win->RPort, x, y) == dri->dri_Pens[FILLPEN],
	    "only changed rows redrawn");

	Signal(p, SIGBREAKF_CTRL_C);
	Delay(150);
	check(shown("SysMonProbe", 7, "wait") < 0, "an ended process gone");
	check(ReadPixel(win->RPort, x, y) != dri->dri_Pens[FILLPEN],
	    "a new list redrawn");
	stackuse(sm);

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
	static char cmd[] = "SYS:Utilities/SysMon\n";
	BPTR fh;

	if ((fh = Open("SYS:SysMonScript", MODE_NEWFILE)) == NULL)
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

	if ((in = Open("SYS:SysMonScript", MODE_OLDFILE)) == NULL)
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
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (IPTR)"SysMonShell";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 4096;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (IPTR)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (IPTR)&s;
	t[5].ti_Tag = TAG_DONE;
	check(CreateNewProc(t) != NULL, "Shell process");
	win = waitwin(TRUE);
	check(win != NULL, "SysMon from the Shell");
	if (win == NULL)
		return;
	sm = win->UserPort->mp_SigTask;
	Delay(100);
	check(shown("SysMonShell", 0, "wait") >= 0, "Shell: the Shell listed");
	Signal(sm, SIGBREAKF_CTRL_C);
	Wait(SIGBREAKF_CTRL_F);
	check(s.s_RC == 0, "Shell: return code");
	check(smwin() == NULL, "Shell: window closed");
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
		KPutStr("FAIL sysmon: OpenLibrary\n");
		return 1;
	}
	check(OpenWorkBench() != 0, "OpenWorkBench");
	wb = LockPubScreen("Workbench");
	dri = GetScreenDrawInfo(wb);
	refbm = AllocBitMap(320, 16, GetBitMapAttr(wb->RastPort.BitMap,
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
	KPutStr("PASS sysmon\n");
	return 0;
}
