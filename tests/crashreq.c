/* SPDX-License-Identifier: MIT */
/*
 * The stack of a crash requester: a process with a 2 KB stack opens an
 * EasyRequest on the default public screen and waits in SysReqHandler,
 * as dos.library's does, until a click answers it.  Each screen mode,
 * look and screen font gets its peak; it must leave 256 bytes for
 * interrupts.
 */
/* EMUFLAGS: --ram=08-1F */
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <graphics/gfxbase.h>
#include <graphics/hrmodeid.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/preferences.h>
#include <intuition/screens.h>
#include <resources/framebuf.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/diskfont.h>
#include <proto/alib.h>
#ifdef LC
#include "lc/lctest.h"
#define NAME	"crashreqlc"
#else
#define NAME	"crashreq"
#endif

struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct DosLibrary *DOSBase;
struct Library *DiskfontBase;

#define STACK	2048L
#define ROOM	256

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL %s: %s\n", NAME, what);
		fails++;
	}
}

#ifndef LC
/* framebuf.resource of an HR card in mono, VRAM in two RAM pages */
static struct FBMode fbmode = {
	1024, 800, 1, 128, FBF_MONO, 0xFFFF, 0, { NULL, NULL }, 0x20000L
};
static struct FBInfo fbinfo = { FBC_HR, 1, &fbmode, 0 };
static char *owner;

static struct FBInfo *fb_getinfo(APTR base)
{
	return &fbinfo;
}

static char *fb_alloc(APTR base, char *name)
{
	if (owner)
		return owner;
	owner = name;
	return NULL;
}

static void fb_free(APTR base)
{
	owner = NULL;
}

static APTR fbfuncs[] = {
	(APTR)fb_getinfo, (APTR)fb_alloc, (APTR)fb_free, (APTR)-1
};

static BOOL fbsetup(void)
{
	struct Library *fb;

	fbmode.fbm_Half[0] = AllocMem(0x10000L, MEMF_CLEAR);
	fbmode.fbm_Half[1] = AllocMem(0x10000L, MEMF_CLEAR);
	if (fbmode.fbm_Half[0] == NULL || fbmode.fbm_Half[1] == NULL ||
	    ((ULONG)fbmode.fbm_Half[0] & 0xFFFF) != 0 ||
	    ((ULONG)fbmode.fbm_Half[1] & 0xFFFF) != 0)
		return FALSE;
	fb = MakeLibrary(fbfuncs, NULL, NULL, sizeof(struct Library), NULL);
	if (fb == NULL)
		return FALSE;
	fb->lib_Node.ln_Name = FRAMEBUFNAME;
	fb->lib_Node.ln_Type = NT_RESOURCE;
	AddResource(fb);
	return TRUE;
}
#endif

static struct EasyStruct es = {
	sizeof(struct EasyStruct), 0, "Software Failure",
	"Program failed (error #%08lx).\nWait for disk activity to finish.",
	"Suspend|Reboot"
};

static struct Task *maintask;
static volatile LONG answer;
static volatile UWORD peak;

#define PAINT	0xA5A5

/* The stack below the caller's frame */
static void paint(void)
{
	UWORD here, *p;

	for (p = (UWORD *)FindTask(NULL)->tc_SPLower + 1; p < &here - 16; p++)
		*p = PAINT;
}

/* The requester process: its stack painted, the requester, the peak */
static LONG crash(char *args, LONG len)
{
	struct Task *me = FindTask(NULL);
	struct Library *ib;
	struct Window *w;
	ULONG num = 0x80000004L;
	UWORD *p;
	LONG a = -1;

	paint();
	if ((ib = OpenLibrary(INTUITIONNAME, 0L)) != NULL) {
		w = BuildEasyRequestArgs(NULL, &es, NULL, (RAWARG)&num);
		if (w != NULL && w != (struct Window *)1L) {
			while ((a = SysReqHandler(w, NULL, TRUE)) < 0)
				;
			FreeSysRequest(w);
		}
		CloseLibrary(ib);
	}
	for (p = (UWORD *)me->tc_SPLower + 1; *p == PAINT; p++)
		;
	peak = (UWORD)((char *)me->tc_SPUpper - (char *)p);
	answer = a;
	Signal(maintask, SIGBREAKF_CTRL_F);
	return 0;
}

/* Clicks the requester's first button */
static void clicker(void)
{
	struct MsgPort *p = CreateMsgPort();
	struct IOStdReq *r = (struct IOStdReq *)CreateIORequest(p, sizeof(*r));
	struct InputEvent e;
	struct Window *w;
	struct Gadget *g;
	char *t, *s;
	int i, k;

	if (r == NULL || OpenDevice("input.device", 0, (struct IORequest *)r, 0))
		Wait(0);
	for (i = 0; i < 200; i++) {
		TimeDelay(UNIT_VBLANK, 0, 20000);
		w = IntuitionBase->ActiveWindow;
		if (w == NULL || (t = (char *)w->Title) == NULL)
			continue;
		for (s = es.es_Title; *t && *t == *s; t++, s++)
			;
		if (*t != *s)
			continue;
		for (g = w->FirstGadget; g; g = g->NextGadget)
			if (g->GadgetID == 1)
				break;
		if (g == NULL)
			continue;
		for (k = 0; k < 3; k++) {
			e.ie_NextEvent = NULL;
			e.ie_Class = IECLASS_RAWMOUSE;
			e.ie_SubClass = 0;
			e.ie_Code = k == 0 ? IECODE_NOBUTTON : k == 1 ?
			    IECODE_LBUTTON : IECODE_LBUTTON | IECODE_UP_PREFIX;
			e.ie_Qualifier = 0;
			e.ie_X = w->LeftEdge + g->LeftEdge + g->Width / 2;
			e.ie_Y = w->TopEdge + g->TopEdge + g->Height / 2;
			r->io_Command = IND_WRITEEVENT;
			r->io_Data = &e;
			r->io_Length = sizeof(e);
			DoIO((struct IORequest *)r);
		}
		break;
	}
	CloseDevice((struct IORequest *)r);
	Wait(0);
}

