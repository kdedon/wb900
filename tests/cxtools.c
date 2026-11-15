/*
 * The commodities in SYS:Tools/Commodity, launched as Workbench
 * launches them, with their icons' tool types.  The root program writes
 * events to input.device and checks each one's effect on the Workbench
 * screen: NoCapsLock drops Caps Lock, ClickToFront brings a window to
 * front on a double click, AutoPoint activates the window under a
 * pointer at rest, DepthMenu's list on a depth gadget picks a window,
 * and Blanker blanks when idle or on its hotkey and unblanks on input.
 * Each is ended with CXCMD_KILL.  It reports their stack use.
 */
#include <exec/exec.h>
#include <dos/dos.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <devices/timer.h>
#include <graphics/gfxbase.h>
#include <graphics/layers.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <workbench/workbench.h>
#include <libraries/commodities.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/workbench.h>
#include <proto/commodities.h>

struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Library *WorkbenchBase;
struct Library *CxBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL cxtools: %s\n", what);
		fails++;
	}
}

/* The last key past the commodities, ahead of intuition */
static UWORD seenqual, seencode, seenkeys;

static struct InputEvent *spy(struct InputEvent *events, APTR data)
{
	struct InputEvent *ie;

	for (ie = events; ie != NULL; ie = ie->ie_NextEvent)
		if (ie->ie_Class == IECLASS_RAWKEY) {
			seenkeys++;
			seencode = ie->ie_Code;
			seenqual = ie->ie_Qualifier;
		}
	return events;
}

static struct MsgPort *ioport;
static struct IOStdReq *io;
static struct InputEvent ev;
static struct Interrupt spyint;

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
}

static void mouse(WORD x, WORD y)
{
	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, 0, x, y);
}

static void button(UWORD code, WORD x, WORD y)
{
	event(IECLASS_RAWMOUSE, code, 0, x, y);
}

/* Wait in ticks of 20 ms; timer events go on meanwhile */
static void ticks(LONG n)
{
	Delay(n);
}

/* Give the input handler an event and a moment */
static void sync(void)
{
	event(IECLASS_NULL, 0, 0, 0, 0);
	ticks(2);
}

/* Whether a broker of the name is in commodities' list, and active */
static BOOL broker(char *name, BOOL active)
{
	struct List l;
	struct Node *n;
	BOOL found = FALSE;
	char *a, *b;

	NewList(&l);
	CopyBrokerList(&l);
	for (n = l.lh_Head; n->ln_Succ; n = n->ln_Succ) {
		for (a = n->ln_Name, b = name; *a && *a == *b; a++, b++)
			;
		if (*a == *b && (!active ||
		    (((struct BrokerCopy *)n)->bc_Flags & COF_ACTIVE)))
			found = TRUE;
	}
	FreeBrokerList(&l);
	return found;
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
	xKPrintf("cxtools: %s stack %d of %d bytes used\n", name,
	    (int)(top - p), (int)(top - (UBYTE *)t->tc_SPLower));
}

/* Launch SYS:Tools/Commodity/file and wait for its broker, name, to start */
static BOOL launch(char *file, char *name)
{
	char path[48];
	char *p = path, *s = "SYS:Tools/Commodity/";
	int i;

	while (*s)
		*p++ = *s++;
	for (s = file; *s; )
		*p++ = *s++;
	*p = 0;
	check(OpenWorkbenchObjectA(path, NULL), name);
	for (i = 0; i < 200; i++) {
		if (broker(name, TRUE))
			return TRUE;
		ticks(1);
	}
	check(FALSE, "broker up");
	return FALSE;
}

/* End it with CXCMD_KILL and wait for its process to go */
static void kill(char *name)
{
	struct Task *t;
	int i;

	stackuse(name);
	check(BrokerCommand(name, CXCMD_KILL) == 0, "CXCMD_KILL");
	for (i = 0; i < 200; i++) {
		Forbid();
		t = FindTask(name);
		Permit();
		if (t == NULL && !broker(name, FALSE))
			return;
		ticks(1);
	}
	check(FALSE, "commodity ends");
}

static struct Screen *wb;

static struct Window *openwin(WORD x, WORD y, WORD w, WORD h, char *title)
{
	struct TagItem t[10];

	t[0].ti_Tag = WA_Left;		t[0].ti_Data = x;
	t[1].ti_Tag = WA_Top;		t[1].ti_Data = y;
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = w;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = h;
	t[4].ti_Tag = WA_Title;		t[4].ti_Data = (IPTR)title;
	t[5].ti_Tag = WA_PubScreen;	t[5].ti_Data = (IPTR)wb;
	t[6].ti_Tag = WA_DepthGadget;	t[6].ti_Data = TRUE;
	t[7].ti_Tag = WA_DragBar;	t[7].ti_Data = TRUE;
	t[8].ti_Tag = WA_Activate;	t[8].ti_Data = TRUE;
	t[9].ti_Tag = TAG_DONE;
	return OpenWindowTagList(NULL, t);
}

