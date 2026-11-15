/* SPDX-License-Identifier: MIT */
/*
 * ScreenMode preferences on hr.gfxdriver, behind a framebuf.resource of
 * an HR card read as 1024x800 mono, with its VRAM in RAM.  OpenWorkBench()
 * takes the saved mode of ENV:Sys/screenmode.prefs: another jumper mode,
 * an unknown mode (the card's then), and the card's with Shift held.
 * Then SYS:Prefs/ScreenMode, driven by clicks, Uses RGBI and Saves
 * 1024x1024 with the 1.x look, and the files it wrote open that.
 */
/* EMUFLAGS: --ram=08-1F */
#include <exec/exec.h>
#include <dos/dos.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <devices/timer.h>
#include <graphics/displayinfo.h>
#include <graphics/gfxbase.h>
#include <graphics/hrmodeid.h>
#include <graphics/modeid.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/preferences.h>
#include <intuition/screens.h>
#include <prefs/prefhdr.h>
#include <prefs/screenmode.h>
#include <resources/framebuf.h>
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/workbench.h>
#include <proto/alib.h>

struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Library *WorkbenchBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL screenmode: %s\n", what);
		fails++;
	}
}

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

/* Events through input.device, which runs intuition's handler */
static struct MsgPort *port;
static struct IOStdReq *io;
static struct InputEvent ev;

static void event(UWORD cmd, UBYTE class, UWORD code, UWORD qual, WORD x,
    WORD y)
{
	ev.ie_NextEvent = NULL;
	ev.ie_Class = class;
	ev.ie_SubClass = 0;
	ev.ie_Code = code;
	ev.ie_Qualifier = qual;
	ev.ie_X = x;
	ev.ie_Y = y;
	io->io_Command = cmd;
	io->io_Data = &ev;
	io->io_Length = sizeof(ev);
	DoIO((struct IORequest *)io);
}

static struct Task *tool;

/* Until the tool waits for input, 2 s at most */
static void pause(void)
{
	int i, n = 0;

	for (i = 0; i < 100 && n < 2; i++) {
		TimeDelay(UNIT_VBLANK, 0, 20000);
		n = tool->tc_State == TS_WAIT ? n + 1 : 0;
	}
}

static void click(WORD x, WORD y)
{
	event(IND_WRITEEVENT, IECLASS_RAWMOUSE, IECODE_NOBUTTON, 0, x, y);
	event(IND_WRITEEVENT, IECLASS_RAWMOUSE, IECODE_LBUTTON, 0, x, y);
	event(IND_WRITEEVENT, IECLASS_RAWMOUSE,
	    IECODE_LBUTTON | IECODE_UP_PREFIX, 0, x, y);
	pause();
}

/* The widest gadget of id: a listview's list rather than its arrows */
static struct Gadget *gadget(struct Window *w, UWORD id)
{
	struct Gadget *g, *best = NULL;

	for (g = w->FirstGadget; g; g = g->NextGadget)
		if (g->GadgetID == id && !(g->GadgetType & GTYP_SYSGADGET) &&
		    (best == NULL || g->Width > best->Width))
			best = g;
	return best;
}

/* The tool's gadget id, in the middle or dy pixels down its left end */
static void press(struct Window *w, UWORD id, WORD dy)
{
	struct Gadget *g = gadget(w, id);

	check(g != NULL, "gadget");
	if (g)
		click(w->LeftEdge + g->LeftEdge + (dy ? 20 : g->Width / 2),
		    w->TopEdge + g->TopEdge + (dy ? dy : g->Height / 2));
}

/* The tool's gadget ids */
enum { GAD_LIST = 1, GAD_SIZE, GAD_WIDTH, GAD_HEIGHT, GAD_LOOK, GAD_SAVE,
	GAD_USE, GAD_CANCEL };

/* Its list's row n: rows of the font under a 3 pixel frame */
static void row(struct Window *w, WORD n)
{
	WORD h = w->WScreen->Font->ta_YSize;

	press(w, GAD_LIST, 3 + n * h + h / 2);
}

static BOOL streq(char *a, char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return *a == *b;
}

static struct Window *toolwin(struct Screen *s)
{
	struct Window *w;

	for (w = s->FirstWindow; w; w = w->NextWindow)
		if (w->Title && streq((char *)w->Title, "ScreenMode"))
			return w;
	return NULL;
}

static struct Window *waitwin(struct Screen *s, BOOL open)
{
	struct Window *w = NULL;
	int i;

