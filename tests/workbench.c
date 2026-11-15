/* SPDX-License-Identifier: MIT */
/*
 * workbench.library: a registered Workbench port gets the handler's
 * messages for AppIcons, UpdateWorkbench() and drawers to open; an
 * AppWindow gets its AppMessage; and OpenWorkbenchObjectA() launches
 * the Hello tool with a WBStartup message, which the tool replies when
 * it ends.
 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <workbench/workbench.h>
#include <workbench/startup.h>
#include <workbench/handler.h>
#include <workbench/icon.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/icon.h>
#include <proto/workbench.h>

struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Library *IconBase;
struct Library *WorkbenchBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL workbench: %s\n", what);
		fails++;
	}
}

/* Whether s ends with e */
static int ends(char *s, char *e)
{
	char *p = s, *q = e;

	while (*p)
		p++;
	while (*q)
		q++;
	while (q > e && p > s && *--p == *--q)
		;
	return q == e && *p == *q;
}

static struct MsgPort *port;

/* The next handler message, or NULL after about 2 s */
static struct WBHandlerMessage *next(void)
{
	struct WBHandlerMessage *m;
	int i;

	for (i = 0; i < 100; i++) {
		if ((m = (struct WBHandlerMessage *)GetMsg(port)) != NULL)
			return m;
		Delay(1);
	}
	return NULL;
}

/* A handler message of the type, replied; with its name ending in name */
static void expect(enum WBHM_Type type, LONG utype, char *name, char *what)
{
	struct WBHandlerMessage *m;

	/* OpenWorkBench()'s message comes when the handler gets to it */
	while ((m = next()) != NULL && m->wbhm_Type == WBHM_TYPE_SHOW)
		ReplyMsg((struct Message *)m);
	check(m != NULL && m->wbhm_Type == type, what);
	if (m == NULL)
		return;
	if (type == WBHM_TYPE_OPEN)
		check(ends((char *)m->wbhm_Data.Open.Name, name), what);
	if (type == WBHM_TYPE_UPDATE)
		check(m->wbhm_Data.Update.Type == utype && (name == NULL ?
		    m->wbhm_Data.Update.Name == NULL :
		    ends((char *)m->wbhm_Data.Update.Name, name)), what);
	ReplyMsg((struct Message *)m);
}

static void appicons(void)
{
	struct DiskObject *icon = GetDefDiskObject(WBTOOL);
	struct AppIcon *ai;
	char text[32];

	ai = AddAppIconA(1, 2, "AppIcon", port, NULL, icon, NULL);
	check(ai != NULL, "AddAppIconA");
	expect(WBHM_TYPE_UPDATE, WBAPPICON, NULL, "AppIcon added");
	check(GetNextAppIcon(NULL, text) == icon && ends(text, "AppIcon") &&
	    GetNextAppIcon(icon, text) == NULL, "GetNextAppIcon");
	check(RemoveAppIcon(ai), "RemoveAppIcon");
	expect(WBHM_TYPE_UPDATE, WBAPPICON, NULL, "AppIcon removed");
	check(GetNextAppIcon(NULL, text) == NULL, "no AppIcon");
	FreeDiskObject(icon);
}

static void appmenus(void)
{
	struct AppMenuItem *m1, *m2;
	struct TagItem t[2];

	t[0].ti_Tag = WBAPPMENUA_CommandKeyString;	t[0].ti_Data = (IPTR)"K";
	t[1].ti_Tag = TAG_DONE;
	m1 = AddAppMenuItemA(1, 0, "One", port, t);
	m2 = AddAppMenuItemA(2, 0, "Two", port, NULL);
	check(m1 != NULL && m2 != NULL, "AddAppMenuItemA");
	check(RemoveAppMenuItem(m2) && RemoveAppMenuItem(m1),
	    "RemoveAppMenuItem");
}

/* The AppWindow's port: a process that checks one AppMessage */
static struct Task *parent;
static int appmsgok;

static LONG helper(char *args, LONG len)
{
	struct Process *me = (struct Process *)FindTask(NULL);
	struct AppMessage *am;
	char path[64];

	WaitPort(&me->pr_MsgPort);
	am = (struct AppMessage *)GetMsg(&me->pr_MsgPort);
	appmsgok = am->am_Type == AMTYPE_APPWINDOW && am->am_ID == 7 &&
	    am->am_UserData == 8 && am->am_NumArgs == 1 &&
	    am->am_MouseX == 10 && am->am_MouseY == 20 &&
	    ends((char *)am->am_ArgList[0].wa_Name, "Hello") &&
	    NameFromLock(am->am_ArgList[0].wa_Lock, path, sizeof(path)) &&
	    ends(path, "Demos");
	ReplyMsg((struct Message *)am);
	Signal(parent, SIGBREAKF_CTRL_E);
	return 0;
}