/* Whether no other window is in front of w */
static BOOL front(struct Window *w)
{
	struct Layer *l;

	for (l = w->WLayer->front; l != NULL; l = l->front)
		if (l->Window != NULL)
			return FALSE;
	return TRUE;
}

/* A system gadget's centre on the screen */
static BOOL sysgad(struct Window *w, UWORD type, WORD *x, WORD *y)
{
	struct Gadget *g;
	WORD gx, gy, gw, gh;

	for (g = w->FirstGadget; g; g = g->NextGadget) {
		if ((g->GadgetType & GTYP_SYSTYPEMASK) != type)
			continue;
		gx = g->LeftEdge + ((g->Flags & GFLG_RELRIGHT) ? w->Width - 1 : 0);
		gy = g->TopEdge + ((g->Flags & GFLG_RELBOTTOM) ? w->Height - 1 : 0);
		gw = g->Width + ((g->Flags & GFLG_RELWIDTH) ? w->Width : 0);
		gh = g->Height + ((g->Flags & GFLG_RELHEIGHT) ? w->Height : 0);
		*x = w->LeftEdge + gx + gw / 2;
		*y = w->TopEdge + gy + gh / 2;
		return TRUE;
	}
	return FALSE;
}

static void nocapslock(void)
{
	key(0x20, IEQUALIFIER_CAPSLOCK);
	check(seenqual & IEQUALIFIER_CAPSLOCK, "Caps Lock without NoCapsLock");
	if (!launch("NoCaps", "NoCapsLock"))
		return;
	key(0x20, IEQUALIFIER_CAPSLOCK | IEQUALIFIER_LSHIFT);
	check(seencode == 0x20 && seenqual == IEQUALIFIER_LSHIFT,
	    "NoCapsLock drops Caps Lock");
	kill("NoCapsLock");
	key(0x20, IEQUALIFIER_CAPSLOCK);
	check(seenqual & IEQUALIFIER_CAPSLOCK, "NoCapsLock gone");
}

static void doubleclick(WORD x, WORD y)
{
	mouse(x, y);
	button(IECODE_LBUTTON, x, y);
	button(IECODE_LBUTTON | IECODE_UP_PREFIX, x, y);
	button(IECODE_LBUTTON, x, y);
	button(IECODE_LBUTTON | IECODE_UP_PREFIX, x, y);
	sync();
}

static void clicktofront(struct Window *a, struct Window *b)
{
	WORD x = a->LeftEdge + 20, y = a->TopEdge + a->Height - 10;

	WindowToFront(b);
	sync();
	check(front(b) && !front(a), "B in front");
	doubleclick(x, y);
	check(!front(a), "a double click alone does not raise");
	if (!launch("ToFront", "ClickToFront"))
		return;
	WindowToFront(b);
	sync();
	mouse(x, y);
	button(IECODE_LBUTTON, x, y);
	button(IECODE_LBUTTON | IECODE_UP_PREFIX, x, y);
	sync();
	check(!front(a), "ClickToFront: one click does not raise");
	ticks(50);
	doubleclick(x, y);
	check(front(a), "ClickToFront raises on a double click");
	check(IntuitionBase->ActiveWindow == a, "ClickToFront activates");
	kill("ClickToFront");
}

static void autopoint(struct Window *a, struct Window *b)
{
	WORD x = a->LeftEdge + 20, y = a->TopEdge + a->Height - 10;
	int i;

	ActivateWindow(b);
	mouse(b->LeftEdge + b->Width / 2, b->TopEdge + b->Height / 2);
	sync();
	check(IntuitionBase->ActiveWindow == b, "B active");
	if (!launch("AutoPoint", "AutoPoint"))
		return;
	/* the pointer keeps moving over A: no activation yet */
	for (i = 0; i < 10; i++) {
		mouse(x + (i & 1), y);
		ticks(2);
	}
	check(IntuitionBase->ActiveWindow == b, "AutoPoint waits while moving");
	ticks(15);
	sync();
	check(IntuitionBase->ActiveWindow == a, "AutoPoint activates at rest");
	kill("AutoPoint");
}

