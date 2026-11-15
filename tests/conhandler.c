/*
 * CON: and RAW: through dos.library, on the Workbench screen in a RAM
 * bitmap.  A process opens windows by name, writes to them, types keys
 * through input.device and reads them back cooked and raw, opens "*" on
 * a window's handler, and closes the windows with Close() and with their
 * close gadget.
 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/alib.h>

struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL conhandler: %s\n", what);
		fails++;
	}
}

static struct MsgPort *port;
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

static void sync(void)
{
	event(IECLASS_NULL, 0, 0, 0, 0);
	TimeDelay(UNIT_VBLANK, 0, 40000);
}

/* Raw key codes of the characters the test types */
static UWORD rawcode(char c)
{
	static char row1[] = "qwertyuiop", row2[] = "asdfghjkl", row3[] = "zxcvbnm";
	int i;

	if (c == '\n')
		return RAWKEY_RETURN;
	if (c == ' ')
		return RAWKEY_SPACE;
	if (c == '\b')
		return RAWKEY_BACKSPACE;
	for (i = 0; row1[i]; i++)
		if (row1[i] == c)
			return 0x10 + i;
	for (i = 0; row2[i]; i++)
		if (row2[i] == c)
			return 0x20 + i;
	for (i = 0; row3[i]; i++)
		if (row3[i] == c)
			return 0x31 + i;
	return 0x01;	/* '1' */
}

static void type(char *s)
{
	for (; *s; s++) {
		event(IECLASS_RAWKEY, rawcode(*s), 0, 0, 0);
		event(IECLASS_RAWKEY, rawcode(*s) | IECODE_UP_PREFIX, 0, 0, 0);
	}
	sync();
}

static int streq(char *a, char *b, LONG n)
{
	for (; n > 0; n--)
		if (*a++ != *b++)
			return 0;
	return *b == 0;
}

/* The window a handle's handler draws in: ACTION_DISK_INFO's volume */
static struct Window *window(BPTR fh)
{
	struct InfoData id;
	struct FileHandle *f = (struct FileHandle *)fh;

	if (DoPkt(f->fh_Type, ACTION_DISK_INFO, (LONG)&id, 0, 0, 0, 0)
	    == DOSFALSE)
		return NULL;
	return (struct Window *)id.id_VolumeNode;
}

static int nwindows(void)
{
	struct Screen *s = IntuitionBase->FirstScreen;
	struct Window *w;
	int n = 0;

	if (s)
		for (w = s->FirstWindow; w; w = w->NextWindow)
			n++;
	return n;
}

/* The stacks of the handler's processes: the pool clears a new stack */
static void stackuse(void)
{
	struct Task *t;
	UBYTE *p, *top;

	Forbid();
	for (t = (struct Task *)SysBase->TaskWait.lh_Head;
	    t->tc_Node.ln_Succ; t = (struct Task *)t->tc_Node.ln_Succ) {
		if (!streq(t->tc_Node.ln_Name, "CON", 3) &&
		    !streq(t->tc_Node.ln_Name, "RAW", 3))
			continue;
		p = (UBYTE *)t->tc_SPLower + 2;
		top = (UBYTE *)t->tc_SPUpper;
		while (p < top && *p == 0)
			p++;
		xKPrintf("conhandler: %s stack %ld of %ld bytes used\n",
		    t->tc_Node.ln_Name, (LONG)(top - p),
		    (LONG)(top - (UBYTE *)t->tc_SPLower));
	}
	Permit();
}

