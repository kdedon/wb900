/*
 * Edit from SYS:Utilities, on the Workbench screen in RAM.  Workbench
 * opens a test file with it; the root program types into it through
 * input.device and compares the text rows with lines drawn in a bitmap
 * of the same font: inserting, splitting and joining lines, saving,
 * undo, Find, Save As and Quit, checking the files written.  Then More
 * starts Edit on its file with E, and a Shell runs Edit with a file
 * argument.  It reports Edit's stack use.
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
		xKPrintf("FAIL edit: %s\n", what);
		fails++;
	}
}

static struct MsgPort *ioport;
static struct IOStdReq *io;
static struct InputEvent ev;

static void key(UWORD code, UWORD qual)
{
	WORD i;

	for (i = 0; i < 2; i++) {
		ev.ie_NextEvent = NULL;
		ev.ie_Class = IECLASS_RAWKEY;
		ev.ie_SubClass = 0;
		ev.ie_Code = code | (i ? IECODE_UP_PREFIX : 0);
		ev.ie_Qualifier = qual;
		ev.ie_X = ev.ie_Y = 0;
		io->io_Command = IND_WRITEEVENT;
		io->io_Data = &ev;
		io->io_Length = sizeof(ev);
		DoIO((struct IORequest *)io);
	}
}

#define AMIGA	IEQUALIFIER_RCOMMAND
#define SHIFT	IEQUALIFIER_LSHIFT

static struct Task *task;

/* A key, then a pause until the task waits: an ASL file gadget ends on
 * each key and starts again */
static void keywait(UWORD code, UWORD qual)
{
	int i, n;

	key(code, qual);
	for (i = n = 0; i < 100 && n < 2; i++) {
		Delay(1);
		n = task->tc_State == TS_WAIT ? n + 1 : 0;
	}
}

/* The raw keys of a US keyboard */
static UBYTE rows[] = "qwertyuiop\0\0\0\0\0\0asdfghjkl\0\0\0\0\0\0\0\0zxcvbnm";

static void type(char *s)
{
	UWORD qual, code;
	char c, *r;

	for (; (c = *s) != 0; s++) {
		qual = 0;
		if (c >= 'A' && c <= 'Z') {
			c += 'a' - 'A';
			qual = SHIFT;
		}
		if (c >= '1' && c <= '9')
			code = c - '0';
		else if (c == '0')
			code = 0x0A;
		else if (c == ' ')
			code = 0x40;
		else if (c == ':') {
			code = 0x29;
			qual = SHIFT;
		} else {
			for (r = (char *)rows; *r != c; r++)
				;
			code = 0x10 + (r - (char *)rows);
		}
		keywait(code, qual);
	}
}

static struct Screen *wb;
static struct Window *win;

static BOOL contains(char *t, char *s)
{
	char *a, *b;

	for (; t && *t; t++) {
		for (a = t, b = s; *b && *a == *b; a++, b++)
			;
		if (*b == 0)
			return TRUE;
	}
	return FALSE;
}

/* A window whose title holds s, waiting for it up to 10 s */
static struct Window *findwin(char *s, BOOL open)
{
	struct Window *w = NULL;
	int i;

	ULONG lock;

	for (i = 0; i < 500; i++) {
		lock = LockIBase(0);
		for (w = wb->FirstWindow; w; w = w->NextWindow)
			if (contains(w->Title, s))
				break;
		UnlockIBase(lock);
		if ((w != NULL) == open)
			break;
		Delay(1);
	}
	return w;
}

/* Until the task waits with its window's messages read (a busy window has no port) */
static void idle(void)
{
	struct MsgPort *port;
	int i;

	for (i = 0; i < 500; i++) {
		Delay(1);
		port = win->UserPort;
		if (task->tc_State == TS_WAIT &&
		    (port == NULL || port->mp_MsgList.lh_Head->ln_Succ == NULL))
			return;
	}
	check(FALSE, "idle");
}

/* Until the active window's title holds s */
static void waitactive(char *s)
{
	int i;

	ULONG lock;
	BOOL found;

	for (i = 0; i < 500; i++) {
		Delay(1);
		lock = LockIBase(0);
		found = IntuitionBase->ActiveWindow &&
		    contains(IntuitionBase->ActiveWindow->Title, s);
		UnlockIBase(lock);
		if (found)
			break;
	}
	check(i < 500, s);
	/* A requester activates its gadget through the input handler */
	idle();
	ev.ie_Class = IECLASS_NULL;
	io->io_Command = IND_WRITEEVENT;
	io->io_Data = &ev;
	io->io_Length = sizeof(ev);
	DoIO((struct IORequest *)io);
	Delay(5);
}

static BOOL writefile(char *name, char *text)
{
	BPTR fh;
	LONG len;

	if ((fh = Open(name, MODE_NEWFILE)) == NULL)
		return FALSE;
	for (len = 0; text[len]; len++)
		;
	Write(fh, text, len);
	Close(fh);
	return TRUE;
}

