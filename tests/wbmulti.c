/* SPDX-License-Identifier: MIT */
/*
 * The stack pool under load: Workbench, a Shell in a CON: window and three
 * tools launched from Workbench run at once.  Each must start, and the
 * pool must keep room for a Shell command's 4,000 bytes.  Workbench runs
 * as wbboot starts it; this test's selftest() drives it instead of the
 * usual self test.
 */
#include <exec/exec.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <intuition/intuitionbase.h>
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/workbench.h>
#include <proto/alib.h>

extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;
extern struct Library *WorkbenchBase;

LONG Workbench(char *args, LONG len);
extern BOOL (*wbselftest)(void);
extern LONG (*wbselftestdone)(void);

#define CMDSTACK 4000

static char *tools[] = { "SYS:Utilities/Clock", "SYS:Utilities/SysMon",
	"SYS:Demos/Notes" };
static char *names[] = { "Clock", "SysMon", "More" };
#define NTOOLS 3

struct sync {
	struct Task *s_Task;
	ULONG s_Sig;
	LONG s_RC;
};

static struct sync wb, sh, drv;
static struct Task *wbtask;
static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL wbmulti: %s\n", what);
		fails++;
	}
}

static void done(LONG rc, struct sync *s)
{
	s->s_RC = rc;
	Signal(s->s_Task, s->s_Sig);
}

static BOOL start(LONG (*entry)(), char *name, ULONG stack, struct sync *s,
    ULONG sig)
{
	struct TagItem t[6];

	s->s_Task = FindTask(NULL);
	s->s_Sig = sig;
	s->s_RC = -1;
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)entry;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)name;
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = stack;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (ULONG)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (ULONG)s;
	t[5].ti_Tag = TAG_DONE;
	return CreateNewProc(t) != NULL;
}

/* The task called name, or NULL after about 8 s */
static struct Task *task(char *name, BOOL gone)
{
	struct Task *t = NULL;
	int i;

	for (i = 0; i < 400; i++) {
		Forbid();
		t = FindTask(name);
		Permit();
		if ((t == NULL) == gone)
			break;
		Delay(1);
	}
	return t;
}

static void stackuse(char *name)
{
	struct Task *t;
	UBYTE *p, *top;

	Forbid();
	if ((t = FindTask(name)) != NULL) {
		p = (UBYTE *)t->tc_SPLower + 2;
		top = (UBYTE *)t->tc_SPUpper;
		while (p < top && *p == 0)
			p++;
		xKPrintf("wbmulti: %s stack %ld of %ld bytes used\n", name,
		    (LONG)(top - p), (LONG)(top - (UBYTE *)t->tc_SPLower));
	}
	Permit();
}

static LONG shell(char *args, LONG len)
{
	BPTR in, out;

	if ((in = Open("CON:0/120/400/80/Shell/CLOSE", MODE_OLDFILE)) == NULL)
		return RETURN_FAIL;
	out = Open("*", MODE_NEWFILE);
	if (out != NULL) {
		Execute("", in, out);
		Close(out);
	}
	Close(in);
	return out != NULL ? RETURN_OK : RETURN_FAIL;
}

static struct IOStdReq *io;

static void key(UWORD code)
{
	struct InputEvent ev;

	ev.ie_NextEvent = NULL;
	ev.ie_Class = IECLASS_RAWKEY;
	ev.ie_SubClass = 0;
	ev.ie_Code = code;
	ev.ie_Qualifier = 0;
	ev.ie_X = ev.ie_Y = 0;
	io->io_Command = IND_WRITEEVENT;
	io->io_Data = &ev;
	io->io_Length = sizeof(ev);
	DoIO((struct IORequest *)io);
}

/* EndShell, typed into the active window */
static void endshell(void)
{
	static UWORD codes[] = { 0x12, 0x36, 0x22, 0x21, 0x25, 0x12, 0x28,
		0x28, RAWKEY_RETURN };
	int i;

	for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
		key(codes[i]);
		key(codes[i] | IECODE_UP_PREFIX);
	}
}

