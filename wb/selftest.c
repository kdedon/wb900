/* SPDX-License-Identifier: MIT */
/*
 * Workbench without a display: a process writes a user's events to
 * input.device while Workbench runs.  It opens the System disk and the
 * Demos drawer with double clicks, checks the icons' pixels, selects
 * Hello with a click and a lasso, drags it, launches it, shows all files
 * and cleans up through the menus, and quits.  Each check prints a FAIL
 * line when it fails.
 */
#include "workbench.h"
#include <devices/input.h>
#include <devices/inputevent.h>

extern struct Menu *wbmenus;
extern BOOL busy;

static struct Task *wbtask;
static struct MsgPort *inport;
static struct IOStdReq *inio;
static LONG result;
static WORD fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL wbboot: %s\n", what);
		fails++;
	}
}

static int streq(char *a, char *b)
{
	return Stricmp(a, b) == 0;
}

static void event(UBYTE class, UWORD code, WORD x, WORD y)
{
	struct InputEvent ie;

	memset(&ie, 0, sizeof(ie));
	ie.ie_Class = class;
	ie.ie_Code = code;
	ie.ie_X = x;
	ie.ie_Y = y;
	inio->io_Command = IND_WRITEEVENT;
	inio->io_Data = &ie;
	inio->io_Length = sizeof(ie);
	DoIO((struct IORequest *)inio);
}

/* Give Workbench time to handle what came so far */
static void settle(void)
{
	Delay(5);
	while (busy || !IsMsgPortEmpty(idport))
		Delay(2);
}

static void mouse(WORD x, WORD y)
{
	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, x, y);
}

static void click(WORD x, WORD y)
{
	mouse(x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON, x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, x, y);
	settle();
}

/* With the button down from x0, y0 to x1, y1 in steps */
static void drag(WORD x0, WORD y0, WORD x1, WORD y1)
{
	WORD i;

	mouse(x0, y0);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON, x0, y0);
	settle();
	for (i = 1; i <= 4; i++) {
		mouse(x0 + (x1 - x0) * i / 4, y0 + (y1 - y0) * i / 4);
		settle();
	}
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, x1, y1);
	settle();
}

/* The screen position of the centre of an icon's image */
static void iconpos(struct WBWin *ww, struct WBIcon *in, WORD *x, WORD *y)
{
	struct Window *w = ww->ww_Win;

	*x = w->LeftEdge + w->BorderLeft + in->in_X + 4 +
	    in->in_Obj->do_Gadget.Width / 2;
	*y = w->TopEdge + w->BorderTop + in->in_Y + 4 +
	    in->in_Obj->do_Gadget.Height / 2;
}

static struct WBIcon *findicon(struct WBWin *ww, char *name)
{
	struct WBIcon *in;

	if (ww == NULL)
		return NULL;
	FOREACH(struct WBIcon *, in, &ww->ww_Icons)
		if (streq(in->in_Name, name))
			return in;
	return NULL;
}

/* The window of path, waiting up to 2 s for it */
static struct WBWin *waitwin(STRPTR path)
{
	struct WBWin *ww = NULL;
	WORD i;

	for (i = 0; i < 20 && (ww = findpath(path)) == NULL; i++)
		Delay(5);
	return ww;
}

/* The pen of pixel x, y of a planar image */
static UWORD imgpen(struct Image *im, WORD x, WORD y)
{
	UWORD wpr = (im->Width + 15) >> 4, pen = 0, d;
	UWORD *row;

	for (d = 0; d < im->Depth; d++) {
		row = im->ImageData + (d * im->Height + y) * wpr;
		if (row[x >> 4] & (0x8000 >> (x & 15)))
			pen |= 1 << d;
	}
	return pen;
}