/* Whether the file holds exactly text */
static BOOL filehas(char *name, char *text)
{
	static char buf[300];
	BPTR fh;
	LONG len, n;

	if ((fh = Open(name, MODE_OLDFILE)) == NULL)
		return FALSE;
	len = Read(fh, buf, sizeof(buf));
	Close(fh);
	for (n = 0; text[n]; n++)
		if (n >= len || buf[n] != text[n])
			return FALSE;
	return n == len;
}

static char original[] = "Line 001\nLine 002\nLine 003\nLine 004\n"
    "Line 005\nLine 006\nLine 007\nLine 008\n";

/* Edit's text area: the default font at the window's inner corner */
static WORD fw, fh, x0, y0;
static struct BitMap *refbm;
static struct RastPort ref;
static struct DrawInfo *dri;

/* Whether row r shows s, but for column skip (the cursor) */
#define COLS	16
static BOOL row(WORD r, char *s, WORD skip)
{
	WORD x, y, len;

	for (len = 0; s[len]; len++)
		;
	SetAPen(&ref, dri->dri_Pens[BACKGROUNDPEN]);
	RectFill(&ref, 0, 0, COLS * fw - 1, fh - 1);
	SetABPenDrMd(&ref, dri->dri_Pens[TEXTPEN],
	    dri->dri_Pens[BACKGROUNDPEN], JAM2);
	Move(&ref, 0, ref.TxBaseline);
	if (len > 0)
		Text(&ref, s, len);
	for (y = 0; y < fh; y++)
		for (x = 0; x < COLS * fw; x++)
			if (x / fw != skip && ReadPixel(&ref, x, y) !=
			    ReadPixel(win->RPort, x0 + x, y0 + r * fh + y)) {
				xKPrintf("edit: row %d \"%s\": pixel %d,%d\n",
				    r, s, x, y);
				return FALSE;
			}
	return TRUE;
}

static void stackuse(struct Task *t)
{
	UBYTE *p, *top;

	p = (UBYTE *)t->tc_SPLower + 2;
	top = (UBYTE *)t->tc_SPUpper;
	while (p < top && *p == 0)
		p++;
	xKPrintf("edit: %s stack %d of %d bytes used\n", t->tc_Node.ln_Name,
	    (int)(top - p), (int)(top - (UBYTE *)t->tc_SPLower));
}

/* Edit's window on SYS:name */
static BOOL edited(char *name)
{
	win = findwin(name, TRUE);
	check(win != NULL, name);
	if (win == NULL)
		return FALSE;
	task = win->UserPort->mp_SigTask;
	fw = GfxBase->DefaultFont->tf_XSize;
	fh = GfxBase->DefaultFont->tf_YSize;
	x0 = win->BorderLeft;
	y0 = win->BorderTop;
	idle();
	return TRUE;
}

/* Whether t is still a task */
static BOOL alive(struct Task *t)
{
	struct Node *n;
	BOOL found = FALSE;

	Disable();
	for (n = SysBase->TaskReady.lh_Head; n->ln_Succ; n = n->ln_Succ)
		found |= n == &t->tc_Node;
	for (n = SysBase->TaskWait.lh_Head; n->ln_Succ; n = n->ln_Succ)
		found |= n == &t->tc_Node;
	Enable();
	return found;
}

static void quit(char *name)
{
	int i;

	key(0x10, AMIGA);			/* Amiga-Q */
	check(findwin(name, FALSE) == NULL, "quit");
	for (i = 0; i < 200 && alive(task); i++)
		Delay(1);
	check(i < 200, "Edit ends");
}

