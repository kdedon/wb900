/*
 * Disk libraries expunged and loaded again.  A process opens and closes
 * each library, a low-memory AllocMem expunges the ones that can go, and
 * another process opens it again.  asl runs a file requester on a screen
 * of its process, cancelled through input.device, each time.
 */
#include <exec/exec.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <libraries/asl.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/asl.h>
#include <proto/alib.h>

struct IntuitionBase *IntuitionBase;
struct DosLibrary *DOSBase;
struct Library *AslBase;

static int fails;

static void check(int ok, char *what, char *name)
{
	if (!ok) {
		xKPrintf("FAIL expunge: %s: %s\n", name, what);
		fails++;
	}
}

static int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return (UBYTE)*a - (UBYTE)*b;
}

/* In load order; goes: whether its Expunge lets it go when closed */
static struct {
	char *name;
	BOOL dev, goes;
} mods[] = {
	{ "utility.library", FALSE, TRUE },
	{ "keymap.library", FALSE, FALSE },
	{ "graphics.library", FALSE, TRUE },
	{ "layers.library", FALSE, FALSE },
	{ "commodities.library", FALSE, FALSE },
	{ "intuition.library", FALSE, FALSE },
	{ "gadtools.library", FALSE, TRUE },
	{ "icon.library", FALSE, TRUE },
	{ "workbench.library", FALSE, FALSE },
	{ CONSOLENAME, TRUE, FALSE },
	{ AslName, FALSE, TRUE }
};
#define NMODS	(sizeof(mods) / sizeof(mods[0]))

static WORD cur;
static BOOL opened;
static struct Task *maintask;
static struct Screen *scr;

/* Events through input.device, which runs intuition's handler */
static struct MsgPort *port;
static struct IOStdReq *io;
static struct InputEvent ev;

static void event(UWORD code, WORD x, WORD y)
{
	ev.ie_NextEvent = NULL;
	ev.ie_Class = IECLASS_RAWMOUSE;
	ev.ie_SubClass = 0;
	ev.ie_Code = code;
	ev.ie_Qualifier = 0;
	ev.ie_X = x;
	ev.ie_Y = y;
	io->io_Command = IND_WRITEEVENT;
	io->io_Data = &ev;
	io->io_Length = sizeof(ev);
	DoIO((struct IORequest *)io);
}

#define CANCEL	105

static struct Gadget *gadget(struct Window *w, UWORD id)
{
	struct Gadget *g;

	for (g = w->FirstGadget; g; g = g->NextGadget)
		if (g->GadgetID == id && !(g->GadgetType & GTYP_SYSGADGET))
			return g;
	return NULL;
}

/* Cancel the requester once its window shows, within 4 s */
static void cancel(void)
{
	struct Window *w = NULL, *f;
	struct Gadget *g;
	WORD x, y;
	int i;

	for (i = 0; i < 200 && w == NULL; i++) {
		TimeDelay(UNIT_VBLANK, 0, 20000);
		Forbid();
		if (scr)
			for (f = scr->FirstWindow; f; f = f->NextWindow)
				if (f->Title && strcmp((char *)f->Title,
				    "Expunge") == 0 && gadget(f, CANCEL))
					w = f;
		Permit();
	}
	check(w != NULL, "requester window", AslName);
	if (w == NULL)
		return;
	TimeDelay(UNIT_VBLANK, 0, 200000);
	g = gadget(w, CANCEL);
	x = w->LeftEdge + g->LeftEdge + g->Width / 2 +
	    ((g->Flags & GFLG_RELRIGHT) ? w->Width - 1 : 0);
	y = w->TopEdge + g->TopEdge + g->Height / 2 +
	    ((g->Flags & GFLG_RELBOTTOM) ? w->Height - 1 : 0);
	event(IECODE_NOBUTTON, x, y);
	event(IECODE_LBUTTON, x, y);
	event(IECODE_LBUTTON | IECODE_UP_PREFIX, x, y);
}

