/* SPDX-License-Identifier: MIT */
/*
 * Workbench for screenshots, each step held 10 s: drawers, then the
 * utilities and a Shell, then ScreenMode once the utilities have quit.
 * Prints "showcase: STEP ..." as each starts.
 */
/* EMUFLAGS: --hr --ram=08-1F */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/dos.h>
#include <proto/workbench.h>
#include <proto/intuition.h>

extern struct DosLibrary *DOSBase;
extern struct Library *WorkbenchBase;
extern struct IntuitionBase *IntuitionBase;

LONG Workbench(char *args, LONG len);

#define HOLD (10 * TICKS_PER_SECOND)

static char *drawers[] = { "SYS:Utilities", "SYS:Tools/Commodities",
	"SYS:Demos" };
/* the larger windows first, so the smaller open in front of them */
static char *tools[] = { "SYS:Demos/ReadMe", "SYS:Utilities/KeyShow",
	"SYS:Utilities/SysMon", "SYS:Utilities/Calculator", "SYS:Utilities/Clock" };
static char *names[] = { "More", "KeyShow", "SysMon", "Calculator", "Clock" };
#define NDRAWERS (sizeof(drawers) / sizeof(drawers[0]))
#define NTOOLS (sizeof(tools) / sizeof(tools[0]))

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL showcase: %s\n", what);
		fails++;
	}
}

static BOOL proc(LONG (*entry)(), char *name, LONG stack)
{
	struct TagItem t[4];

	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)entry;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)name;
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = stack;
	t[3].ti_Tag = TAG_DONE;
	return CreateNewProc(t) != NULL;
}

static LONG shell(char *args, LONG len)
{
	BPTR in, out;

	if ((in = Open("CON:20/150/400/80/Shell/CLOSE", MODE_OLDFILE)) == NULL)
		return RETURN_FAIL;
	if ((out = Open("*", MODE_NEWFILE)) != NULL) {
		Execute("", in, out);
		Close(out);
	}
	Close(in);
	return RETURN_OK;
}

/* The task called name, or NULL once it has gone; waits about 8 s */
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

/* The windows open; LockPubScreen() would open Workbench's screen
 * before Workbench reads its saved mode */
static int windows(void)
{
	struct Screen *s;
	struct Window *w;
	ULONG lock = LockIBase(0);
	int n = 0;

	for (s = IntuitionBase->FirstScreen; s != NULL; s = s->NextScreen)
		for (w = s->FirstWindow; w != NULL; w = w->NextWindow)
			n++;
	UnlockIBase(lock);
	return n;
}

static void step(char *what)
{
	xKPrintf("showcase: STEP %s\n", what);
	Delay(HOLD);
}

int main(void)
{
	int i;

	if ((DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L)) == NULL ||
	    (WorkbenchBase = OpenLibrary(WORKBENCH_NAME, 45L)) == NULL ||
	    (IntuitionBase = (struct IntuitionBase *)
	    OpenLibrary(INTUITIONNAME, 39L)) == NULL) {
		KPutStr("FAIL showcase: OpenLibrary\n");
		return 1;
	}
	check(proc((LONG (*)())Workbench, "Workbench", 3072), "Workbench");
	for (i = 0; i < NDRAWERS; i++)
		check(OpenWorkbenchObjectA(drawers[i], NULL), drawers[i]);
	/* asked for before Workbench registered, so held until it did */
	for (i = 0; i < 400 && windows() < 1 + NDRAWERS; i++)
		Delay(1);
	check(i < 400, "the drawers' windows");
	step("drawers");
	for (i = 0; i < NTOOLS; i++) {
		check(OpenWorkbenchObjectA(tools[i], NULL), tools[i]);
		check(task(names[i], FALSE) != NULL, names[i]);
	}
	check(proc((LONG (*)())shell, "Shell", 2048), "Shell");
	step("tools");
	for (i = 0; i < NTOOLS; i++) {
		struct Task *t = task(names[i], FALSE);

		if (t != NULL)
			Signal(t, SIGBREAKF_CTRL_C);
		check(task(names[i], TRUE) == NULL, "a tool quit");
	}
	check(OpenWorkbenchObjectA("SYS:Prefs/ScreenMode", NULL), "ScreenMode");
	check(task("ScreenMode", FALSE) != NULL, "ScreenMode started");
	step("prefs");
	if (fails)
		return 1;
	KPutStr("PASS showcase\n");
	return 0;
}