	for (i = 0; i < 250; i++) {
		w = toolwin(s);
		if ((w != NULL) == open)
			break;
		TimeDelay(UNIT_VBLANK, 0, 20000);
	}
	return w;
}

#define PREFSLEN	62

/* A file of the tool's layout; FALSE if it isn't one */
static BOOL readprefs(char *path, struct ScreenModePrefs *smp)
{
	ULONG buf[PREFSLEN / 4 + 2];
	UBYTE *b = (UBYTE *)buf;
	BPTR fh;
	LONG n;

	if ((fh = Open(path, MODE_OLDFILE)) == NULL)
		return FALSE;
	n = Read(fh, buf, sizeof(buf));
	Close(fh);
	if (n != PREFSLEN || buf[0] != ID_FORM || buf[1] != PREFSLEN - 8 ||
	    buf[2] != ID_PREF || buf[3] != ID_PRHD || buf[4] != 6 ||
	    *(ULONG *)(b + 26) != ID_SCRM || *(ULONG *)(b + 30) != sizeof(*smp))
		return FALSE;
	CopyMem(b + 34, smp, sizeof(*smp));
	return TRUE;
}

/* ENV:Sys/screenmode.prefs: FORM PREF of a PRHD and an SCRM chunk */
static void saveprefs(ULONG id, UWORD w, UWORD h, UWORD d)
{
	static ULONG hdr[5] = { ID_FORM, PREFSLEN - 8, ID_PREF, ID_PRHD, 6 };
	static UBYTE ph[6];
	static ULONG scrm[2] = { ID_SCRM, sizeof(struct ScreenModePrefs) };
	struct ScreenModePrefs smp, back;
	BPTR fh;
	WORD i;

	for (i = 0; i < 4; i++)
		smp.smp_Reserved[i] = 0;
	smp.smp_Control = 0;
	smp.smp_DisplayID = id;
	smp.smp_Width = w;
	smp.smp_Height = h;
	smp.smp_Depth = d;
	fh = Open("ENV:Sys/screenmode.prefs", MODE_NEWFILE);
	check(fh != NULL, "write ENV:Sys/screenmode.prefs");
	if (fh == NULL)
		return;
	Write(fh, hdr, sizeof(hdr));
	Write(fh, ph, sizeof(ph));
	Write(fh, scrm, sizeof(scrm));
	Write(fh, &smp, sizeof(smp));
	Close(fh);
	check(readprefs("ENV:Sys/screenmode.prefs", &back) &&
	    back.smp_DisplayID == id && back.smp_Width == w &&
	    back.smp_Height == h && back.smp_Depth == d, "prefs read back");
}

/* Close the Workbench screen and open it again: its mode, size, depth */
static void reopen(ULONG id, WORD w, WORD h, WORD d, char *what)
{
	struct Screen *s;

	check(CloseWorkBench(), what);
	s = (struct Screen *)OpenWorkBench();
	check(s != NULL, what);
	if (s == NULL)
		return;
	if (GetVPModeID(&s->ViewPort) != id || s->Width != w ||
	    s->Height != h || GetBitMapAttr(s->RastPort.BitMap, BMA_DEPTH) != d) {
		xKPrintf("FAIL screenmode: %s: mode %lx %dx%dx%d\n", what,
		    GetVPModeID(&s->ViewPort), s->Width, s->Height,
		    (int)GetBitMapAttr(s->RastPort.BitMap, BMA_DEPTH));
		fails++;
	}
}

static void choose(void)
{
	check(OpenWorkBench() != 0, "OpenWorkBench");
	check(NextDisplayInfo(INVALID_ID) == HRMONO_800_ID &&
	    ModeNotAvailable(HRMONO_800_ID) == 0 &&
	    ModeNotAvailable(HRMONO_1024_ID) == DI_AVAIL_NOMONITOR &&
	    ModeNotAvailable(HRCOLOR_ID) == DI_AVAIL_NOMONITOR,
	    "the card's mode first, the jumper's others not detected");
	reopen(HRMONO_800_ID, 1024, 800, 1, "no saved mode");

	saveprefs(HRMONO_1024_ID, 1024, 1024, 1);
	reopen(HRMONO_1024_ID, 1024, 1024, 1, "saved 1024x1024");
	check(ModeNotAvailable(HRMONO_1024_ID) == 0 &&
	    ModeNotAvailable(HRMONO_800_ID) == DI_AVAIL_NOMONITOR,
	    "the jumper taken as 1024x1024");

	saveprefs(HRCOLOR_ID, 640, 400, 4);
	reopen(HRCOLOR_ID, 640, 400, 4, "saved RGBI");

	saveprefs(NTSC_MONITOR_ID | HIRES_KEY, 640, 200, 2);
	reopen(HRMONO_800_ID, 1024, 800, 1, "saved mode not on the display");

	saveprefs(HRMONO_1024_ID, 1024, 1024, 1);
	event(IND_ADDEVENT, IECLASS_RAWKEY, RAWKEY_LSHIFT, IEQUALIFIER_LSHIFT,
	    0, 0);
	reopen(HRMONO_800_ID, 1024, 800, 1, "Shift held");
	event(IND_ADDEVENT, IECLASS_RAWKEY, RAWKEY_LSHIFT | IECODE_UP_PREFIX, 0,
	    0, 0);
	DeleteFile("ENV:Sys/screenmode.prefs");
}

