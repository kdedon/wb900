/*
 * More from SYS:Utilities, on the Workbench screen in RAM.  Workbench
 * opens a test file with it, as it opens a project; the root program
 * writes keys and mouse events to input.device and compares the text
 * rows in More's window with the expected lines drawn in a bitmap of
 * the same font and pens: line and page scrolling, top and bottom,
 * horizontal scrolling, tab stops, the arrow gadget, Find and Jump,
 * Open from the menu, and redraws after another window covered it.
 * Then a process runs More from the Shell with the file as its
 * argument.  It reports More's stack use.
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
		xKPrintf("FAIL more: %s\n", what);
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

static void key(UWORD code, UWORD qual)
{
	event(IECLASS_RAWKEY, code, qual, 0, 0);
	event(IECLASS_RAWKEY, code | IECODE_UP_PREFIX, qual, 0, 0);
}

static struct Task *more;

/* A key, then a pause until the task waits: an ASL file gadget ends on
 * each key and starts again */
static void keywait(UWORD code, UWORD qual)
{
	int i, n;

	key(code, qual);
	for (i = n = 0; i < 100 && n < 2; i++) {
		Delay(1);
		n = more->tc_State == TS_WAIT ? n + 1 : 0;
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
			qual = IEQUALIFIER_LSHIFT;
		}
		if (c >= '1' && c <= '9')
			code = c - '0';
		else if (c == '0')
			code = 0x0A;
		else if (c == '.')
			code = 0x39;
		else if (c == ':') {
			code = 0x29;
			qual = IEQUALIFIER_LSHIFT;
		} else {
			for (r = (char *)rows; *r != c; r++)
				;
			code = 0x10 + (r - (char *)rows);
		}
		keywait(code, qual);
	}
}

static void button(UWORD code, WORD x, WORD y)
{
	event(IECLASS_RAWMOUSE, code, 0, x, y);
}

static struct Screen *wb;
static struct Window *win;
static struct DrawInfo *dri;

/* More's window, once its title is set */
static struct Window *morewin(void)
{
	struct Window *w;
	char *t;

	for (w = wb->FirstWindow; w; w = w->NextWindow)
		if ((t = w->Title) != NULL && t[0] == 'M' && t[1] == 'o' &&
		    t[2] == 'r' && t[3] == 'e' && t[4] == ':')
			return w;
	return NULL;
}

static struct Window *waitwin(BOOL open)
{
	struct Window *w = NULL;
	int i;

	for (i = 0; i < 500; i++) {
		w = morewin();
		if ((w != NULL) == open)
			break;
		Delay(1);
	}
	return w;
}

/* Until More waits with its messages read */
static void idle(void)
{
	int i;

	for (i = 0; i < 500; i++) {
		Delay(1);
		if (more->tc_State == TS_WAIT &&
		    win->UserPort->mp_MsgList.lh_Head->ln_Succ == NULL)
			return;
	}
	check(FALSE, "More idle");
}

/* Until a requester of More's is active and More waits */
static void waitreq(char *title)
{
	struct Window *w;
	char *a, *b;
	ULONG lock;
	BOOL found;
	int i, n;

	for (i = 0; i < 500; i++) {
		Delay(1);
		found = FALSE;
		lock = LockIBase(0);
		if ((w = IntuitionBase->ActiveWindow) != NULL && w->Title) {
			for (a = w->Title, b = title; *a && *a == *b; a++, b++)
				;
			found = *a == *b;
		}
		UnlockIBase(lock);
		if (found)
			break;
	}
	check(i < 500, title);
	for (i = n = 0; i < 100 && n < 2; i++) {
		Delay(1);
		n = more->tc_State == TS_WAIT ? n + 1 : 0;
	}
}

/* The test file's lines */
#define NLINES	120
#define LONGLINE 1
#define TABLINE	3
#define FINDLINE 70

static char line[200];
static LONG nlines = NLINES;