static void depthmenu(struct Window *a, struct Window *b)
{
	struct Window *w, *popup = NULL;
	WORD x, y, n, i;

	WindowToFront(b);
	ActivateWindow(b);
	sync();
	if (!launch("DepthMenu", "DepthMenu"))
		return;
	check(sysgad(b, GTYP_WDEPTH, &x, &y), "depth gadget");
	mouse(x, y);
	button(IECODE_RBUTTON, x, y);
	for (i = 0; i < 100 && popup == NULL; i++) {
		ticks(1);
		for (w = wb->FirstWindow; w; w = w->NextWindow)
			if (w->Title == NULL && (w->Flags & WFLG_BORDERLESS) &&
			    w->Width < wb->Width)
				popup = w;
	}
	check(popup != NULL, "DepthMenu's list");
	if (popup != NULL) {
		/* A's entry: the titled windows, in the screen's order */
		n = 0;
		for (w = wb->FirstWindow; w && w != a; w = w->NextWindow)
			if (w->Title != NULL)
				n++;
		x = popup->LeftEdge + popup->Width / 2;
		y = popup->TopEdge + 4 + n * popup->RPort->TxHeight +
		    popup->RPort->TxHeight / 2;
		mouse(x, y);
		ticks(2);
	}
	button(IECODE_RBUTTON | IECODE_UP_PREFIX, x, y);
	ticks(5);
	sync();
	check(front(a) && IntuitionBase->ActiveWindow == a,
	    "DepthMenu brings A to front");
	for (w = wb->FirstWindow; w; w = w->NextWindow)
		check(w != popup, "DepthMenu's list closes");
	kill("DepthMenu");
}

/* Blanker's window: borderless, the screen's size, all one pen */
static struct Window *blanked(void)
{
	struct Window *w;

	for (w = wb->FirstWindow; w; w = w->NextWindow)
		if (w->Width == wb->Width && w->Height == wb->Height &&
		    (w->Flags & WFLG_BORDERLESS) && w->Title == NULL)
			return w;
	return NULL;
}

static struct Window *waitblank(BOOL on)
{
	struct Window *w = NULL;
	int i;

	for (i = 0; i < 150; i++) {
		w = blanked();
		if ((w != NULL) == on)
			break;
		ticks(1);
	}
	return w;
}

static void blanker(void)
{
	struct Window *w;
	UWORD n;
	LONG black;

	if (!launch("Blanker", "Blanker"))
		return;
	check(blanked() == NULL, "Blanker waits");
	w = waitblank(TRUE);
	check(w != NULL, "Blanker blanks when idle");
	ticks(10);
	if (w != NULL) {
		black = ReadPixel(w->RPort, 5, 5);
		check(ReadPixel(w->RPort, w->Width - 5, w->Height - 5) == black &&
		    ReadPixel(w->RPort, w->Width / 2, w->Height / 3) == black,
		    "black window");
	}
	mouse(300, 300);
	check(waitblank(FALSE) == NULL, "Blanker unblanks on input");

	/* The hotkey blanks at once; its release does not unblank */
	n = seenkeys;
	key(0x35, IEQUALIFIER_CONTROL | IEQUALIFIER_LALT);
	key(0x35 | IECODE_UP_PREFIX, IEQUALIFIER_CONTROL | IEQUALIFIER_LALT);
	check(seenkeys == n + 1 && seencode == (0x35 | IECODE_UP_PREFIX),
	    "BLANKKEY swallowed");
	check(waitblank(TRUE) != NULL, "BLANKKEY blanks");
	ticks(10);
	check(blanked() != NULL, "key release keeps the blank");
	key(0x20, 0);
	check(waitblank(FALSE) == NULL, "a key unblanks");
	kill("Blanker");
}

int main(void)
{
	struct Window *a, *b;

	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 36L);
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	WorkbenchBase = OpenLibrary(WORKBENCH_NAME, 45L);
	CxBase = OpenLibrary("commodities.library", 0);
	ioport = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(ioport, sizeof(struct IOStdReq));
	if (DOSBase == NULL || GfxBase == NULL || IntuitionBase == NULL ||
	    WorkbenchBase == NULL || CxBase == NULL ||
	    OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL cxtools: OpenLibrary\n");
		return 1;
	}
	check(OpenWorkBench() != 0, "OpenWorkBench");
	wb = LockPubScreen("Workbench");
	spyint.is_Node.ln_Pri = 52;
	spyint.is_Code = (int (*)())spy;
	io->io_Command = IND_ADDHANDLER;
	io->io_Data = &spyint;
	DoIO((struct IORequest *)io);

	nocapslock();
	a = openwin(40, 60, 300, 160, "A");
	b = openwin(140, 100, 300, 160, "B");
	check(a != NULL && b != NULL, "windows");
	if (a != NULL && b != NULL) {
		clicktofront(a, b);
		autopoint(a, b);
		depthmenu(a, b);
	}
	if (b)
		CloseWindow(b);
	if (a)
		CloseWindow(a);
	blanker();

	stackuse("input.device");
	io->io_Command = IND_REMHANDLER;
	io->io_Data = &spyint;
	DoIO((struct IORequest *)io);
	UnlockPubScreen(NULL, wb);
	if (fails != 0)
		return 1;
	KPutStr("PASS cxtools\n");
	return 0;
}