/* Whether the icon's image is on the window, complemented when comp */
static BOOL pixels(struct WBWin *ww, struct WBIcon *in, UWORD comp)
{
	struct RastPort *rp = ww->ww_Win->RPort;
	struct Image *im = in->in_Obj->do_Gadget.GadgetRender;
	UWORD mask = GetBitMapAttr(rp->BitMap, BMA_DEPTH) == 1 ? 1 : 3;
	WORD x, y;

	for (y = 0; y < im->Height; y++)
		for (x = 0; x < im->Width; x++)
			if (ReadPixel(rp, in->in_X + 4 + x, in->in_Y + 4 + y) !=
			    ((imgpen(im, x, y) ^ comp) & mask))
				return FALSE;
	return TRUE;
}

/* A menu item picked with the right button */
static void pick(WORD menu, WORD item)
{
	struct Screen *s = scr;
	struct Menu *m = wbmenus;
	struct MenuItem *it;
	WORD x, y;

	while (menu-- > 0)
		m = m->NextMenu;
	for (it = m->FirstItem; item-- > 0; it = it->NextItem)
		;
	x = m->LeftEdge + 4;
	mouse(x, 2);
	event(IECLASS_RAWMOUSE, IECODE_RBUTTON, x, 2);
	settle();
	x = m->LeftEdge + it->LeftEdge + it->Width / 2;
	y = s->BarHeight + 1 + it->TopEdge + it->Height / 2;
	mouse(x, y);
	mouse(x + 1, y);
	settle();
	event(IECLASS_RAWMOUSE, IECODE_RBUTTON | IECODE_UP_PREFIX, x + 1, y);
	settle();
}

static struct Task *waittask(char *name, BOOL gone)
{
	struct Task *t = NULL;
	WORD i;

	for (i = 0; i < 40; i++) {
		Forbid();
		t = FindTask(name);
		Permit();
		if ((t == NULL) == gone)
			break;
		Delay(5);
	}
	return t;
}

static void stackuse(char *name)
{
	struct Task *t = FindTask(name);
	UBYTE *p, *top;

	if (t == NULL)
		return;
	p = (UBYTE *)t->tc_SPLower + 2;
	top = (UBYTE *)t->tc_SPUpper;
	while (p < top && *p == 0)
		p++;
	xKPrintf("wbboot: %s stack %d of %d bytes used\n", name,
	    (int)(top - p), (int)(top - (UBYTE *)t->tc_SPLower));
}