static void tests(void)
{
	BPTR con, raw, star;
	struct Window *w;
	char buf[80];
	LONG n;

	/* a window from its specification */
	con = Open("CON:10/20/400/100/Test Window/CLOSE", MODE_NEWFILE);
	check(con != NULL, "Open CON:");
	if (con == NULL)
		return;
	check(IsInteractive(con), "IsInteractive");
	w = window(con);
	check(w != NULL && w->LeftEdge == 10 && w->TopEdge == 20 &&
	    w->Width == 400 && w->Height == 100, "window size");
	check(w != NULL && streq(w->Title, "Test Window", 11), "window title");
	check(w != NULL && (w->Flags & WFLG_CLOSEGADGET), "CLOSE");
	check(nwindows() == 1, "one window");
	check(Write(con, "Hello\n", 6) == 6, "Write");

	/* cooked input: a line, edited */
	sync();
	type("dirx\b\n");
	n = Read(con, buf, sizeof(buf));
	check(n == 4 && streq(buf, "dir\n", 4), "cooked Read");
	check(WaitForChar(con, 0) == DOSFALSE, "WaitForChar empty");

	/* raw mode on the same window */
	check(SetMode(con, 1), "SetMode raw");
	type("ab");
	check(WaitForChar(con, 100000), "WaitForChar");
	n = Read(con, buf, sizeof(buf));
	check(n == 2 && streq(buf, "ab", 2), "raw Read");
	SetMode(con, 0);

	/* "*" is the process's first window */
	star = Open("*", MODE_OLDFILE);
	check(star != NULL && window(star) == w, "Open *");
	if (star != NULL)
		Close(star);
	check(nwindows() == 1, "* opens no window");

	/* RAW: gives each key at once */
	raw = Open("RAW:0/0/300/80/Raw", MODE_OLDFILE);
	check(raw != NULL && nwindows() == 2, "Open RAW:");
	if (raw != NULL) {
		sync();
		type("q");
		n = Read(raw, buf, sizeof(buf));
		check(n == 1 && buf[0] == 'q', "RAW: Read");
		stackuse();
		Close(raw);
		check(nwindows() == 1, "Close RAW:");
	}

	/* the close gadget ends the input */
	ActivateWindow(w);
	sync();
	n = 0;
	{
		struct Gadget *g;

		for (g = w->FirstGadget; g; g = g->NextGadget)
			if ((g->GadgetType & GTYP_SYSTYPEMASK) == GTYP_CLOSE)
				break;
		if (g != NULL) {
			WORD x = w->LeftEdge + g->LeftEdge + g->Width / 2;
			WORD y = w->TopEdge + g->TopEdge + g->Height / 2;

			event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, 0, x, y);
			event(IECLASS_RAWMOUSE, IECODE_LBUTTON, 0, x, y);
			event(IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX,
			    0, x, y);
			sync();
			n = Read(con, buf, sizeof(buf));
		}
		check(g != NULL && n == 0, "close gadget: end of input");
	}
	Close(con);
	check(nwindows() == 0, "Close CON:");

	/* a bad specification */
	con = Open("CON:10/x/100/100/Bad", MODE_NEWFILE);
	check(con == NULL && IoErr() == ERROR_BAD_STREAM_NAME, "bad name");
}

static LONG proc(char *args, LONG len)
{
	tests();
	return 0;
}

struct sync {
	struct Task *s_Task;
};

static void done(LONG rc, struct sync *s)
{
	Signal(s->s_Task, SIGBREAKF_CTRL_F);
}

int main(void)
{
	struct TagItem t[6];
	struct sync s;

	port = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
	if (OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL conhandler: input.device\n");
		return 1;
	}
	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	if (DOSBase == NULL || IntuitionBase == NULL) {
		KPutStr("FAIL conhandler: OpenLibrary\n");
		return 1;
	}
	s.s_Task = FindTask(NULL);
	SetSignal(0L, SIGBREAKF_CTRL_F);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)proc;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)"conhandler";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 3072;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (ULONG)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (ULONG)&s;
	t[5].ti_Tag = TAG_DONE;
	if (CreateNewProc(t) == NULL) {
		KPutStr("FAIL conhandler: CreateNewProc\n");
		return 1;
	}
	Wait(SIGBREAKF_CTRL_F);
	CloseWorkBench();
	if (fails != 0)
		return 1;
	KPutStr("PASS conhandler\n");
	return 0;
}
