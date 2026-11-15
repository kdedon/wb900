/* SPDX-License-Identifier: MIT */
/*
 * Workbench without a display: a process writes a user's events to
 * input.device while Workbench runs.  It moves the Ram Disk, opens the
 * System disk and the Utilities and Demos drawers with double clicks,
 * checks the icons' pixels and labels, selects
 * Hello with a click and a lasso, drags it, launches it, opens Notes,
 * shows all files and cleans up through the menus.  It uses an AppIcon
 * and an AppMenuItem, scrolls a narrowed drawer, snapshots it, copies
 * Hello to the Ram Disk, checks the backdrop's pattern, and quits.  Each
 * check prints a FAIL line when it fails.
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

	*x = w->LeftEdge + w->BorderLeft + in->in_X - ww->ww_OffX + 4 +
	    in->in_Obj->do_Gadget.Width / 2;
	*y = w->TopEdge + w->BorderTop + in->in_Y - ww->ww_OffY + 4 +
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
			if (ReadPixel(rp, in->in_X - ww->ww_OffX + 4 + x,
			    in->in_Y - ww->ww_OffY + 4 + y) !=
			    ((imgpen(im, x, y) ^ comp) & mask))
				return FALSE;
	return TRUE;
}

/*
 * Workbench may wait in ClearMenuStrip() while a menu is up, so wait
 * until the menu handler waits for input, not for a lock or a window.
 */