static void script(void)
{
	struct WBWin *sys, *demos;
	struct WBIcon *in, *hello;
	struct Task *t;
	WORD x, y, x0, n;

	in = findicon(root, "System:");
	check(in != NULL, "System disk icon");
	if (in == NULL)
		return;
	check(pixels(root, in, 0), "disk icon pixels");
	iconpos(root, in, &x, &y);
	click(x, y);
	click(x, y);
	sys = waitwin("System:");
	check(sys != NULL && findicon(sys, "Demos") != NULL &&
	    findicon(sys, "Trashcan") != NULL && findicon(sys, "C") == NULL,
	    "System drawer");
	if (sys == NULL || (in = findicon(sys, "Demos")) == NULL)
		return;
	check(in->in_Obj->do_Type == WBDRAWER && pixels(sys, in, 0),
	    "drawer icon pixels");
	iconpos(sys, in, &x, &y);
	click(x, y);
	click(x, y);
	demos = waitwin("System:Demos");
	hello = findicon(demos, "Hello");
	check(hello != NULL && findicon(demos, "Notes") != NULL &&
	    findicon(demos, "ReadMe") == NULL, "Demos drawer");
	if (hello == NULL)
		return;
	check(hello->in_Obj->do_Type == WBTOOL && pixels(demos, hello, 0),
	    "Hello icon pixels");

	/* a click selects it, one beside deselects it, a lasso again */
	iconpos(demos, hello, &x, &y);
	click(x, y);
	check(hello->in_Selected && pixels(demos, hello, 3), "selected Hello");
	click(x, y + 60);
	check(!hello->in_Selected && pixels(demos, hello, 0), "deselected Hello");
	x0 = demos->ww_Win->LeftEdge + demos->ww_Win->BorderLeft + hello->in_X;
	n = demos->ww_Win->TopEdge + demos->ww_Win->BorderTop + hello->in_Y;
	drag(x0 + hello->in_Box.MinX - 3, n + hello->in_Box.MinY - 3,
	    x0 + hello->in_Box.MaxX + 3, n + hello->in_Box.MaxY + 3);
	check(hello->in_Selected, "lasso");

	/* dragged 40 to the right */
	x0 = hello->in_X;
	drag(x, y, x + 40, y + 10);
	check(hello->in_X == x0 + 40 && pixels(demos, hello, 3), "drag");

	/* a double click launches it */
	iconpos(demos, hello, &x, &y);
	click(x, y);
	click(x, y);
	t = waittask("Hello", FALSE);
	check(t != NULL, "Hello launched");
	if (t != NULL) {
		Delay(10);
		Signal(t, SIGBREAKF_CTRL_C);
		check(waittask("Hello", TRUE) == NULL, "Hello ended");
	}

	/* Notes is a project: its default tool Hello opens it */
	in = findicon(demos, "Notes");
	iconpos(demos, in, &x, &y);
	click(x, y);
	click(x, y);
	t = waittask("Hello", FALSE);
	check(t != NULL, "Notes opened");
	if (t != NULL) {
		Delay(10);
		Signal(t, SIGBREAKF_CTRL_C);
		check(waittask("Hello", TRUE) == NULL, "Hello ended again");
	}

	/* Window: Show All Files, then Clean Up */
	pick(1, 4);
	check(findicon(demos, "ReadMe") != NULL, "Show All Files");
	/* move Notes onto Hello's column, then clean up: a row again */
	in = findicon(demos, "Notes");
	iconpos(demos, in, &x, &y);
	drag(x, y, x, y + 40);
	check(in->in_Y == 46, "drag Notes");
	pick(1, 2);
	x0 = 0;
	FOREACH(struct WBIcon *, in, &demos->ww_Icons) {
		check(in->in_Y == 6 && in->in_X + in->in_Box.MinX >= x0,
		    "Clean Up");
		x0 = in->in_X + in->in_Box.MaxX;
	}

	/* Workbench: Backdrop off and on again */
	pick(0, 0);
	check(root != NULL && !(root->ww_Win->Flags & WFLG_BACKDROP) &&
	    findicon(root, "System:") != NULL, "Backdrop off");
	pick(0, 0);
	check(root != NULL && (root->ww_Win->Flags & WFLG_BACKDROP),
	    "Backdrop on");
	stackuse("Workbench");
}

static LONG driver(char *args, LONG len)
{
	if ((inport = CreateMsgPort()) != NULL &&
	    (inio = (struct IOStdReq *)CreateIORequest(inport,
	    sizeof(*inio))) != NULL &&
	    OpenDevice("input.device", 0, (struct IORequest *)inio, 0) == 0) {
		KPutStr("wbboot: no display: writing the events to input.device\n");
		settle();
		script();
		/* Workbench: Quit */
		pick(0, 4);
		CloseDevice((struct IORequest *)inio);
	} else {
		check(FALSE, "input.device");
	}
	DeleteIORequest((struct IORequest *)inio);
	DeleteMsgPort(inport);
	result = fails ? RETURN_FAIL : RETURN_OK;
	Signal(wbtask, SIGBREAKF_CTRL_E);
	return 0;
}

/* Start the driver */
BOOL selftest(void)
{
	struct TagItem t[4];

	wbtask = FindTask(NULL);
	SetSignal(0L, SIGBREAKF_CTRL_E);
	t[0].ti_Tag = NP_Entry;		t[0].ti_Data = (IPTR)driver;
	t[1].ti_Tag = NP_Name;		t[1].ti_Data = (IPTR)"WBDriver";
	t[2].ti_Tag = NP_StackSize;	t[2].ti_Data = 2048;
	t[3].ti_Tag = TAG_DONE;
	return CreateNewProc(t) != NULL;
}

/* The driver's result, once it ends */
LONG selftestdone(void)
{
	Wait(SIGBREAKF_CTRL_E);
	return result;
}