static struct Window *shellwindow(void)
{
	struct Window *w;

	for (w = IntuitionBase->FirstScreen->FirstWindow; w; w = w->NextWindow)
		if (w->Title && w->Title[0] == 'S' && w->Title[1] == 'h')
			return w;
	return NULL;
}

static LONG driver(char *args, LONG len)
{
	struct MsgPort *port = CreateMsgPort();
	struct Task *t;
	struct Window *w;
	ULONG free, largest;
	int i;

	io = (struct IOStdReq *)CreateIORequest(port, sizeof(*io));
	if (io == NULL ||
	    OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		check(0, "input.device");
		Signal(wbtask, SIGBREAKF_CTRL_C);
		return 1;
	}
	check(start((LONG (*)())shell, "Shell", 2048, &sh, SIGBREAKF_CTRL_E),
	    "Shell");
	check(task("Shell Process", FALSE) != NULL, "Shell started");
	for (i = 0; i < NTOOLS; i++) {
		check(OpenWorkbenchObjectA(tools[i], NULL), tools[i]);
		check(task(names[i], FALSE) != NULL, names[i]);
	}
	/* let them draw */
	Delay(100);
	stackuse("Workbench");
	stackuse("Workbench Handler");
	stackuse("input.device");
	stackuse("Shell Process");
	stackuse("CON");
	stackuse("console.device");
	for (i = 0; i < NTOOLS; i++)
		stackuse(names[i]);
	free = AvailMem(MEMF_STACK);
	largest = AvailMem(MEMF_STACK | MEMF_LARGEST);
	xKPrintf("wbmulti: stack pool %ld bytes free, %ld largest; memory %ld free\n",
	    free, largest, AvailMem(MEMF_ANY));
	check(largest >= CMDSTACK, "room for a Shell command");

	for (i = 0; i < NTOOLS; i++)
		if ((t = task(names[i], FALSE)) != NULL)
			Signal(t, SIGBREAKF_CTRL_C);
	for (i = 0; i < NTOOLS; i++)
		check(task(names[i], TRUE) == NULL, "a tool quit");
	if ((w = shellwindow()) != NULL) {
		ActivateWindow(w);
		Delay(10);
		endshell();
	}
	check(task("Shell Process", TRUE) == NULL, "EndShell");
	CloseDevice((struct IORequest *)io);
	DeleteIORequest((struct IORequest *)io);
	DeleteMsgPort(port);
	/* Workbench reads it as soon as it quits */
	drv.s_RC = fails ? 1 : 0;
	Signal(wbtask, SIGBREAKF_CTRL_C);
	return drv.s_RC;
}

/* Workbench's hooks for its self test */
BOOL selftest(void)
{
	wbtask = FindTask(NULL);
	return start((LONG (*)())driver, "wbmulti", 2048, &drv,
	    SIGBREAKF_CTRL_D);
}

LONG selftestdone(void)
{
	return drv.s_RC;
}

int main(void)
{
	struct DosLibrary *dos;
	struct IntuitionBase *ib;

	wbselftest = selftest;
	wbselftestdone = selftestdone;
	if ((dos = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L)) == NULL ||
	    (ib = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L))
	    == NULL) {
		KPutStr("FAIL wbmulti: OpenLibrary\n");
		return 1;
	}
	DOSBase = dos;
	IntuitionBase = ib;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	if (!start((LONG (*)())Workbench, "Workbench", 3072, &wb,
	    SIGBREAKF_CTRL_F)) {
		KPutStr("FAIL wbmulti: Workbench\n");
		return 1;
	}
	Wait(SIGBREAKF_CTRL_F);
	if (wb.s_RC != RETURN_OK)
		return 1;
	KPutStr("PASS wbmulti\n");
	return 0;
}