static void menuwait(void)
{
	struct Task *t;

	do {
		Delay(2);
		Forbid();
		t = FindTask("Intuition menu handler");
		Permit();
	} while (t != NULL && (t->tc_State != TS_WAIT ||
	    (t->tc_SigWait & (SIGF_SINGLE | SIGF_INTUITION))));
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
	menuwait();
	x = m->LeftEdge + it->LeftEdge + it->Width / 2;
	y = s->BarHeight + 1 + it->TopEdge + it->Height / 2;
	mouse(x, y);
	mouse(x + 1, y);
	menuwait();
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

/* Whether the screen shows its background in r, screen coordinates */
static BOOL backdrop(struct Rectangle *r)
{
	static UWORD halftone[] = { 0x3333, 0xCCCC };
	struct DrawInfo *dri = GetScreenDrawInfo(scr);
	UWORD fill = dri->dri_Pens[FILLPEN], back = dri->dri_Pens[BACKGROUNDPEN];
	BOOL plain = scr->LayerInfo.BlankHook == NULL;
	WORD x, y;
	LONG pen;

	FreeScreenDrawInfo(scr, dri);
	for (y = r->MinY; y <= r->MaxY; y++)
		for (x = r->MinX; x <= r->MaxX; x++) {
			pen = !plain && (halftone[y & 1] & (0x8000 >> (x & 15))) ?
			    fill : back;
			if (ReadPixel(&scr->RastPort, x, y) != pen)
				return FALSE;
		}
	return TRUE;
}

/*
 * Whether the icon's label is its text in TEXTPEN on a box of
 * BACKGROUNDPEN, 2 pixels wider each side and 1 taller
 */
static BOOL labelled(struct WBWin *ww, struct WBIcon *in)
{
	struct RastPort *rp = ww->ww_Win->RPort, t;
	struct DrawInfo *dri = GetScreenDrawInfo(scr);
	struct TextExtent te;
	struct BitMap *bm;
	WORD n = strlen(in->in_Name), x, y, x0, y0, bw, bh;
	BOOL ok = TRUE;

	if (in->in_Name[n - 1] == ':')
		n--;
	TextExtent(rp, in->in_Name, n, &te);
	bw = te.te_Width + 4;
	bh = te.te_Height + 2;
	x0 = in->in_X - ww->ww_OffX + in->in_Box.MinX +
	    (in->in_Box.MaxX - in->in_Box.MinX + 1 - bw) / 2;
	y0 = in->in_Y - ww->ww_OffY + in->in_Box.MaxY - bh + 1;
	bm = AllocBitMap(bw, bh, GetBitMapAttr(rp->BitMap, BMA_DEPTH),
	    BMF_CLEAR, NULL);
	if (bm == NULL) {
		FreeScreenDrawInfo(scr, dri);
		return FALSE;
	}
	InitRastPort(&t);
	t.BitMap = bm;
	SetFont(&t, rp->Font);
	SetRast(&t, dri->dri_Pens[BACKGROUNDPEN]);
	SetAPen(&t, dri->dri_Pens[TEXTPEN]);
	SetDrMd(&t, JAM1);
	Move(&t, 2, 1 - te.te_Extent.MinY);
	Text(&t, in->in_Name, n);
	for (y = 0; y < bh && ok; y++)
		for (x = 0; x < bw && ok; x++)
			ok = ReadPixel(rp, x0 + x, y0 + y) == ReadPixel(&t, x, y);
	FreeBitMap(bm);
	FreeScreenDrawInfo(scr, dri);
	return ok;
}

/* The icon's box on the screen */
static void screenbox(struct WBWin *ww, struct WBIcon *in, struct Rectangle *r)
{
	struct Window *w = ww->ww_Win;

	r->MinX = w->LeftEdge + w->BorderLeft + in->in_X - ww->ww_OffX +
	    in->in_Box.MinX;
	r->MinY = w->TopEdge + w->BorderTop + in->in_Y - ww->ww_OffY +
	    in->in_Box.MinY;
	r->MaxX = r->MinX + in->in_Box.MaxX - in->in_Box.MinX;
	r->MaxY = r->MinY + in->in_Box.MaxY - in->in_Box.MinY;
}

/* The next AppMessage, waiting up to 2 s; its first file in a static name */
static char argname[PATHLEN];

static struct AppMessage *appmsg(struct MsgPort *mp)
{
	struct AppMessage *am = NULL;
	WORD i;

	for (i = 0; i < 40 && (am = (struct AppMessage *)GetMsg(mp)) == NULL;
	    i++)
		Delay(5);
	argname[0] = 0;
	if (am != NULL && am->am_NumArgs > 0 &&
	    NameFromLock(am->am_ArgList[0].wa_Lock, argname, PATHLEN))
		AddPart(argname, am->am_ArgList[0].wa_Name, PATHLEN);
	return am;
}

static BOOL gotmsg(struct MsgPort *mp, UWORD type, IPTR id, LONG n)
{
	struct AppMessage *am = appmsg(mp);
	BOOL ok = am != NULL && am->am_Type == type && am->am_ID == id &&
	    am->am_NumArgs == n;

	if (am != NULL)
		ReplyMsg((struct Message *)am);
	return ok;
}

/* An AppIcon on the backdrop: opened, a file dropped on it, moved, removed */
static void appicon(struct WBWin *demos, struct MsgPort *mp)
{
	struct DiskObject *dobj = GetDefDiskObject(WBTOOL);
	struct WBIcon *in = NULL, *hello = findicon(demos, "Hello");
	struct AppIcon *ai;
	struct Rectangle r;
	WORD i, x, y, y0, hx, hy;

	ai = AddAppIconA(5, 6, "AppIcon", mp, NULL, dobj, NULL);
	for (i = 0; i < 20 && (in = findicon(root, "AppIcon")) == NULL; i++)
		Delay(5);
	check(ai != NULL && in != NULL && in->in_App == dobj, "AppIcon shown");
	if (in == NULL)
		return;
	check(pixels(root, in, 0), "AppIcon pixels");
	iconpos(root, in, &x, &y);
	click(x, y);
	click(x, y);
	check(gotmsg(mp, AMTYPE_APPICON, 5, 0), "AppIcon opened");
	iconpos(demos, hello, &hx, &hy);
	drag(hx, hy, x, y);
	check(gotmsg(mp, AMTYPE_APPICON, 5, 1) &&
	    streq(argname, "System:Demos/Hello"), "Hello dropped on the AppIcon");

	/* moved down, the pattern is where it was */
	screenbox(root, in, &r);
	y0 = in->in_Y;
	drag(x, y, x, y + 100);
	check(in->in_Y == y0 + 100 && backdrop(&r), "AppIcon moved");
	screenbox(root, in, &r);
	check(RemoveAppIcon(ai), "RemoveAppIcon");
	for (i = 0; i < 20 && findicon(root, "AppIcon") != NULL; i++)
		Delay(5);
	settle();
	check(findicon(root, "AppIcon") == NULL && backdrop(&r),
	    "AppIcon removed");
	FreeDiskObject(dobj);
}

static WORD nmenus(void)
{
	struct Menu *m;
	WORD n = 0;

	for (m = wbmenus; m != NULL; m = m->NextMenu)
		n++;
	return n;
}

/* An AppMenuItem in the Tools menu, picked with Hello selected */
static void appmenu(struct WBWin *demos, struct MsgPort *mp)
{
	struct AppMenuItem *ami;
	WORD i, x, y;

	ami = AddAppMenuItemA(7, 8, "Tool Item", mp, NULL);
	for (i = 0; i < 20 && nmenus() != 4; i++)
		Delay(5);
	check(ami != NULL && nmenus() == 4, "Tools menu");
	if (nmenus() != 4)
		return;
	iconpos(demos, findicon(demos, "Hello"), &x, &y);
	click(x, y);
	pick(3, 0);
	check(gotmsg(mp, AMTYPE_APPMENUITEM, 7, 1) &&
	    streq(argname, "System:Demos/Hello"), "AppMenuItem picked");
	check(RemoveAppMenuItem(ami), "RemoveAppMenuItem");
	for (i = 0; i < 20 && nmenus() != 3; i++)
		Delay(5);
	check(nmenus() == 3, "no Tools menu");
}

/* The scroller's knob on the screen */
static void knob(struct WBWin *ww, WORD i, struct Rectangle *r)
{
	struct Window *w = ww->ww_Win;
	struct Gadget *g = &ww->ww_Scroll[i];
	struct PropInfo *pi = &ww->ww_Prop[i];
	WORD k;

	if (i == HSCROLL) {
		k = (ULONG)pi->CWidth * pi->HorizBody / MAXBODY;
		r->MinX = w->LeftEdge + g->LeftEdge + 1 +
		    (ULONG)(pi->CWidth - k) * pi->HorizPot / MAXPOT;
		r->MaxX = r->MinX + k - 1;
		r->MinY = w->TopEdge + w->Height - 1 + g->TopEdge + 1;
		r->MaxY = r->MinY + pi->CHeight - 1;
	} else {
		k = (ULONG)pi->CHeight * pi->VertBody / MAXBODY;
		r->MinX = w->LeftEdge + w->Width - 1 + g->LeftEdge + 1;
		r->MaxX = r->MinX + pi->CWidth - 1;
		r->MinY = w->TopEdge + g->TopEdge + 1 +
		    (ULONG)(pi->CHeight - k) * pi->VertPot / MAXPOT;
		r->MaxY = r->MinY + k - 1;
	}
}

/* Whether the icons that the window shows whole are drawn */
static BOOL shown(struct WBWin *ww)
{
	struct Window *w = ww->ww_Win;
	struct WBIcon *in;
	WORD x, y, n = 0;

	FOREACH(struct WBIcon *, in, &ww->ww_Icons) {
		x = in->in_X - ww->ww_OffX;
		y = in->in_Y - ww->ww_OffY;
		if (x + in->in_Box.MinX < 0 || y + in->in_Box.MinY < 0 ||
		    x + in->in_Box.MaxX >= w->GZZWidth ||
		    y + in->in_Box.MaxY >= w->GZZHeight)
			continue;
		if (!pixels(ww, in, in->in_Selected ? 3 : 0))
			return FALSE;
		n++;
	}
	return n > 0;
}

/* Utilities opens showing each of its 7 tools whole */
static void utilities(struct WBWin *sys)
{
	struct WBIcon *in = findicon(sys, "Utilities");
	struct WBWin *ww;
	WORD x, y, n = 0;

	check(in != NULL, "Utilities icon");
	if (in == NULL)
		return;
	iconpos(sys, in, &x, &y);
	click(x, y);
	click(x, y);
	ww = waitwin("System:Utilities");
	check(ww != NULL, "Utilities drawer");
	if (ww == NULL)
		return;
	FOREACH(struct WBIcon *, in, &ww->ww_Icons) {
		x = in->in_X - ww->ww_OffX;
		y = in->in_Y - ww->ww_OffY;
		check(x + in->in_Box.MinX >= 0 && y + in->in_Box.MinY >= 0 &&
		    x + in->in_Box.MaxX < ww->ww_Win->GZZWidth &&
		    y + in->in_Box.MaxY < ww->ww_Win->GZZHeight, in->in_Name);
		n++;
	}
	check(n == 7, "7 utilities");
	click(ww->ww_Win->LeftEdge + 4, ww->ww_Win->TopEdge + 4);
	check(findpath("System:Utilities") == NULL, "Utilities closed");
}

/* Narrowed, Demos scrolls; cleaned up it fits its width again */
static void scrolling(struct WBWin *demos)
{
	struct Window *w = demos->ww_Win;
	struct Rectangle r;
	WORD x, y;

	x = w->LeftEdge + w->Width - 3;
	y = w->TopEdge + w->Height - 3;
	drag(x, y, x - 260, y);
	check(w->Width < 160 && demos->ww_Prop[HSCROLL].HorizBody < MAXBODY,
	    "narrowed Demos scrolls");
	/* a click right of the knob pages right, a drag goes back */
	knob(demos, HSCROLL, &r);
	click(r.MaxX + 3, (r.MinY + r.MaxY) / 2);
	check(demos->ww_OffX > 0 && shown(demos), "scrolled right");
	knob(demos, HSCROLL, &r);
	x = (r.MinX + r.MaxX) / 2;
	y = (r.MinY + r.MaxY) / 2;
	drag(x, y, x - 200, y);
	check(demos->ww_OffX == demos->ww_Virt.MinX && shown(demos),
	    "scrolled back");
	pick(1, 2);
	check(demos->ww_OffX == 0 &&
	    demos->ww_Prop[HSCROLL].HorizBody == MAXBODY, "Clean Up fits");
	if (demos->ww_Prop[VSCROLL].VertBody < MAXBODY) {
		knob(demos, VSCROLL, &r);
		x = (r.MinX + r.MaxX) / 2;
		y = (r.MinY + r.MaxY) / 2;
		drag(x, y, x, y + 30);
		check(demos->ww_OffY > 0 && shown(demos), "scrolled down");
	}
}

/* Window and Icons: Snapshot, UnSnapshot, and the window reopened */
static void snapshot(struct WBWin *sys, struct WBWin *demos)
{
	static char path[] = "System:Demos";
	struct Window *w = demos->ww_Win;
	struct DiskObject *d;
	struct WBIcon *in = findicon(demos, "Hello");
	WORD width = w->Width, offy = demos->ww_OffY, x, y;
	LONG hx = in->in_X;

	pick(1, 5);
	d = GetDiskObject(path);
	check(d != NULL && d->do_DrawerData != NULL &&
	    d->do_DrawerData->dd_NewWindow.Width == width &&
	    d->do_DrawerData->dd_CurrentY == offy, "Window Snapshot");
	FreeDiskObject(d);
	d = GetDiskObject("System:Demos/Hello");
	check(d != NULL && d->do_CurrentX == hx, "icon Snapshot");
	FreeDiskObject(d);
	iconpos(demos, in, &x, &y);
	click(x, y);
	pick(2, 3);
	d = GetDiskObject("System:Demos/Hello");
	check(d != NULL && d->do_CurrentX == NO_ICON_POSITION, "UnSnapshot");
	FreeDiskObject(d);

	/* closed and opened again, as snapshot */
	pick(1, 1);
	check(findpath(path) == NULL, "Demos closed");
	iconpos(sys, findicon(sys, "Demos"), &x, &y);
	click(x, y);
	click(x, y);
	demos = waitwin(path);
	check(demos != NULL && demos->ww_Win->Width == width &&
	    demos->ww_OffY == offy && findicon(demos, "Hello") != NULL,
	    "Demos reopened");
}

/* Whether the path is there */
static BOOL exists(STRPTR path)
{
	BPTR lock = Lock(path, ACCESS_READ);

	UnLock(lock);
	return lock != NULL;
}

/*
 * The Ram Disk's window moved aside: an icon of Demos dropped in it is
 * copied to where it was let go, and Hello dropped on a drawer moves
 */
static void ramdisk(struct WBWin *demos, struct WBWin *ram)
{
	static char name[NAMELEN + 4];
	struct DiskObject *d = GetDefDiskObject(WBDRAWER);
	struct Window *w = ram->ww_Win;
	struct WBIcon *in, *sub = NULL;
	WORD x, y, x1, y1, i;

	x = w->LeftEdge + 40;
	y = w->TopEdge + 4;
	drag(x, y, x + 500, y + 300);
	check(w->LeftEdge > 500, "Ram Disk window moved");
	FOREACH(struct WBIcon *, in, &demos->ww_Icons)
		if (in->in_Y - demos->ww_OffY >= 0 &&
		    in->in_Y - demos->ww_OffY + in->in_Box.MaxY <
		    demos->ww_Win->GZZHeight && !streq(in->in_Name, "Hello"))
			break;
	check(in->in_Node.mln_Succ != NULL, "an icon to copy");
	if (in->in_Node.mln_Succ == NULL)
		return;
	strcpy(name, "RAM:");
	strcat(name, in->in_Name);
	iconpos(demos, in, &x, &y);
	x1 = w->LeftEdge + w->Width / 2;
	y1 = w->TopEdge + w->Height / 2;
	drag(x, y, x1, y1);
	in = findicon(ram, FilePart(name));
	if (in != NULL)
		iconpos(ram, in, &x, &y);
	check(exists(name) && in != NULL && x == x1 && y == y1,
	    "copied where dropped");

	/* a drawer in the Ram Disk, and Hello moved into it */
	/* PutDiskObject() tells Workbench */
	UnLock(CreateDir("RAM:Sub"));
	check(d != NULL && PutDiskObject("RAM:Sub", d), "RAM:Sub");
	FreeDiskObject(d);
	for (i = 0; i < 20 && (sub = findicon(ram, "Sub")) == NULL; i++)
		Delay(5);
	in = findicon(ram, "Hello");
	check(sub != NULL && in != NULL, "Sub shown");
	if (sub == NULL || in == NULL)
		return;
	iconpos(ram, in, &x, &y);
	iconpos(ram, sub, &x1, &y1);
	drag(x, y, x1, y1);
	check(findicon(ram, "Hello") == NULL && exists("RAM:Sub/Hello") &&
	    exists("RAM:Sub/Hello.info") && !exists("RAM:Hello"),
	    "Hello moved into Sub");
}

/* Hello dropped on the Ram Disk is copied with its icon */
static void copy(struct WBWin *demos)
{
	static struct FileInfoBlock fib;
	struct WBIcon *in = findicon(demos, "Hello"), *ram = findicon(root,
	    "Ram Disk:");
	struct WBWin *ww;
	LONG size[2];
	WORD i, x, y, rx, ry;
	BPTR lock;
	static char *names[] = { "System:Demos/Hello", "RAM:Hello" };

	check(in != NULL && ram != NULL, "Ram Disk");
	if (in == NULL || ram == NULL)
		return;
	iconpos(demos, in, &x, &y);
	iconpos(root, ram, &rx, &ry);
	drag(x, y, rx, ry);
	for (i = 0; i < 2; i++) {
		size[i] = -1;
		if ((lock = Lock(names[i], ACCESS_READ)) != NULL) {
			if (Examine(lock, &fib))
				size[i] = fib.fib_Size;
			UnLock(lock);
		}
	}
	lock = Lock("RAM:Hello.info", ACCESS_READ);
	check(size[0] > 0 && size[1] == size[0] && lock != NULL &&
	    findicon(demos, "Hello") != NULL, "Hello copied");
	UnLock(lock);
	click(rx, ry);
	click(rx, ry);
	ww = waitwin("Ram Disk:");
	check(ww != NULL && findicon(ww, "Hello") != NULL, "Ram Disk window");
	if (ww != NULL)
		ramdisk(demos, ww);
}

/* The Ram Disk moved left: its label goes, the backdrop shows again */
static void ramlabel(void)
{
	struct WBIcon *in = findicon(root, "Ram Disk:");
	struct Rectangle r;
	WORD x, y, x0;

	check(in != NULL && labelled(root, in), "Ram Disk label");
	if (in == NULL)
		return;
	x0 = in->in_X;
	screenbox(root, in, &r);
	iconpos(root, in, &x, &y);
	drag(x, y, x - 120, y);
	check(in->in_X == x0 - 120 && labelled(root, in) && backdrop(&r),
	    "Ram Disk moved");
}

static void script(void)
{
	struct WBWin *sys, *demos;
	struct WBIcon *in, *hello;
	struct MsgPort *port;
	struct Rectangle r;
	struct Task *t;
	WORD x, y, x0, n;

	in = findicon(root, "System:");
	check(in != NULL, "System disk icon");
	if (in == NULL)
		return;
	check(pixels(root, in, 0) && labelled(root, in), "disk icon pixels");
	ramlabel();
	iconpos(root, in, &x, &y);
	click(x, y);
	click(x, y);
	sys = waitwin("System:");
	check(sys != NULL && findicon(sys, "Demos") != NULL &&
	    findicon(sys, "Trashcan") != NULL && findicon(sys, "C") == NULL,
	    "System drawer");
	if (sys == NULL)
		return;
	utilities(sys);
	if ((in = findicon(sys, "Demos")) == NULL)
		return;
	check(in->in_Obj->do_Type == WBDRAWER && pixels(sys, in, 0) &&
	    labelled(sys, in),
	    "drawer icon pixels");
	iconpos(sys, in, &x, &y);
	click(x, y);
	click(x, y);
	demos = waitwin("System:Demos");
	hello = findicon(demos, "Hello");
	check(hello != NULL && findicon(demos, "Notes") != NULL &&
	    findicon(demos, "ReadMe") != NULL, "Demos drawer");
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

	/* Notes is a project: its default tool More opens it */
	in = findicon(demos, "Notes");
	iconpos(demos, in, &x, &y);
	click(x, y);
	click(x, y);
	t = waittask("More", FALSE);
	check(t != NULL, "Notes opened");
	if (t != NULL) {
		Delay(10);
		Signal(t, SIGBREAKF_CTRL_C);
		check(waittask("More", TRUE) == NULL, "More ended");
	}

	/* Window: Show All Files, then Clean Up */
	pick(1, 4);
	check(demos->ww_ShowAll && findicon(demos, "ReadMe") != NULL,
	    "Show All Files");
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

	if ((port = CreateMsgPort()) != NULL) {
		appicon(demos, port);
		appmenu(demos, port);
		DeleteMsgPort(port);
	}
	scrolling(demos);
	snapshot(sys, demos);
	if ((demos = waitwin("System:Demos")) != NULL)
		copy(demos);
	r.MinX = scr->Width / 2;
	r.MaxX = r.MinX + 40;
	r.MinY = scr->Height - 40;
	r.MaxY = r.MinY + 20;
	check(backdrop(&r), "backdrop pattern");

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