static char *text(LONG n)
{
	char *p = line;
	int i;

	if (n == TABLINE) {
		p[0] = 'a';
		p[1] = 'b';
		p[2] = '\t';
		p[3] = 'c';
		p[4] = 0;
		return line;
	}
	*p++ = 'L';
	*p++ = 'i';
	*p++ = 'n';
	*p++ = 'e';
	*p++ = ' ';
	*p++ = '0' + n / 100;
	*p++ = '0' + n / 10 % 10;
	*p++ = '0' + n % 10;
	if (n == LONGLINE)
		for (i = 0; i < 150; i++)
			*p++ = 'a' + i % 26;
	if (n == FINDLINE) {
		for (i = 0; i < 7; i++)
			*p++ = " needle"[i];
	}
	*p = 0;
	return line;
}

static BOOL writefile(char *name, LONG n)
{
	BPTR fh;
	LONG i, len;

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

static BOOL script(void)
{
	static char cmd[] = "SYS:Utilities/More SYS:MoreTest\n";
	BPTR fh;

	if ((fh = Open("SYS:MoreScript", MODE_NEWFILE)) == NULL)
		return FALSE;
	Write(fh, cmd, sizeof(cmd) - 1);
	Close(fh);
	return TRUE;
}

/* The layout More uses */
static WORD fw, fh, x0, y0, visible;

static void layout(void)
{
	fw = win->RPort->TxWidth;
	fh = win->RPort->TxHeight;
	x0 = win->BorderLeft + 2;
	y0 = win->BorderTop + 2;
	visible = (win->Height - win->BorderTop - win->BorderBottom - 4) / fh;
}

/* The columns compared */
#define COLS	16

static struct BitMap *refbm;
static struct RastPort ref;

/* Whether row shows columns col.. of the file's line n (0: none) */
static BOOL row(WORD r, LONG n, WORD col, BOOL inverted)
{
	char exp[COLS];
	char *s;
	WORD i, c, x, y, len;

	for (i = 0; i < COLS; i++)
		exp[i] = ' ';
	if (n > 0) {
		s = text(n);
		for (c = 0; *s; s++) {
			if (*s == '\t') {
				do
					c++;
				while (c % 8);
				continue;
			}
			if (c >= col && c < col + COLS)
				exp[c - col] = *s;
			c++;
		}
	}
	for (len = COLS; len > 0 && exp[len - 1] == ' '; len--)
		;
	SetAPen(&ref, dri->dri_Pens[BACKGROUNDPEN]);
	RectFill(&ref, 0, 0, COLS * fw - 1, fh - 1);
	SetAPen(&ref, dri->dri_Pens[TEXTPEN]);
	SetBPen(&ref, dri->dri_Pens[inverted ? SHINEPEN : BACKGROUNDPEN]);
	SetDrMd(&ref, JAM2);
	Move(&ref, 0, ref.TxBaseline);
	if (len > 0)
		Text(&ref, exp, len);
	for (y = 0; y < fh; y++)
		for (x = 0; x < COLS * fw; x++)
			if (ReadPixel(&ref, x, y) !=
			    ReadPixel(win->RPort, x0 + x, y0 + r * fh + y)) {
				xKPrintf("more: row %d, line %ld: pixel %d,%d\n",
				    r, n, x, y);
				return FALSE;
			}
	return TRUE;
}

/* Rows 0, 1 and the last show the file from line top */
static BOOL view(LONG top, WORD col)
{
	LONG last = top + visible - 1;

	return row(0, top, col, FALSE) && row(1, top + 1, col, FALSE) &&
	    row(visible - 1, last <= nlines ? last : 0, col, FALSE);
}

static void stackuse(struct Task *t)
{
	UBYTE *p, *top;

	p = (UBYTE *)t->tc_SPLower + 2;
	top = (UBYTE *)t->tc_SPUpper;
	while (p < top && *p == 0)
		p++;
	xKPrintf("more: %s stack %d of %d bytes used\n", t->tc_Node.ln_Name,
	    (int)(top - p), (int)(top - (UBYTE *)t->tc_SPLower));
}

/* Whether More's title holds s */
static BOOL titled(char *s)
{
	char *t, *a, *b;

	for (t = win->Title; *t; t++) {
		for (a = t, b = s; *b && *a == *b; a++, b++)
			;
		if (*b == 0)
			return TRUE;
	}
	return FALSE;
}

/* A custom gadget's centre on the screen, by ID */
static BOOL gadget(UWORD id, WORD *x, WORD *y)
{
	struct Gadget *g;
	WORD gx, gy;

	for (g = win->FirstGadget; g; g = g->NextGadget) {
		if ((g->GadgetType & GTYP_GTYPEMASK) != GTYP_CUSTOMGADGET ||
		    g->GadgetID != id)
			continue;
		gx = g->LeftEdge + ((g->Flags & GFLG_RELRIGHT) ? win->Width - 1 : 0);
		gy = g->TopEdge + ((g->Flags & GFLG_RELBOTTOM) ? win->Height - 1 : 0);
		*x = win->LeftEdge + gx + g->Width / 2;
		*y = win->TopEdge + gy + g->Height / 2;
		return TRUE;
	}
	return FALSE;
}

/* A window over More's first rows */
static struct Window *cover(void)
{
	struct TagItem t[7];
	struct Window *w;

	t[0].ti_Tag = WA_Left;		t[0].ti_Data = win->LeftEdge + 20;
	t[1].ti_Tag = WA_Top;		t[1].ti_Data = win->TopEdge + y0 + fh;
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = 100;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = 5 * fh;
	t[4].ti_Tag = WA_PubScreen;	t[4].ti_Data = (IPTR)wb;
	t[5].ti_Tag = WA_Borderless;	t[5].ti_Data = TRUE;
	t[6].ti_Tag = TAG_DONE;
	w = OpenWindowTagList(NULL, t);
	if (w != NULL) {
		SetRast(w->RPort, 1);
		ActivateWindow(win);
	}
	return w;
}

#define AMIGA	IEQUALIFIER_RCOMMAND

static void jump(char *n)
{
	key(0x26, AMIGA);
	waitreq("Jump To Line");
	keywait(0x32, IEQUALIFIER_CONTROL);	/* Ctrl-X clears */
	type(n);
	key(0x44, 0);
	idle();
	key(0x45, 0);				/* the requester's Esc */
	idle();
}

static void workbench(void)
{
	struct TagItem t[3];
	BPTR sys;
	WORD x, y;
	LONG top;

	sys = Lock("SYS:", SHARED_LOCK);
	t[0].ti_Tag = WBOPENA_ArgLock;	t[0].ti_Data = (IPTR)sys;
	t[1].ti_Tag = WBOPENA_ArgName;	t[1].ti_Data = (IPTR)"MoreTest";
	t[2].ti_Tag = TAG_DONE;
	check(OpenWorkbenchObjectA("SYS:Utilities/More", t), "launch");
	UnLock(sys);
	win = waitwin(TRUE);
	check(win != NULL, "More's window");
	if (win == NULL)
		return;
	more = win->UserPort->mp_SigTask;
	idle();
	layout();
	check(visible > 8, "rows");

	check(view(1, 0), "first page");
	check(row(TABLINE - 1, TABLINE, 0, FALSE), "tab stop");
	key(0x4D, 0);				/* cursor down */
	idle();
	check(view(2, 0), "line down");
	key(0x40, 0);				/* space */
	idle();
	top = 2 + visible - 1;
	check(view(top, 0), "page down");
	key(0x41, 0);				/* backspace */
	idle();
	check(view(2, 0), "page up");
	key(0x4C, 0);				/* cursor up */
	idle();
	check(view(1, 0), "line up");
	type("b");
	idle();
	check(view(NLINES - visible + 1, 0), "bottom");
	type("t");
	idle();
	check(view(1, 0), "top");

	key(0x4E, 0);				/* cursor right */
	idle();
	check(row(0, 1, 1, FALSE) && row(1, 2, 1, FALSE), "column right");
	key(0x4F, 0);
	idle();
	check(row(0, 1, 0, FALSE), "column left");

	check(gadget(1, &x, &y), "down arrow");
	button(IECODE_NOBUTTON, x, y);
	button(IECODE_LBUTTON, x, y);
	button(IECODE_LBUTTON | IECODE_UP_PREFIX, x, y);
	idle();
	check(view(2, 0), "down arrow scrolls");

	/* Find: the line in the middle, inverted */
	type("f");
	waitreq("Find Text");
	type("needle");
	key(0x44, 0);
	Delay(10);
	idle();
	top = FINDLINE - visible / 2;
	check(row(0, top, 0, FALSE) &&
	    row(FINDLINE - top, FINDLINE, 0, TRUE), "find");
	key(0x45, 0);				/* the requester's Esc */
	idle();
	type("n");				/* no other match */
	idle();
	check(row(0, top, 0, FALSE) &&
	    row(FINDLINE - top, FINDLINE, 0, TRUE), "find next");

	/* Jump, from the menu's shortcut; past the end is the last page */
	idle();
	jump("400");
	check(view(NLINES - visible + 1, 0), "jump past the end");
	jump("40");
	check(view(40, 0), "jump");

	/* Covered and uncovered; scrolled while covered */
	{
		struct Window *c = cover();

		check(c != NULL, "cover");
		Delay(5);
		CloseWindow(c);
		idle();
		check(view(40, 0), "redraw");
		c = cover();
		idle();
		key(0x4D, 0);
		idle();
		CloseWindow(c);
		idle();
		check(view(41, 0), "scrolled under a window");
	}

	/* Open another file */
	key(0x18, AMIGA);			/* Amiga-O */
	waitreq("Open File");
	keywait(0x32, IEQUALIFIER_CONTROL);	/* Ctrl-X clears */
	type("MoreTest2");
	key(0x44, 0);
	Delay(10);
	idle();
	nlines = 5;
	check(view(1, 0) && row(5, 0, 0, FALSE), "open");
	check(titled("MoreTest2 (5 lines"), "title");
	nlines = NLINES;

	stackuse(more);
	key(0x45, 0);
	check(waitwin(FALSE) == NULL, "Esc quits");
}

struct sync {
	struct Task *s_Task;
	LONG s_RC;
};

static struct sync s;

/* A Shell's commands get 1600 bytes of stack unless Stack sets more */
static LONG shell(char *args, LONG len)
{
	BPTR in;
	LONG rc;

	if ((in = Open("SYS:MoreScript", MODE_OLDFILE)) == NULL)
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

	s.s_Task = FindTask(NULL);
	s.s_RC = -1;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (IPTR)shell;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (IPTR)"MoreShell";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 4096;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (IPTR)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (IPTR)&s;
	t[5].ti_Tag = TAG_DONE;
	check(CreateNewProc(t) != NULL, "Shell process");
	win = waitwin(TRUE);
	check(win != NULL, "More from the Shell");
	if (win == NULL)
		return;
	more = win->UserPort->mp_SigTask;
	idle();
	layout();
	check(view(1, 0), "Shell: first page");
	key(0x40, 0);
	idle();
	check(view(visible, 0), "Shell: page down");
	stackuse(more);
	key(0x45, 0);
	Wait(SIGBREAKF_CTRL_F);
	check(s.s_RC == 0, "Shell: return code");
	check(morewin() == NULL, "Shell: window closed");
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
		KPutStr("FAIL more: OpenLibrary\n");
		return 1;
	}
	check(writefile("SYS:MoreTest", NLINES) && writefile("SYS:MoreTest2", 5) &&
	    script(), "test files");
	check(OpenWorkBench() != 0, "OpenWorkBench");
	wb = LockPubScreen("Workbench");
	dri = GetScreenDrawInfo(wb);
	refbm = AllocBitMap(COLS * 8, 16, GetBitMapAttr(wb->RastPort.BitMap,
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
	KPutStr("PASS more\n");
	return 0;
}