static void appwindows(void)
{
	static char *files[] = { "SYS:Demos/Hello" };
	struct Process *p;
	struct Window *w;
	struct AppWindow *aw;
	struct TagItem t[6];

	t[0].ti_Tag = WA_Width;		t[0].ti_Data = 200;
	t[1].ti_Tag = WA_Height;	t[1].ti_Data = 80;
	t[2].ti_Tag = WA_Title;		t[2].ti_Data = (IPTR)"AppWindow";
	t[3].ti_Tag = TAG_DONE;
	w = OpenWindowTagList(NULL, t);
	check(w != NULL, "OpenWindowTagList");
	if (w == NULL)
		return;
	parent = FindTask(NULL);
	SetSignal(0L, SIGBREAKF_CTRL_E);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (IPTR)helper;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (IPTR)"WBHelper";
	t[2].ti_Tag = TAG_DONE;
	p = CreateNewProc(t);
	check(p != NULL, "helper");
	if (p != NULL) {
		aw = AddAppWindowA(7, 8, w, &p->pr_MsgPort, NULL);
		check(aw != NULL, "AddAppWindowA");
		check(SendAppWindowMessage(w, 1, files, 0, 10, 20, 0, 0),
		    "SendAppWindowMessage");
		Wait(SIGBREAKF_CTRL_E);
		check(appmsgok, "AppMessage");
		check(RemoveAppWindow(aw), "RemoveAppWindow");
	}
	CloseWindow(w);
}

/* The task called name, or NULL after about 4 s */
static struct Task *task(char *name, BOOL gone)
{
	struct Task *t = NULL;
	int i;

	for (i = 0; i < 200; i++) {
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

	t = FindTask(name);
	if (t == NULL)
		return;
	p = (UBYTE *)t->tc_SPLower + 2;
	top = (UBYTE *)t->tc_SPUpper;
	while (p < top && *p == 0)
		p++;
	xKPrintf("workbench: %s stack %d of %d bytes used\n", name,
	    (int)(top - p), (int)(top - (UBYTE *)t->tc_SPLower));
}

static void objects(void)
{
	struct Task *t;
	struct TagItem tags[3];
	BPTR lock;
	ULONG stack = 0;

	lock = Lock("SYS:Demos", ACCESS_READ);
	check(lock != NULL && UpdateWorkbench("Hello", lock,
	    UPDATEWB_ObjectAdded), "UpdateWorkbench");
	expect(WBHM_TYPE_UPDATE, 0, "Demos/Hello", "update message");

	check(OpenWorkbenchObjectA("SYS:Demos", NULL), "open drawer");
	expect(WBHM_TYPE_OPEN, 0, "SYS:Demos", "open message");

	/* The tool and one more argument */
	tags[0].ti_Tag = WBOPENA_ArgLock;	tags[0].ti_Data = (IPTR)lock;
	tags[1].ti_Tag = WBOPENA_ArgName;	tags[1].ti_Data = (IPTR)"Hello.info";
	tags[2].ti_Tag = TAG_DONE;
	check(OpenWorkbenchObjectA("SYS:Demos/Hello", tags), "launch Hello");
	t = task("Hello", FALSE);
	check(t != NULL, "Hello runs");
	if (t != NULL) {
		Delay(25);
		stackuse("Hello");
		Signal(t, SIGBREAKF_CTRL_C);
		check(task("Hello", TRUE) == NULL, "Hello ends");
	}
	UnLock(lock);
	stackuse("Workbench Handler");

	tags[0].ti_Tag = WBCTRLA_GetDefaultStackSize;	tags[0].ti_Data = (IPTR)&stack;
	tags[1].ti_Tag = TAG_DONE;
	check(WorkbenchControlA(NULL, tags) && stack == 2048,
	    "WorkbenchControlA");
}

static LONG run(char *args, LONG len)
{
	struct MsgPort *other;

	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	IconBase = OpenLibrary(ICONNAME, 44L);
	WorkbenchBase = OpenLibrary(WORKBENCH_NAME, 45L);
	if (GfxBase == NULL || IntuitionBase == NULL || IconBase == NULL ||
	    WorkbenchBase == NULL) {
		KPutStr("FAIL workbench: OpenLibrary\n");
		return RETURN_FAIL;
	}
	KPutStr("workbench: libraries open\n");
	check(OpenWorkBench() != 0, "OpenWorkBench");
	port = CreateMsgPort();
	other = CreateMsgPort();
	check(RegisterWorkbench(port) && !RegisterWorkbench(other),
	    "RegisterWorkbench");
	appicons();
	appmenus();
	appwindows();
	objects();
	check(!UnregisterWorkbench(other) && UnregisterWorkbench(port),
	    "UnregisterWorkbench");
	DeleteMsgPort(other);
	DeleteMsgPort(port);
	return fails ? RETURN_FAIL : RETURN_OK;
}

struct sync {
	struct Task *s_Task;
	LONG s_RC;
};

static void done(LONG rc, struct sync *s)
{
	s->s_RC = rc;
	Signal(s->s_Task, SIGBREAKF_CTRL_F);
}

int main(void)
{
	struct TagItem t[6];
	struct sync s;

	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	if (DOSBase == NULL) {
		KPutStr("FAIL workbench: no dos.library\n");
		return 1;
	}
	s.s_Task = FindTask(NULL);
	s.s_RC = -1;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)run;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)"WBTest";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 4096;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (ULONG)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (ULONG)&s;
	t[5].ti_Tag = TAG_DONE;
	if (CreateNewProc(t) == NULL) {
		KPutStr("FAIL workbench: CreateNewProc\n");
		return 1;
	}
	Wait(SIGBREAKF_CTRL_F);
	if (s.s_RC != RETURN_OK)
		return 1;
	KPutStr("PASS workbench\n");
	return 0;
}