static void workbench(void)
{
	struct TagItem t[3];
	BPTR sys;

	sys = Lock("SYS:", SHARED_LOCK);
	t[0].ti_Tag = WBOPENA_ArgLock;	t[0].ti_Data = (IPTR)sys;
	t[1].ti_Tag = WBOPENA_ArgName;	t[1].ti_Data = (IPTR)"EditTest";
	t[2].ti_Tag = TAG_DONE;
	check(OpenWorkbenchObjectA("SYS:Utilities/Edit", t), "launch");
	UnLock(sys);
	if (!edited("EditTest"))
		return;
	check(row(0, "Line 001", 0) && row(1, "Line 002", -1) &&
	    row(7, "Line 008", -1) && row(8, "", -1), "file shown");

	type("Hi ");
	idle();
	check(row(0, "Hi Line 001", 3), "typed");
	check(contains(win->Title, "EditTest+") &&
	    contains(win->Title, "(4, 1)"), "title: modified, column 4");

	key(0x4D, 0);				/* cursor down */
	key(0x44, 0);				/* Return splits the line */
	idle();
	check(row(1, "Lin", -1) && row(2, "e 002", 0) &&
	    row(3, "Line 003", -1) && contains(win->Title, "(1, 3)"),
	    "split");
	key(0x41, 0);				/* Backspace joins it */
	idle();
	check(row(1, "Line 002", 3) && row(2, "Line 003", -1), "join");

	key(0x11, AMIGA);			/* Amiga-W saves */
	idle();
	check(filehas("SYS:EditTest", "Hi Line 001\nLine 002\nLine 003\n"
	    "Line 004\nLine 005\nLine 006\nLine 007\nLine 008\n"), "saved");
	check(!contains(win->Title, "+"), "title: saved");

	key(0x16, AMIGA);			/* Amiga-U undoes the join */
	idle();
	check(row(1, "Lin", 3) && row(2, "e 002", -1) &&
	    contains(win->Title, "+"), "undo");

	/* Find opens its window; Return finds, Esc closes */
	key(0x21, AMIGA);			/* Amiga-S */
	waitactive("Find");
	type("Line 007");
	key(0x44, 0);
	Delay(10);
	key(0x45, 0);
	waitactive("EditTest");
	idle();
	check(contains(win->Title, "(1, 8)"), "find");

	/* Shift-Amiga-W: Save As */
	key(0x11, AMIGA | SHIFT);
	waitactive("Save");
	keywait(0x32, IEQUALIFIER_CONTROL);	/* Ctrl-X clears */
	type("EditOut");
	key(0x44, 0);
	waitactive("EditOut");
	idle();
	check(filehas("SYS:EditOut", "Hi Line 001\nLin\ne 002\nLine 003\n"
	    "Line 004\nLine 005\nLine 006\nLine 007\nLine 008\n"),
	    "saved as, after undo");
	stackuse(task);
	quit("EditOut");
}

/* More's E key opens its file in Edit */
static void more(void)
{
	struct TagItem t[3];
	struct Window *mw;
	BPTR sys;

	sys = Lock("SYS:", SHARED_LOCK);
	t[0].ti_Tag = WBOPENA_ArgLock;	t[0].ti_Data = (IPTR)sys;
	t[1].ti_Tag = WBOPENA_ArgName;	t[1].ti_Data = (IPTR)"EditOut";
	t[2].ti_Tag = TAG_DONE;
	check(OpenWorkbenchObjectA("SYS:Utilities/More", t), "More");
	UnLock(sys);
	mw = findwin("More:", TRUE);
	check(mw != NULL, "More's window");
	if (mw == NULL)
		return;
	Delay(20);
	type("e");
	if (edited("EditOut    (")) {
		check(row(0, "Hi Line 001", 0), "Edit from More");
		quit("EditOut    (");
	}
	ActivateWindow(mw);
	Delay(10);
	key(0x45, 0);
	check(findwin("More:", FALSE) == NULL, "More quits");
}

struct sync {
	struct Task *s_Task;
	LONG s_RC;
};

static struct sync s;

static LONG shell(char *args, LONG len)
{
	BPTR in;
	LONG rc;

	if ((in = Open("SYS:EditScript", MODE_OLDFILE)) == NULL)
		return RETURN_FAIL;
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

	check(writefile("SYS:EditScript", "SYS:Utilities/Edit SYS:EditOut\n"),
	    "script");
	s.s_Task = FindTask(NULL);
	s.s_RC = -1;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (IPTR)shell;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (IPTR)"EditShell";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 4096;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (IPTR)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (IPTR)&s;
	t[5].ti_Tag = TAG_DONE;
	check(CreateNewProc(t) != NULL, "Shell process");
	if (!edited("EditOut    ("))
		return;
	check(row(1, "Lin", -1), "Shell: file shown");
	stackuse(task);
	quit("EditOut    (");
	Wait(SIGBREAKF_CTRL_F);
	check(s.s_RC == 0, "Shell: return code");
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
		KPutStr("FAIL edit: OpenLibrary\n");
		return 1;
	}
	check(writefile("SYS:EditTest", original), "test file");
	check(OpenWorkBench() != 0, "OpenWorkBench");
	wb = LockPubScreen("Workbench");
	dri = GetScreenDrawInfo(wb);
	refbm = AllocBitMap(COLS * 8, 16, GetBitMapAttr(wb->RastPort.BitMap,
	    BMA_DEPTH), BMF_CLEAR, wb->RastPort.BitMap);
	InitRastPort(&ref);
	ref.BitMap = refbm;
	SetFont(&ref, GfxBase->DefaultFont);
	check(refbm != NULL, "bitmap");

	if (refbm != NULL) {
		workbench();
		more();
		cli();
	}

	FreeBitMap(refbm);
	FreeScreenDrawInfo(wb, dri);
	UnlockPubScreen(NULL, wb);
	if (fails != 0)
		return 1;
	KPutStr("PASS edit\n");
	return 0;
}