/* The requester on a default public screen of mode id in font tf */
static void measure(char *what, ULONG id, UWORD depth, struct TextFont *tf)
{
	struct TagItem t[7];
	struct Screen *s;
	struct Task *helper;

	SetDefaultScreenFont(tf);
	t[0].ti_Tag = SA_DisplayID;	t[0].ti_Data = id;
	t[1].ti_Tag = SA_Depth;		t[1].ti_Data = depth;
	t[2].ti_Tag = SA_SharePens;	t[2].ti_Data = TRUE;
	t[3].ti_Tag = SA_SysFont;	t[3].ti_Data = 1;
	t[4].ti_Tag = SA_PubName;	t[4].ti_Data = (IPTR)"Crash";
	t[5].ti_Tag = SA_Title;		t[5].ti_Data = (IPTR)what;
	t[6].ti_Tag = TAG_DONE;
	s = OpenScreenTagList(NULL, t);
	check(s != NULL, what);
	if (s == NULL)
		return;
	PubScreenStatus(s, 0);
	SetDefaultPubScreen("Crash");

	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)crash;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)"crash requester";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = STACK;
	t[3].ti_Tag = NP_CopyVars;	t[3].ti_Data = FALSE;
	t[4].ti_Tag = NP_CurrentDir;	t[4].ti_Data = 0;
	t[5].ti_Tag = TAG_DONE;
	answer = 0;
	SetSignal(0L, SIGBREAKF_CTRL_F);
	helper = CreateTask("crash clicker", 1, (TASKPROC)clicker, 1536);
	check(helper != NULL && CreateNewProc(t) != NULL, what);
	if (helper != NULL) {
		Wait(SIGBREAKF_CTRL_F);
		RemTask(helper);
	}
	xKPrintf("%s: %s %ld of %ld bytes\n", NAME, what, (LONG)peak, STACK);
	check(answer == 1, what);
	check(peak <= STACK - ROOM, what);

	SetDefaultPubScreen(NULL);
	CloseScreen(s);
}

/* Each mode in topaz 8 and in a disk font */
static void fonts(char *what, char *disk, ULONG id, UWORD depth)
{
	static struct TextAttr topaz = { "topaz.font", 8, 0, 0 };
	struct TextAttr ta;
	struct TextFont *tf;
	char name[48];
	char *p, *q;

	tf = OpenFont(&topaz);
	for (p = what, q = name; (*q = *p) != 0; p++, q++)
		;
	measure(name, id, depth, tf);
	CloseFont(tf);
	ta.ta_Name = disk;
	ta.ta_YSize = disk[0] == 's' ? 9 : 11;
	ta.ta_Style = ta.ta_Flags = 0;
	tf = OpenDiskFont(&ta);
	check(tf != NULL && tf->tf_YSize == ta.ta_YSize, disk);
	if (tf == NULL)
		return;
	for (p = ", "; (*q = *p) != 0; p++, q++)
		;
	for (p = disk; *p != '.' && (*q = *p) != 0; p++, q++)
		;
	*q = 0;
	measure(name, id, depth, tf);
	SetDefaultScreenFont(GfxBase->DefaultFont);
	CloseFont(tf);
}

/* The modes, in a process: OpenDiskFont needs one */
static struct Task *roottask;

static LONG modes(char *args, LONG len)
{
	struct Preferences prefs;

	maintask = FindTask(NULL);
#ifdef LC
	fonts("LC 640x200", "fixed.font", LCHIRES_ID, 1);
#else
	fonts("mono 1024x800", "sans.font", HRMONO_800_ID, 1);
	fonts("RGBI 640x400", "fixed.font", HRCOLOR_ID, 4);
	GetPrefs(&prefs, sizeof(prefs));
	prefs.EnableCLI |= OLD_LOOK;
	SetPrefs(&prefs, sizeof(prefs), FALSE);
	fonts("1.x RGBI 640x400", "sans.font", HRCOLOR_ID, 4);
#endif
	Signal(roottask, SIGBREAKF_CTRL_E);
	return 0;
}

int main(void)
{
	struct TagItem t[4];

#ifdef LC
	if (!lct_setup(NAME, 0)) {
#else
	if (!fbsetup()) {
#endif
		xKPrintf("FAIL %s: setup\n", NAME);
		return 1;
	}
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	DiskfontBase = OpenLibrary("diskfont.library", 0L);
	if (GfxBase == NULL || IntuitionBase == NULL || DOSBase == NULL ||
	    DiskfontBase == NULL) {
		xKPrintf("FAIL %s: OpenLibrary\n", NAME);
		return 1;
	}
	roottask = FindTask(NULL);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)modes;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)NAME;
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 8192;
	t[3].ti_Tag = TAG_DONE;
	if (CreateNewProc(t) == NULL) {
		xKPrintf("FAIL %s: CreateNewProc\n", NAME);
		return 1;
	}
	Wait(SIGBREAKF_CTRL_E);
	if (fails != 0)
		return 1;
	xKPrintf("PASS %s\n", NAME);
	return 0;
}