/* Run the tool, click its list's row n (and Look), then button id */
static void runtool(WORD n, BOOL look, UWORD id)
{
	struct Screen *s = LockPubScreen("Workbench");
	struct Window *w;

	check(OpenWorkbenchObjectA("SYS:Prefs/ScreenMode", NULL), "launch");
	w = waitwin(s, TRUE);
	check(w != NULL, "ScreenMode's window");
	if (w != NULL) {
		tool = w->UserPort->mp_SigTask;
		pause();
		row(w, n);
		if (look)
			press(w, GAD_LOOK, 0);
		press(w, id, 0);
		check(waitwin(s, FALSE) == NULL, "ScreenMode closed");
	}
	UnlockPubScreen(NULL, s);
}

static void prefstool(void)
{
	struct ScreenModePrefs smp;
	struct Preferences p;
	BPTR fh;
	LONG n;

	runtool(2, FALSE, GAD_USE);
	check(readprefs("ENV:Sys/screenmode.prefs", &smp) &&
	    smp.smp_DisplayID == HRCOLOR_ID && smp.smp_Width == 640 &&
	    smp.smp_Height == 400 && smp.smp_Depth == 4, "Use: ENV:");
	check(!readprefs("ENVARC:Sys/screenmode.prefs", &smp),
	    "Use: no ENVARC:");
	reopen(HRCOLOR_ID, 640, 400, 4, "the mode used");

	runtool(1, TRUE, GAD_SAVE);
	check(readprefs("ENV:Sys/screenmode.prefs", &smp) &&
	    smp.smp_DisplayID == HRMONO_1024_ID && smp.smp_Width == 1024 &&
	    smp.smp_Height == 1024 && smp.smp_Depth == 1, "Save: ENV:");
	check(readprefs("ENVARC:Sys/screenmode.prefs", &smp) &&
	    smp.smp_DisplayID == HRMONO_1024_ID, "Save: ENVARC:");
	n = 0;
	if ((fh = Open("DEVS:system-configuration", MODE_OLDFILE)) != NULL) {
		n = Read(fh, &p, sizeof(p));
		Close(fh);
	}
	check(n == sizeof(p) && (p.EnableCLI & OLD_LOOK) && p.color0 == 0x05A,
	    "Save: the 1.x look");
	reopen(HRMONO_1024_ID, 1024, 1024, 1, "the mode saved");

	runtool(0, FALSE, GAD_CANCEL);
	check(readprefs("ENV:Sys/screenmode.prefs", &smp) &&
	    smp.smp_DisplayID == HRMONO_1024_ID, "Cancel");
}

/* ENV: and ENVARC: in drawers of SYS:, with their Sys drawers */
static BOOL assign(char *name, char *dir, char *sys)
{
	BPTR l;

	UnLock(CreateDir(dir));
	UnLock(CreateDir(sys));
	return (l = Lock(dir, ACCESS_READ)) != NULL && AssignLock(name, l);
}

int main(void)
{
	port = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
	if (!fbsetup() ||
	    OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL screenmode: setup\n");
		return 1;
	}
	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	WorkbenchBase = OpenLibrary(WORKBENCH_NAME, 45L);
	if (DOSBase == NULL || GfxBase == NULL || IntuitionBase == NULL ||
	    WorkbenchBase == NULL) {
		KPutStr("FAIL screenmode: OpenLibrary\n");
		return 1;
	}
	check(assign("ENV", "SYS:TEnv", "SYS:TEnv/Sys") &&
	    assign("ENVARC", "SYS:TArc", "SYS:TArc/Sys"),
	    "ENV: and ENVARC:");
	choose();
	prefstool();
	check(CloseWorkBench(), "CloseWorkBench");
	if (fails != 0)
		return 1;
	KPutStr("PASS screenmode\n");
	return 0;
}