static void request(void)
{
	struct IntuitionBase *ib;
	struct FileRequester *fr;
	struct TagItem t[5];

	ib = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	if (ib == NULL)
		return;
	IntuitionBase = ib;
	t[0].ti_Tag = SA_Width;		t[0].ti_Data = 640;
	t[1].ti_Tag = SA_Height;	t[1].ti_Data = 400;
	t[2].ti_Tag = SA_Depth;		t[2].ti_Data = 1;
	t[3].ti_Tag = SA_Type;		t[3].ti_Data = CUSTOMSCREEN;
	t[4].ti_Tag = TAG_DONE;
	if ((scr = OpenScreenTagList(NULL, t)) != NULL) {
		fr = AllocAslRequest(ASL_FileRequest, NULL);
		check(fr != NULL, "AllocAslRequest", AslName);
		if (fr) {
			t[0].ti_Tag = ASLFR_Screen;	t[0].ti_Data = (ULONG)scr;
			t[1].ti_Tag = ASLFR_TitleText;	t[1].ti_Data = (ULONG)"Expunge";
			t[2].ti_Tag = ASLFR_InitialDrawer; t[2].ti_Data = (ULONG)"SYS:";
			t[3].ti_Tag = TAG_DONE;
			Signal(maintask, SIGBREAKF_CTRL_E);
			check(!AslRequest(fr, t), "cancelled", AslName);
			FreeAslRequest(fr);
		}
		Forbid();
		CloseScreen(scr);
		scr = NULL;
		Permit();
	}
	CloseLibrary((struct Library *)ib);
}

/* The process: open, use and close mods[cur] */
static LONG job(char *args, LONG len)
{
	static struct IOStdReq ior;
	struct Library *lib;

	if (mods[cur].dev) {
		opened = OpenDevice(mods[cur].name, CONU_LIBRARY,
		    (struct IORequest *)&ior, 0) == 0;
		if (opened)
			CloseDevice((struct IORequest *)&ior);
		return 0;
	}
	lib = OpenLibrary(mods[cur].name, 0L);
	opened = lib != NULL;
	if (lib && strcmp(mods[cur].name, AslName) == 0) {
		AslBase = lib;
		request();
	}
	CloseLibrary(lib);
	return 0;
}

static void done(LONG rc, struct Task *t)
{
	Signal(t, SIGBREAKF_CTRL_F);
}

static void runjob(char *what)
{
	struct TagItem t[6];

	opened = FALSE;
	SetSignal(0L, SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (ULONG)job;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (ULONG)what;
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 8192;
	t[3].ti_Tag = NP_ExitCode;	t[3].ti_Data = (ULONG)done;
	t[4].ti_Tag = NP_ExitData;	t[4].ti_Data = (ULONG)maintask;
	t[5].ti_Tag = TAG_DONE;
	if (CreateNewProc(t) == NULL) {
		check(FALSE, what, mods[cur].name);
		return;
	}
	if (strcmp(mods[cur].name, AslName) == 0 &&
	    (Wait(SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F) & SIGBREAKF_CTRL_E))
		cancel();
	Wait(SIGBREAKF_CTRL_F);
	check(opened, what, mods[cur].name);
}

/* An allocation that cannot succeed runs the low-memory handlers */
static void lowmem(void)
{
	ULONG n = AvailMem(MEMF_ANY) + 0x20000L;
	APTR p;

	if ((p = AllocMem(n, MEMF_ANY)) != NULL)
		FreeMem(p, n);
}

static BOOL listed(WORD i)
{
	struct List *l = mods[i].dev ? &SysBase->DeviceList : &SysBase->LibList;
	BOOL r;

	Forbid();
	r = FindName(l, mods[i].name) != NULL;
	Permit();
	return r;
}

int main(void)
{
	port = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	if (OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0 ||
	    DOSBase == NULL) {
		KPutStr("FAIL expunge: input.device\n");
		return 1;
	}
	maintask = FindTask(NULL);
	for (cur = 0; cur < NMODS; cur++) {
		runjob("first open");
		lowmem();
		check(listed(cur) != mods[cur].goes, "low-memory expunge",
		    mods[cur].name);
		runjob("second open");
		check(listed(cur), "reloaded", mods[cur].name);
	}
	CloseDevice((struct IORequest *)io);
	if (fails != 0)
		return 1;
	KPutStr("PASS expunge\n");
	return 0;
}
