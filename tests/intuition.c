/*
 * intuition.library from LIBS:, on screens in RAM bitmaps.  The root
 * program opens screens and
 * windows, checks the pixels of the title bars, borders and gadgets,
 * and drives the input handler with events written to input.device:
 * activation, IDCMP messages, dragging, sizing and depth arrangement,
 * gadgets, menus and an EasyRequest answered by a click.  It reports
 * the stack the input task and the menu task used.
 */
/* EMUKEYS: 3000000:1c,6000000:9c */
/* HWMARK: intuition: press Return */
#include <exec/exec.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxdriver.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <intuition/preferences.h>
#include <intuition/iprefs.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/alib.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static int fails;

static void check(int ok, char *what)
{
	if (!ok) {
		xKPrintf("FAIL intuition: %s\n", what);
		fails++;
	}
}

/* Events through input.device, which runs intuition's handler */
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

/*
 * Calls like MoveWindow() and ScreenToBack() leave the work to the input
 * handler, as on the Amiga: give it an event and a moment.
 */
static void sync(void)
{
	event(IECLASS_NULL, 0, 0, 0, 0);
	TimeDelay(UNIT_VBLANK, 0, 40000);
}

/* The pointer to (x, y) of the display, then a button */
static void mouse(WORD x, WORD y)
{
	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, 0, x, y);
}

static void button(UWORD code, WORD x, WORD y)
{
	event(IECLASS_RAWMOUSE, code, 0, x, y);
}

/* The pens of the screen's DrawInfo */
static UWORD pen(struct Screen *s, int which)
{
	struct DrawInfo *dri = GetScreenDrawInfo(s);
	UWORD p = dri->dri_Pens[which];

	FreeScreenDrawInfo(s, dri);
	return p;
}

/* The task's stack: the pool clears a new stack; count above the last zero */
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
	xKPrintf("intuition: %s stack %d of %d bytes used\n", name,
	    (int)(top - p), (int)(top - (UBYTE *)t->tc_SPLower));
}

static struct TagItem scrtags[] = {
	{ SA_Width, 640 }, { SA_Height, 200 }, { SA_Depth, 2 },
	{ SA_Title, (IPTR)"Test Screen" }, { SA_PubName, (IPTR)"TEST" },
	{ TAG_DONE, 0 }
};

static struct Screen *scr;

static void dbuf(struct Screen *s);

static void screens(void)
{
	struct Screen *s2, *ps;
	struct Window *w;
	struct Gadget *g;
	int x, y, n;
	static struct TagItem s1wtags[] = {
		{ WA_CustomScreen, 0 }, { WA_Title, (IPTR)"Mono" },
		{ WA_Width, 200 }, { WA_Height, 60 }, { WA_CloseGadget, TRUE },
		{ WA_DragBar, TRUE }, { WA_Activate, TRUE }, { TAG_DONE, 0 }
	};
	static struct TagItem s2tags[] = {
		{ SA_Width, 320 }, { SA_Height, 100 }, { SA_Depth, 1 },
		{ SA_Title, (IPTR)"Second" }, { TAG_DONE, 0 }
	};

	scr = OpenScreenTagList(NULL, scrtags);
	check(scr != NULL, "OpenScreenTagList");
	if (scr == NULL)
		return;
	check(scr->Width == 640 && scr->Height == 200 &&
	    GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH) == 2,
	    "screen size");
	check(IntuitionBase->FirstScreen == scr, "screen in front");
	check(scr->BarHeight > 8 && scr->BarHeight < 20, "BarHeight");

	/* the title bar: BARBLOCKPEN, a BARTRIMPEN line under it, the title */
	check(ReadPixel(&scr->RastPort, 400, 1) == pen(scr, BARBLOCKPEN),
	    "title bar fill");
	check(ReadPixel(&scr->RastPort, 300, scr->BarHeight) ==
	    pen(scr, BARTRIMPEN), "title bar trim");
	n = 0;
	for (x = 0; x < 120; x++)
		if (ReadPixel(&scr->RastPort, x, scr->BarHeight / 2) ==
		    pen(scr, BARDETAILPEN))
			n++;
	check(n > 10, "title text");
	check(ReadPixel(&scr->RastPort, 300, 100) == pen(scr, BACKGROUNDPEN),
	    "screen background");
	check(scr->FirstGadget != NULL &&
	    (scr->FirstGadget->GadgetType & GTYP_SYSTYPEMASK) == GTYP_SDEPTH,
	    "screen depth gadget");

	/* public screens */
	check(LockPubScreen("TEST") == NULL, "private screen");
	check(PubScreenStatus(scr, 0) & PSNF_PRIVATE, "PubScreenStatus");
	ps = LockPubScreen("TEST");
	check(ps == scr, "LockPubScreen");
	if (ps)
		UnlockPubScreen(NULL, ps);

	/* a second screen comes in front, then goes behind */
	s2 = OpenScreenTagList(NULL, s2tags);
	check(s2 != NULL && IntuitionBase->FirstScreen == s2,
	    "second screen in front");
	if (s2) {
		/* one bitplane: the active window's gadgets show on FILLPEN */
		s1wtags[0].ti_Data = (IPTR)s2;
		w = OpenWindowTagList(NULL, s1wtags);
		check(w != NULL && (w->Flags & WFLG_WINDOWACTIVE), "1-plane window");
		if (w) {
			g = w->FirstGadget;
			while (g && (g->GadgetType & GTYP_SYSTYPEMASK) != GTYP_CLOSE)
				g = g->NextGadget;
			n = 0;
			for (y = 1; g && y < g->Height - 1; y++)
				for (x = 1; x < g->Width - 1; x++)
					if (ReadPixel(w->RPort, g->LeftEdge + x,
					    g->TopEdge + y) != pen(s2, FILLPEN))
						n++;
			check(n > 0, "1-plane close gadget");
			CloseWindow(w);
		}
		dbuf(s2);
		ScreenToBack(s2);
		sync();
		check(IntuitionBase->FirstScreen == scr, "ScreenToBack");
		ScreenToFront(s2);
		sync();
		check(IntuitionBase->FirstScreen == s2, "ScreenToFront");
		check(CloseScreen(s2), "CloseScreen");
		check(IntuitionBase->FirstScreen == scr, "screen closed");
	}

	/* the input handler moves the pointer */
	mouse(100, 50);
	check(IntuitionBase->MouseX == 100 && IntuitionBase->MouseY == 50,
	    "pointer from input.device");
	check(scr->MouseX == 100 && scr->MouseY == 50, "screen pointer");
}


/* The next IDCMP message of a window: its class, code and mouse position */
static ULONG msgclass;
static UWORD msgcode;
static WORD msgx, msgy;

static BOOL getmsg(struct Window *w)
{
	struct IntuiMessage *im;

	im = (struct IntuiMessage *)GetMsg(w->UserPort);
	if (im == NULL)
		return FALSE;
	msgclass = im->Class;
	msgcode = im->Code;
	msgx = im->MouseX;
	msgy = im->MouseY;
	ReplyMsg((struct Message *)im);
	return TRUE;
}

/* Wait for a message of a class, dropping others; FALSE if none comes */
static BOOL waitmsg(struct Window *w, ULONG class)
{
	while (getmsg(w))
		if (msgclass == class)
			return TRUE;
	return FALSE;
}

static void flush(struct Window *w)
{
	while (getmsg(w))
		;
}

/* A system gadget of a window and the screen position of its centre */
static struct Gadget *sysgad(struct Window *w, UWORD type, WORD *x, WORD *y)
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
		return g;
	}
	return NULL;
}

static void click(WORD x, WORD y)
{
	mouse(x, y);
	button(IECODE_LBUTTON, x, y);
	button(IECODE_LBUTTON | IECODE_UP_PREFIX, x, y);
}

#define WIDCMP (IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | \
	IDCMP_RAWKEY | IDCMP_VANILLAKEY | IDCMP_GADGETUP | \
	IDCMP_REFRESHWINDOW | IDCMP_NEWSIZE | IDCMP_ACTIVEWINDOW | \
	IDCMP_INACTIVEWINDOW | IDCMP_CHANGEWINDOW | IDCMP_MENUPICK)

static struct Window *openwin(WORD x, WORD y, WORD w, WORD h, char *title,
    ULONG refresh)
{
	struct TagItem t[19];

	t[0].ti_Tag = WA_Left;		t[0].ti_Data = x;
	t[1].ti_Tag = WA_Top;		t[1].ti_Data = y;
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = w;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = h;
	t[4].ti_Tag = WA_Title;		t[4].ti_Data = (IPTR)title;
	t[5].ti_Tag = WA_CustomScreen;	t[5].ti_Data = (IPTR)scr;
	t[6].ti_Tag = WA_IDCMP;		t[6].ti_Data = WIDCMP;
	t[7].ti_Tag = WA_CloseGadget;	t[7].ti_Data = TRUE;
	t[8].ti_Tag = WA_DepthGadget;	t[8].ti_Data = TRUE;
	t[9].ti_Tag = WA_SizeGadget;	t[9].ti_Data = TRUE;
	t[10].ti_Tag = WA_DragBar;	t[10].ti_Data = TRUE;
	t[11].ti_Tag = WA_Activate;	t[11].ti_Data = TRUE;
	t[12].ti_Tag = refresh;		t[12].ti_Data = TRUE;
	t[13].ti_Tag = WA_ReportMouse;	t[13].ti_Data = TRUE;
	t[14].ti_Tag = WA_MinWidth;	t[14].ti_Data = 60;
	t[15].ti_Tag = WA_MinHeight;	t[15].ti_Data = 30;
	t[16].ti_Tag = WA_MaxWidth;	t[16].ti_Data = ~0;
	t[17].ti_Tag = WA_MaxHeight;	t[17].ti_Data = ~0;
	t[18].ti_Tag = TAG_DONE;
	return OpenWindowTagList(NULL, t);
}

static void windows(void)
{
	struct Window *w1, *w2;
	struct Gadget *g;
	WORD x, y, ox, oy, ow;

	w1 = openwin(20, 30, 200, 100, "One", WA_SimpleRefresh);
	check(w1 != NULL, "OpenWindowTagList");
	if (w1 == NULL)
		return;
	check(IntuitionBase->ActiveWindow == w1 &&
	    (w1->Flags & WFLG_WINDOWACTIVE), "active window");
	check(w1->BorderTop > 8 && w1->BorderLeft > 0 && w1->BorderRight > 0 &&
	    w1->BorderBottom > 0, "borders");
	check(w1->UserPort != NULL, "IDCMP port");

	/* the frame: SHINEPEN on the left and top, SHADOWPEN on the right
	   and bottom, the title bar in FILLPEN */
	check(ReadPixel(w1->RPort, 0, w1->Height / 2) == pen(scr, SHINEPEN),
	    "left border");
	check(ReadPixel(w1->RPort, w1->Width - 1, w1->Height / 2) ==
	    pen(scr, SHADOWPEN), "right border");
	check(ReadPixel(w1->RPort, w1->Width / 2, w1->Height - 1) ==
	    pen(scr, SHADOWPEN), "bottom border");
	check(ReadPixel(w1->RPort, w1->Width / 2 + 40, 2) == pen(scr, FILLPEN),
	    "title bar");
	check(sysgad(w1, GTYP_CLOSE, &x, &y) != NULL, "close gadget");
	check(sysgad(w1, GTYP_WDEPTH, &x, &y) != NULL, "depth gadget");
	check(sysgad(w1, GTYP_SIZING, &x, &y) != NULL, "size gadget");
	check(sysgad(w1, GTYP_WDRAGGING, &x, &y) != NULL, "drag bar");
	flush(w1);

	/* mouse, keys */
	click(w1->LeftEdge + 50, w1->TopEdge + 40);
	check(waitmsg(w1, IDCMP_MOUSEBUTTONS) && msgcode == IECODE_LBUTTON &&
	    msgx == 50 && msgy == 40, "IDCMP_MOUSEBUTTONS");
	mouse(w1->LeftEdge + 60, w1->TopEdge + 45);
	check(waitmsg(w1, IDCMP_MOUSEMOVE) && msgx == 60 && msgy == 45,
	    "IDCMP_MOUSEMOVE");
	event(IECLASS_RAWKEY, RAWKEY_A, 0, 0, 0);
	event(IECLASS_RAWKEY, RAWKEY_A | IECODE_UP_PREFIX, 0, 0, 0);
	check(waitmsg(w1, IDCMP_VANILLAKEY) && msgcode == 'a',
	    "IDCMP_VANILLAKEY");
	event(IECLASS_RAWKEY, RAWKEY_UP, 0, 0, 0);
	check(waitmsg(w1, IDCMP_RAWKEY) && msgcode == RAWKEY_UP,
	    "IDCMP_RAWKEY");
	event(IECLASS_RAWKEY, RAWKEY_UP | IECODE_UP_PREFIX, 0, 0, 0);
	flush(w1);

	/* dragging: an outline until the button goes up */
	sysgad(w1, GTYP_WDRAGGING, &x, &y);
	ox = w1->LeftEdge;
	oy = w1->TopEdge;
	mouse(x, y);
	button(IECODE_LBUTTON, x, y);
	mouse(x + 30, y + 20);
	check(w1->LeftEdge == ox, "outline drag");
	button(IECODE_LBUTTON | IECODE_UP_PREFIX, x + 30, y + 20);
	sync();
	check(w1->LeftEdge == ox + 30 && w1->TopEdge == oy + 20, "drag");
	check(waitmsg(w1, IDCMP_CHANGEWINDOW), "IDCMP_CHANGEWINDOW");

	/* sizing */
	sysgad(w1, GTYP_SIZING, &x, &y);
	ow = w1->Width;
	mouse(x, y);
	button(IECODE_LBUTTON, x, y);
	mouse(x + 40, y + 10);
	button(IECODE_LBUTTON | IECODE_UP_PREFIX, x + 40, y + 10);
	sync();
	check(w1->Width == ow + 40, "size");
	check(waitmsg(w1, IDCMP_NEWSIZE), "IDCMP_NEWSIZE");
	flush(w1);

	/* a second window in front; depth arrangement; refresh */
	w2 = openwin(100, 60, 200, 100, "Two", WA_SmartRefresh);
	check(w2 != NULL && IntuitionBase->ActiveWindow == w2, "second window");
	if (w2) {
		check(waitmsg(w1, IDCMP_INACTIVEWINDOW), "IDCMP_INACTIVEWINDOW");
		check(w2->WLayer->front == NULL, "second window in front");
		check(ReadPixel(w1->RPort, w1->Width / 2 + 40, 2) ==
		    pen(scr, BACKGROUNDPEN) && ReadPixel(w2->RPort,
		    w2->Width / 2 + 40, 2) == pen(scr, FILLPEN),
		    "inactive and active title bars");
		sysgad(w2, GTYP_WDEPTH, &x, &y);
		click(x, y);
		sync();
		check(w1->WLayer->front == NULL, "depth gadget");
		check(waitmsg(w1, IDCMP_REFRESHWINDOW), "IDCMP_REFRESHWINDOW");
		BeginRefresh(w1);
		EndRefresh(w1, TRUE);
		WindowToFront(w2);
		sync();
		check(w2->WLayer->front == NULL, "WindowToFront");
		WindowToBack(w2);
		sync();
		check(w1->WLayer->front == NULL, "WindowToBack");
		ActivateWindow(w2);
		sync();
		check(IntuitionBase->ActiveWindow == w2, "ActivateWindow");
		MoveWindow(w2, -10, 5);
		sync();
		check(w2->LeftEdge == 90 && w2->TopEdge == 65, "MoveWindow");
		SizeWindow(w2, 20, -10);
		sync();
		check(w2->Width == 220 && w2->Height == 90, "SizeWindow");
		ChangeWindowBox(w2, 300, 40, 150, 80);
		sync();
		check(w2->LeftEdge == 300 && w2->TopEdge == 40 &&
		    w2->Width == 150 && w2->Height == 80, "ChangeWindowBox");
		CloseWindow(w2);
	}

	/* the close gadget */
	flush(w1);
	g = sysgad(w1, GTYP_CLOSE, &x, &y);
	click(x, y);
	check(waitmsg(w1, IDCMP_CLOSEWINDOW), "IDCMP_CLOSEWINDOW");
	CloseWindow(w1);
	check(scr->FirstWindow == NULL, "CloseWindow");
}

/* A message that another task (the menu task) sends: wait up to 1 s */
static BOOL waitlate(struct Window *w, ULONG class)
{
	int i;

	for (i = 0; i < 50; i++) {
		if (waitmsg(w, class))
			return TRUE;
		TimeDelay(UNIT_VBLANK, 0, 20000);
	}
	return FALSE;
}

static UBYTE strbuf[32], undobuf[32];
static struct StringInfo strinfo = { strbuf, undobuf, 0, 32 };
static struct Gadget strgad = {
	NULL, 20, 60, 100, 10, GFLG_GADGHCOMP, GACT_RELVERIFY,
	GTYP_STRGADGET, NULL, NULL, NULL, 0, (APTR)&strinfo, 3
};
static struct Image knob;
static struct PropInfo propinfo = { AUTOKNOB | FREEHORIZ | PROPNEWLOOK,
	0, 0, MAXBODY / 4, MAXBODY };
static struct Gadget propgad = {
	&strgad, 20, 40, 120, 10, GFLG_GADGHCOMP, GACT_RELVERIFY,
	GTYP_PROPGADGET, (APTR)&knob, NULL, NULL, 0, (APTR)&propinfo, 2
};
static struct IntuiText booltext = { 1, 0, JAM1, 4, 2, NULL, "OK", NULL };
static struct Gadget boolgad = {
	&propgad, 20, 20, 40, 12, GFLG_GADGHCOMP, GACT_RELVERIFY,
	GTYP_BOOLGADGET, NULL, NULL, &booltext, 0, NULL, 1
};

static void gadgets(void)
{
	struct Window *w;
	struct Gadget *b;
	struct IntuiMessage *im;
	struct TagItem bt[6];
	UWORD id = 0;

	w = openwin(40, 40, 300, 120, "Gadgets", WA_SmartRefresh);
	check(w != NULL, "gadget window");
	if (w == NULL)
		return;
	strbuf[0] = 0;
	strinfo.BufferPos = strinfo.NumChars = 0;
	check(AddGList(w, &boolgad, ~0, 3, NULL) != (UWORD)~0, "AddGList");
	RefreshGList(&boolgad, w, NULL, 3);
	flush(w);

	/* boolean */
	click(w->LeftEdge + 30, w->TopEdge + 25);
	if (waitmsg(w, IDCMP_GADGETUP))
		id = 1;
	check(id == 1, "boolean gadget");
	OffGadget(&boolgad, w, NULL);
	check(boolgad.Flags & GFLG_DISABLED, "OffGadget");
	click(w->LeftEdge + 30, w->TopEdge + 25);
	check(!waitmsg(w, IDCMP_GADGETUP), "disabled gadget");
	OnGadget(&boolgad, w, NULL);

	/* proportional: a click right of the knob pages right */
	NewModifyProp(&propgad, w, NULL, propinfo.Flags, 0, 0,
	    MAXBODY / 4, MAXBODY, 1);
	check(propinfo.HorizPot == 0, "NewModifyProp");
	click(w->LeftEdge + 130, w->TopEdge + 45);
	check(propinfo.HorizPot > 0, "prop gadget");
	flush(w);

	/* string */
	check(ActivateGadget(&strgad, w, NULL), "ActivateGadget");
	event(IECLASS_RAWKEY, RAWKEY_H, 0, 0, 0);
	event(IECLASS_RAWKEY, RAWKEY_H | IECODE_UP_PREFIX, 0, 0, 0);
	event(IECLASS_RAWKEY, RAWKEY_I, 0, 0, 0);
	event(IECLASS_RAWKEY, RAWKEY_I | IECODE_UP_PREFIX, 0, 0, 0);
	event(IECLASS_RAWKEY, RAWKEY_RETURN, 0, 0, 0);
	event(IECLASS_RAWKEY, RAWKEY_RETURN | IECODE_UP_PREFIX, 0, 0, 0);
	check(strbuf[0] == 'h' && strbuf[1] == 'i' && strbuf[2] == 0,
	    "string gadget");
	check(waitmsg(w, IDCMP_GADGETUP), "string GADGETUP");

	/* BOOPSI button */
	bt[0].ti_Tag = GA_Left;		bt[0].ti_Data = 200;
	bt[1].ti_Tag = GA_Top;		bt[1].ti_Data = 20;
	bt[2].ti_Tag = GA_Width;	bt[2].ti_Data = 40;
	bt[3].ti_Tag = GA_Height;	bt[3].ti_Data = 12;
	bt[4].ti_Tag = GA_RelVerify;	bt[4].ti_Data = TRUE;
	bt[5].ti_Tag = TAG_DONE;
	b = (struct Gadget *)NewObjectA(NULL, BUTTONGCLASS, bt);
	check(b != NULL, "NewObject buttongclass");
	if (b) {
		b->GadgetID = 9;
		AddGadget(w, b, ~0);
		RefreshGList(b, w, NULL, 1);
		flush(w);
		click(w->LeftEdge + 210, w->TopEdge + 25);
		id = 0;
		while ((im = (struct IntuiMessage *)GetMsg(w->UserPort))) {
			if (im->Class == IDCMP_GADGETUP)
				id = ((struct Gadget *)im->IAddress)->GadgetID;
			ReplyMsg((struct Message *)im);
		}
		check(id == 9, "BOOPSI gadget");
		RemoveGadget(w, b);
		DisposeObject(b);
	}
	RemoveGList(w, &boolgad, 3);
	CloseWindow(w);
}

static struct IntuiText it1 = { 0, 1, JAM2, 2, 1, NULL, "Open", NULL };
static struct IntuiText it2 = { 0, 1, JAM2, 2, 1, NULL, "Quit", NULL };
static struct MenuItem item2 = { NULL, 0, 10, 80, 10,
	ITEMTEXT | ITEMENABLED | HIGHCOMP, 0, (APTR)&it2 };
static struct MenuItem item1 = { &item2, 0, 0, 80, 10,
	ITEMTEXT | ITEMENABLED | HIGHCOMP, 0, (APTR)&it1 };
static struct Menu menu = { NULL, 0, 0, 60, 10, MENUENABLED, "Project",
	&item1 };

static void menus(void)
{
	struct Window *w;
	WORD y;

	w = openwin(10, 20, 300, 120, "Menus", WA_SmartRefresh);
	check(w != NULL, "menu window");
	if (w == NULL)
		return;
	check(SetMenuStrip(w, &menu), "SetMenuStrip");
	flush(w);
	/* press on the menu title, go to the second item, release */
	mouse(menu.LeftEdge + 10, 2);
	button(IECODE_RBUTTON, menu.LeftEdge + 10, 2);
	y = scr->BarHeight + 1 + item2.TopEdge + 5;
	mouse(menu.LeftEdge + 10, y + 1);
	mouse(menu.LeftEdge + 12, y + 2);
	TimeDelay(UNIT_VBLANK, 0, 100000);
	button(IECODE_RBUTTON | IECODE_UP_PREFIX, menu.LeftEdge + 12, y + 2);
	check(waitlate(w, IDCMP_MENUPICK) && MENUNUM(msgcode) == 0 &&
	    ITEMNUM(msgcode) == 1, "IDCMP_MENUPICK");
	ClearMenuStrip(w);
	CloseWindow(w);
}

static int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return *a - *b;
}

/* The EasyRequest's helper: clicks the first button of the requester */
static struct Task *helper;
static volatile BOOL helped;

static void helpertask(void)
{
	struct MsgPort *p = CreateMsgPort();
	struct IOStdReq *r = (struct IOStdReq *)CreateIORequest(p, sizeof(*r));
	struct InputEvent e;
	struct Window *w;
	struct Gadget *g;
	int i, k;
	WORD x, y;

	if (r == NULL || OpenDevice("input.device", 0, (struct IORequest *)r, 0)) {
		Wait(0);
	}
	for (i = 0; i < 100 && !helped; i++) {
		TimeDelay(UNIT_VBLANK, 0, 20000);
		w = IntuitionBase->ActiveWindow;
		if (w == NULL || w->Title == NULL || strcmp(w->Title, "Request") != 0)
			continue;
		for (g = w->FirstGadget; g; g = g->NextGadget)
			if (g->GadgetID == 1)
				break;
		if (g == NULL)
			continue;
		x = w->LeftEdge + g->LeftEdge + g->Width / 2;
		y = w->TopEdge + g->TopEdge + g->Height / 2;
		for (k = 0; k < 3; k++) {
			e.ie_NextEvent = NULL;
			e.ie_Class = IECLASS_RAWMOUSE;
			e.ie_SubClass = 0;
			e.ie_Code = k == 0 ? IECODE_NOBUTTON : k == 1 ?
			    IECODE_LBUTTON : IECODE_LBUTTON | IECODE_UP_PREFIX;
			e.ie_Qualifier = 0;
			e.ie_X = x;
			e.ie_Y = y;
			r->io_Command = IND_WRITEEVENT;
			r->io_Data = &e;
			r->io_Length = sizeof(e);
			DoIO((struct IORequest *)r);
		}
		helped = TRUE;
	}
	CloseDevice((struct IORequest *)r);
	Wait(0);
}

/* An EasyRequest on the screen of a window of scr */
static void requesters(void)
{
	static struct EasyStruct es = { sizeof(struct EasyStruct), 0,
		"Request", "Is %s working?", "Yes|No" };
	static char *args[] = { "intuition" };
	struct Window *w;
	LONG r;

	w = openwin(10, 20, 200, 60, "Parent", WA_SmartRefresh);
	check(w != NULL, "requester parent window");
	helped = FALSE;
	helper = CreateTask("requester helper", 1, (TASKPROC)helpertask, 1536);
	check(helper != NULL, "helper task");
	if (helper == NULL || w == NULL)
		return;
	r = EasyRequestArgs(w, &es, NULL, (RAWARG)args);
	check(helped && r == 1, "EasyRequestArgs");
	RemTask(helper);
	CloseWindow(w);
}

/*
 * DisplayAlert: a helper waits for the alert's text on the frontmost
 * screen, checks its frame and background, then answers with alertcode,
 * if any.
 * The alert is 40 rows: the frame is rows and columns 4-7 in, the first
 * line's baseline at (40, 20).
 */
static UBYTE alerttext[] = "\000(\024Test alert\000\001\000(\036Line two\000";
static UWORD alertcode;
static volatile BOOL alertdrawn;

static void alerttask(void)
{
	struct MsgPort *p = CreateMsgPort();
	struct IOStdReq *r = (struct IOStdReq *)CreateIORequest(p, sizeof(*r));
	struct InputEvent e;
	struct Screen *s;
	struct RastPort rp;
	ULONG f, b;
	int i, k, n, x, y;

	if (r == NULL || OpenDevice("input.device", 0, (struct IORequest *)r, 0))
		Wait(0);
	for (i = 0; i < 100; i++) {
		TimeDelay(UNIT_VBLANK, 0, 20000);
		if ((s = IntuitionBase->FirstScreen) == NULL)
			continue;
		InitRastPort(&rp);
		rp.BitMap = s->ViewPort.RasInfo->BitMap;
		f = ReadPixel(&rp, 5, 20);
		b = ReadPixel(&rp, 20, 20);
		n = 0;
		for (y = 12; f != b && y <= 21; y++)
			for (x = 40; x < 120; x++)
				n += ReadPixel(&rp, x, y) == f;
		if (n < 20)
			continue;
		alertdrawn = ReadPixel(&rp, 300, 5) == f &&
		    ReadPixel(&rp, 300, 34) == f &&
		    ReadPixel(&rp, s->Width - 6, 20) == f &&
		    ReadPixel(&rp, 2, 20) == b && ReadPixel(&rp, 300, 38) == b &&
		    n < 400;
		break;
	}
	for (k = 0; alertcode && k < 2; k++) {
		e.ie_NextEvent = NULL;
		e.ie_Class = alertcode == IECODE_LBUTTON ||
		    alertcode == IECODE_RBUTTON ? IECLASS_RAWMOUSE : IECLASS_RAWKEY;
		e.ie_SubClass = 0;
		e.ie_Code = k ? alertcode | IECODE_UP_PREFIX : alertcode;
		e.ie_Qualifier = 0;
		e.ie_X = e.ie_Y = 0;
		r->io_Command = IND_WRITEEVENT;
		r->io_Data = &e;
		r->io_Length = sizeof(e);
		DoIO((struct IORequest *)r);
	}
	CloseDevice((struct IORequest *)r);
	Wait(0);
}

/* A sum over the alert's rows of the frontmost screen */
static ULONG alertrows(void)
{
	struct Screen *s = IntuitionBase->FirstScreen;
	ULONG sum = 0;
	int x, y;

	for (y = 0; s && y < 40; y += 3)
		for (x = 0; x < s->Width; x += 7)
			sum = sum * 3 + ReadPixel(&s->RastPort, x, y);
	return sum;
}

static void alert(ULONG num, UWORD code, BOOL want, char *what)
{
	struct Task *t;
	ULONG before = alertrows();
	BOOL r;

	alertcode = code;
	alertdrawn = FALSE;
	t = CreateTask("alert helper", 1, (TASKPROC)alerttask, 1536);
	check(t != NULL, "alert helper task");
	if (t == NULL)
		return;
	r = DisplayAlert(num, alerttext, 40);
	check(alertdrawn && r == want, what);
	check(alertrows() == before, "alert restores the screen");
	RemTask(t);
}

static void alerts(void)
{
	ULONG before = alertrows();

	alert(DEADEND_ALERT, IECODE_LBUTTON, FALSE, "dead-end alert");
	alert(RECOVERY_ALERT, IECODE_LBUTTON, TRUE, "alert left button");
	alert(RECOVERY_ALERT, RAWKEY_ESCAPE, FALSE, "alert Escape");
	check(!TimedDisplayAlert(RECOVERY_ALERT, alerttext, 40, 10) &&
	    alertrows() == before, "TimedDisplayAlert");
}

/*
 * An alert raised by an input handler, on input.device's task, where it
 * polls the keyboard and buttons: a NULL event with code ALERTEVENT raises
 * it, waiting inputtime frames.
 */
#define ALERTEVENT	0xA1E
static ULONG inputtime;
static volatile BOOL inputret, inputdone;

static struct InputEvent *raiser(struct InputEvent *ie, APTR data)
{
	struct InputEvent *e;

	for (e = ie; e; e = e->ie_NextEvent)
		if (e->ie_Class == IECLASS_NULL && e->ie_Code == ALERTEVENT) {
			inputret = TimedDisplayAlert(RECOVERY_ALERT, alerttext, 40,
			    inputtime);
			inputdone = TRUE;
		}
	return ie;
}

static void inputalert(ULONG time, char *mark, BOOL want, char *what)
{
	struct Task *t;
	ULONG before = alertrows();

	alertcode = 0;
	alertdrawn = inputdone = FALSE;
	inputret = !want;
	inputtime = time;
	t = CreateTask("alert helper", 1, (TASKPROC)alerttask, 1536);
	check(t != NULL, "alert helper task");
	if (t == NULL)
		return;
	xKPrintf("intuition: %s\n", mark);
	event(IECLASS_NULL, ALERTEVENT, 0, 0, 0);
	check(inputdone && alertdrawn && inputret == want, what);
	check(alertrows() == before, "input task alert restores the screen");
	RemTask(t);
}

/* The second alert is answered by Return from the keyboard */
static void inputalerts(void)
{
	struct Interrupt h;

	h.is_Node.ln_Type = NT_INTERRUPT;
	h.is_Node.ln_Pri = 60;
	h.is_Node.ln_Name = "alert raiser";
	h.is_Code = (int (*)())raiser;
	h.is_Data = NULL;
	io->io_Command = IND_ADDHANDLER;
	io->io_Data = &h;
	DoIO((struct IORequest *)io);
	inputalert(25, "input task alert", FALSE, "input task alert timeout");
	inputalert(1000, "press Return", TRUE, "input task alert Return");
	io->io_Command = IND_REMHANDLER;
	io->io_Data = &h;
	DoIO((struct IORequest *)io);
}

/*
 * Double buffering on s: the screen's own bitmap, a copy of it and a
 * cleared one; ChangeScreenBuffer() swaps the screen's bitmap and replies
 * shown, then safe.
 */
static void dbuf(struct Screen *s)
{
	struct ScreenBuffer *sb0, *sb1, *sb2;
	struct BitMap *own = s->RastPort.BitMap;
	struct MsgPort *mp;
	struct RastPort rp;
	struct Message *m1, *m2;
	ULONG title = ReadPixel(&s->RastPort, 200, 1);

	mp = CreateMsgPort();
	sb0 = AllocScreenBuffer(s, NULL, SB_SCREEN_BITMAP);
	sb1 = AllocScreenBuffer(s, NULL, SB_COPY_BITMAP);
	sb2 = AllocScreenBuffer(s, NULL, 0);
	check(mp && sb0 && sb1 && sb2 && sb0->sb_BitMap == own &&
	    sb1->sb_BitMap != own && sb2->sb_BitMap != own &&
	    sb1->sb_DBufInfo && sb2->sb_DBufInfo, "AllocScreenBuffer");
	if (!mp || !sb0 || !sb1 || !sb2)
		return;
	InitRastPort(&rp);
	rp.BitMap = sb1->sb_BitMap;
	check(ReadPixel(&rp, 200, 1) == title && ReadPixel(&rp, 150, 80) ==
	    ReadPixel(&s->RastPort, 150, 80), "SB_COPY_BITMAP");
	SetAPen(&rp, 3);
	RectFill(&rp, 10, 50, 30, 60);
	rp.BitMap = sb2->sb_BitMap;
	check(ReadPixel(&rp, 200, 1) == 0 && ReadPixel(&rp, 20, 55) == 0,
	    "cleared screen buffer");

	sb1->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = mp;
	sb1->sb_DBufInfo->dbi_DispMessage.mn_ReplyPort = mp;
	check(ChangeScreenBuffer(s, sb1) && s->RastPort.BitMap == sb1->sb_BitMap &&
	    s->ViewPort.RasInfo->BitMap == sb1->sb_BitMap &&
	    ReadPixel(&s->RastPort, 20, 55) == (3 & ((1 << s->RastPort.BitMap->Depth) - 1)),
	    "ChangeScreenBuffer");
	m1 = GetMsg(mp);
	m2 = GetMsg(mp);
	check(m1 == &sb1->sb_DBufInfo->dbi_DispMessage &&
	    m2 == &sb1->sb_DBufInfo->dbi_SafeMessage && GetMsg(mp) == NULL,
	    "DBufInfo messages");
	check(ChangeScreenBuffer(s, sb0) && s->RastPort.BitMap == own &&
	    GetMsg(mp) == NULL, "ChangeScreenBuffer back");
	ChangeScreenBuffer(s, sb2);
	FreeScreenBuffer(s, sb2);
	check(s->RastPort.BitMap == own && s->ViewPort.RasInfo->BitMap == own,
	    "FreeScreenBuffer of the one shown");
	FreeScreenBuffer(s, sb1);
	FreeScreenBuffer(s, sb0);
	check(ReadPixel(&s->RastPort, 200, 1) == title, "screen bitmap kept");
	DeleteMsgPort(mp);
}

/*
 * SetIPrefs: pens and palette as IPrefs gives them, then the Workbench
 * screen's size; QueryOverscan without a display.  The pens start with
 * intuition's DETAILPEN and BLOCKPEN, which a screen takes as its own.
 */
static void iprefs(void)
{
	static struct IOldPenPrefs pens = { 1, 0, 0, { 1, 0, 3, 1, (UWORD)~0 } };
	static struct ColorSpec colours[] = { { 0, 0x1111, 0x2222, 0x3333 }, { -1 } };
	static struct TagItem t[] = {
		{ SA_Width, 320 }, { SA_Height, 200 }, { SA_Depth, 2 }, { TAG_DONE, 0 }
	};
	struct Preferences prefs;
	struct Rectangle r;
	struct Screen *s;
	UWORD *dp, keep[NUMDRIPENS];
	int i;

	s = OpenScreenTagList(NULL, t);
	check(s != NULL, "screen for IPrefs pens");
	if (s == NULL)
		return;
	dp = GetScreenDrawInfo(s)->dri_Pens;
	for (i = 0; i < NUMDRIPENS; i++)
		keep[i] = dp[i];
	CloseScreen(s);
	check(SetIPrefs(&pens, sizeof(pens), IPREFS_TYPE_PENS_V39), "SetIPrefs pens");
	s = OpenScreenTagList(NULL, t);
	dp = GetScreenDrawInfo(s)->dri_Pens;
	check(dp[TEXTPEN] == 3 && dp[SHINEPEN] == 1 &&
	    dp[SHADOWPEN] == keep[SHADOWPEN], "screen with IPrefs pens");
	CloseScreen(s);
	pens.PenTable[TEXTPEN] = keep[TEXTPEN];
	pens.PenTable[SHINEPEN] = keep[SHINEPEN];
	SetIPrefs(&pens, sizeof(pens), IPREFS_TYPE_PENS_V39);

	GetPrefs(&prefs, sizeof(prefs));
	i = prefs.color0;
	check(SetIPrefs(colours, sizeof(colours), IPREFS_TYPE_PALETTE_V39) &&
	    GetPrefs(&prefs, sizeof(prefs)) && prefs.color0 == 0x123,
	    "SetIPrefs palette");
	colours[0].Red = (i >> 8 & 15) * 0x1111;
	colours[0].Green = (i >> 4 & 15) * 0x1111;
	colours[0].Blue = (i & 15) * 0x1111;
	SetIPrefs(colours, sizeof(colours), IPREFS_TYPE_PALETTE_V39);
	check(!SetIPrefs(&prefs, sizeof(prefs), 99), "SetIPrefs of no type");

	check(!QueryOverscan(INVALID_ID, &r, OSCAN_TEXT), "QueryOverscan without a mode");
}

/* SetIPrefs of the Workbench screen's mode reopens it in that size */
static void wbmode(void)
{
	static struct IScreenModePrefs smp = { INVALID_ID, 640, 480, 1, 0 };
	struct Screen *wb;

	check(SetIPrefs(&smp, sizeof(smp), IPREFS_TYPE_SCREENMODE_V37),
	    "SetIPrefs screen mode");
	wb = LockPubScreen("Workbench");
	check(wb && wb->Width == 640 && wb->Height == 480, "Workbench reopened");
	if (wb)
		UnlockPubScreen(NULL, wb);
	smp.smp_Width = 1024;
	smp.smp_Height = 800;
	SetIPrefs(&smp, sizeof(smp), IPREFS_TYPE_SCREENMODE_V37);
}

/*
 * Whether an area of the screen shows the Workbench backfill, pen fp
 * over BACKGROUNDPEN in a 50% pattern (solid when fp is BACKGROUNDPEN):
 * every third pixel of every 13th row, and row 204, where the bitmap
 * crosses 64 KB
 */
static BOOL backfilled(struct Screen *s, UWORD fp, WORD x0, WORD y0,
    WORD x1, WORD y1)
{
	UWORD bp = pen(s, BACKGROUNDPEN);
	WORD x, y;

	for (y = y0; y <= y1; y = y == 203 ? 204 : y + 13) {
		if (y < 204 && y + 13 > 204)
			y = 203;
		for (x = x0; x <= x1; x += 3)
			if (ReadPixel(&s->RastPort, x, y) !=
			    ((((x >> 1) ^ y) & 1) ? fp : bp))
				return FALSE;
	}
	return TRUE;
}

/*
 * A Workbench-like screen of the colour display, 640x400 in a 4-bit
 * chunky RAM bitmap of 125 KB: plane 0 crosses into its second 64 KB at
 * row 204, byte 256 (x = 512).
 */
static struct TagItem chunkytags[] = {
	{ BMATags_GfxFormat, GDF_CHUNKY4 }, { TAG_DONE, 0 }
};

static void colour(void)
{
	struct BitMap *bm;
	struct DrawInfo *dri;
	struct Window *w;
	struct Gadget *g;
	struct TagItem t[7];
	WORD x, y, n[3];
	ULONG p;

	bm = AllocBitMap(640, 400, 4, BMF_CLEAR | BMF_RTGTAGS | BMF_RTGCHECK |
	    BMF_FRIENDISTAG, (struct BitMap *)chunkytags);
	check(bm != NULL, "640x400 chunky bitmap");
	if (bm == NULL)
		return;
	t[0].ti_Tag = SA_Width;		t[0].ti_Data = 640;
	t[1].ti_Tag = SA_Height;	t[1].ti_Data = 400;
	t[2].ti_Tag = SA_Depth;		t[2].ti_Data = 4;
	t[3].ti_Tag = SA_BitMap;	t[3].ti_Data = (IPTR)bm;
	t[4].ti_Tag = SA_Type;		t[4].ti_Data = WBENCHSCREEN;
	t[5].ti_Tag = SA_Title;		t[5].ti_Data = (IPTR)"Colour";
	t[6].ti_Tag = TAG_DONE;
	scr = OpenScreenTagList(NULL, t);
	check(scr != NULL, "colour screen");
	if (scr == NULL) {
		FreeBitMap(bm);
		return;
	}
	dri = GetScreenDrawInfo(scr);
	check(dri->dri_Depth == 4 && (dri->dri_Flags & DRIF_NEWLOOK) &&
	    GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH) == 4,
	    "colour DrawInfo");
	check(dri->dri_Pens[BACKGROUNDPEN] != dri->dri_Pens[TEXTPEN] &&
	    dri->dri_Pens[FILLPEN] != dri->dri_Pens[BACKGROUNDPEN] &&
	    dri->dri_Pens[SHINEPEN] != dri->dri_Pens[SHADOWPEN] &&
	    dri->dri_Pens[FILLPEN] != dri->dri_Pens[SHINEPEN],
	    "colour pens");
	FreeScreenDrawInfo(scr, dri);
	check(ReadPixel(&scr->RastPort, 400, 1) == pen(scr, BARBLOCKPEN) &&
	    ReadPixel(&scr->RastPort, 300, scr->BarHeight) ==
	    pen(scr, BARTRIMPEN), "colour title bar");
	check(backfilled(scr, pen(scr, FILLPEN), 0, scr->BarHeight + 1, 639, 399),
	    "colour backfill");

	/* a window over the split: 3D frame, close gadget in four pens */
	w = openwin(400, 150, 200, 150, "Split", WA_SmartRefresh);
	check(w != NULL, "window over the split");
	if (w) {
		check(ReadPixel(w->RPort, 0, 100) == pen(scr, SHINEPEN) &&
		    ReadPixel(w->RPort, w->Width - 1, 54) ==
		    pen(scr, SHADOWPEN) && ReadPixel(w->RPort, 100,
		    w->Height - 1) == pen(scr, SHADOWPEN) &&
		    ReadPixel(w->RPort, 150, 2) == pen(scr, FILLPEN) &&
		    ReadPixel(w->RPort, 112, 54) == pen(scr, BACKGROUNDPEN),
		    "window frame over the split");
		g = sysgad(w, GTYP_CLOSE, &x, &y);
		n[0] = n[1] = n[2] = 0;
		for (y = 0; g && y < g->Height; y++)
			for (x = 0; x < g->Width; x++) {
				p = ReadPixel(w->RPort, g->LeftEdge + x,
				    g->TopEdge + y);
				n[0] += p == pen(scr, SHINEPEN);
				n[1] += p == pen(scr, SHADOWPEN);
				n[2] += p == pen(scr, FILLPEN);
			}
		check(n[0] > 0 && n[1] > 0 && n[2] > 0, "3D close gadget");
		MoveWindow(w, -300, 60);
		sync();
		check(w->LeftEdge == 100 && w->TopEdge == 210 &&
		    backfilled(scr, pen(scr, FILLPEN), 500, 150, 599, 209) &&
		    ReadPixel(w->RPort, 112, 54) == pen(scr, BACKGROUNDPEN),
		    "window moved over the split");
		CloseWindow(w);
		check(backfilled(scr, pen(scr, FILLPEN), 100, 200, 299, 359),
		    "backfill after CloseWindow");
	}

	alerts();
	dbuf(scr);
	windows();
	gadgets();
	menus();
	requesters();
	check(backfilled(scr, pen(scr, FILLPEN), 0, scr->BarHeight + 1, 639, 399),
	    "colour backfill at the end");
	check(CloseScreen(scr), "colour CloseScreen");
	scr = NULL;
	FreeBitMap(bm);
}

/* The pixels of pen p in a row of a window, from x0 to x1 */
static WORD count(struct Window *w, UWORD p, WORD y, WORD x0, WORD x1)
{
	WORD x, n = 0;

	for (x = x0; x <= x1; x++)
		n += ReadPixel(w->RPort, x, y) == p;
	return n;
}

/*
 * The Workbench 1.x style: the 1.0 colours and OLD_LOOK in Preferences
 * give a 640x400 screen without SA_Pens the flat 1.x look: no
 * DRIF_NEWLOOK, colour 1 frames and title bars on a solid colour 0.
 */
static void oldlook(void)
{
	static struct Preferences saved, prefs;
	struct BitMap *bm;
	struct DrawInfo *dri;
	struct Window *w, *w2;
	struct Gadget *g;
	struct ColorMap *cm;
	struct TagItem t[7];
	WORD x, y, n, tx;

	GetPrefs(&saved, sizeof(saved));
	prefs = saved;
	prefs.color0 = 0x05A;
	prefs.color1 = 0xFFF;
	prefs.color2 = 0x002;
	prefs.color3 = 0xF80;
	prefs.EnableCLI |= OLD_LOOK;
	SetPrefs(&prefs, sizeof(prefs), FALSE);

	bm = AllocBitMap(640, 400, 4, BMF_CLEAR | BMF_RTGTAGS | BMF_RTGCHECK |
	    BMF_FRIENDISTAG, (struct BitMap *)chunkytags);
	check(bm != NULL, "1.x bitmap");
	if (bm == NULL)
		goto restore;
	t[0].ti_Tag = SA_Width;		t[0].ti_Data = 640;
	t[1].ti_Tag = SA_Height;	t[1].ti_Data = 400;
	t[2].ti_Tag = SA_Depth;		t[2].ti_Data = 4;
	t[3].ti_Tag = SA_BitMap;	t[3].ti_Data = (IPTR)bm;
	t[4].ti_Tag = SA_Type;		t[4].ti_Data = WBENCHSCREEN;
	t[5].ti_Tag = SA_Title;		t[5].ti_Data = (IPTR)"Workbench 1.x";
	t[6].ti_Tag = TAG_DONE;
	scr = OpenScreenTagList(NULL, t);
	check(scr != NULL, "1.x screen");
	if (scr == NULL) {
		FreeBitMap(bm);
		goto restore;
	}
	cm = scr->ViewPort.ColorMap;
	check(GetRGB4(cm, 0) == 0x05A && GetRGB4(cm, 1) == 0xFFF &&
	    GetRGB4(cm, 2) == 0x002 && GetRGB4(cm, 3) == 0xF80, "1.x colours");
	dri = GetScreenDrawInfo(scr);
	check(dri->dri_Depth == 4 && !(dri->dri_Flags & DRIF_NEWLOOK),
	    "1.x DrawInfo");
	check(dri->dri_Pens[BACKGROUNDPEN] == 0 && dri->dri_Pens[TEXTPEN] == 1 &&
	    dri->dri_Pens[SHINEPEN] == 1 && dri->dri_Pens[SHADOWPEN] == 1 &&
	    dri->dri_Pens[FILLPEN] == 1 && dri->dri_Pens[FILLTEXTPEN] == 0 &&
	    dri->dri_Pens[HIGHLIGHTTEXTPEN] == 3 &&
	    dri->dri_Pens[BARDETAILPEN] == 0 && dri->dri_Pens[BARBLOCKPEN] == 1 &&
	    dri->dri_Pens[BARTRIMPEN] == 1, "1.x pens");
	FreeScreenDrawInfo(scr, dri);
	check(ReadPixel(&scr->RastPort, 400, 1) == 1 &&
	    ReadPixel(&scr->RastPort, 300, scr->BarHeight) == 1,
	    "1.x title bar");
	check(backfilled(scr, 0, 0, scr->BarHeight + 1, 639, 399),
	    "1.x solid backfill");

	/* the active window: colour 1 frame and title bar, colour 0 title */
	w = openwin(400, 150, 200, 150, "Split", WA_SmartRefresh);
	check(w != NULL, "1.x window");
	if (w == NULL)
		goto close;
	n = 0;
	for (y = 0; y < w->Height; y++)
		for (x = 0; x < w->Width; x++)
			if (x < w->BorderLeft || y < w->BorderTop ||
			    x >= w->Width - w->BorderRight ||
			    y >= w->Height - w->BorderBottom)
				n += ReadPixel(w->RPort, x, y) > 1;
	check(n == 0, "1.x frame in colours 0 and 1");
	check(ReadPixel(w->RPort, 0, 100) == 1 &&
	    ReadPixel(w->RPort, 1, 100) == 1 &&
	    ReadPixel(w->RPort, w->Width - 2, 100) == 1 &&
	    ReadPixel(w->RPort, 100, w->Height - 2) == 1 &&
	    ReadPixel(w->RPort, 150, 2) == 1 &&
	    ReadPixel(w->RPort, 112, 54) == 0, "1.x active frame");
	g = sysgad(w, GTYP_CLOSE, &x, &y);
	tx = g ? g->LeftEdge + g->Width : 0;
	check(count(w, 0, w->BorderTop / 2, tx + 1, tx + 40) > 5,
	    "1.x active title");

	/* behind another window: colour 1 lines and title on colour 0 */
	w2 = openwin(20, 40, 150, 80, "Other", WA_SmartRefresh);
	check(w2 != NULL, "1.x second window");
	sync();
	check(!(w->Flags & WFLG_WINDOWACTIVE) &&
	    ReadPixel(w->RPort, 0, 100) == 1 &&
	    ReadPixel(w->RPort, 150, 2) == 0 &&
	    count(w, 1, w->BorderTop / 2, tx + 1, tx + 40) > 5,
	    "1.x inactive window");
	if (w2)
		CloseWindow(w2);
	CloseWindow(w);
	check(backfilled(scr, 0, 0, scr->BarHeight + 1, 639, 399),
	    "1.x backfill after CloseWindow");
close:
	check(CloseScreen(scr), "1.x CloseScreen");
	scr = NULL;
	FreeBitMap(bm);
restore:
	SetPrefs(&saved, sizeof(saved), FALSE);
}

/*
 * The default pointer on the Workbench screen, read from its plane
 * over pen 0 and over pen 1: a pixel that changes is transparent.  The
 * tip is the rim, and every pixel of the body has rim or body on all
 * four sides, inside the 16x16 sprite.
 */
static UBYTE seen[2][16][16];

static BOOL opaque(WORD x, WORD y)
{
	return x >= 0 && y >= 0 && x < 16 && y < 16 &&
	    seen[0][y][x] == seen[1][y][x];
}

static void pointer(struct Screen *wb)
{
	struct RastPort *rp = &wb->RastPort;
	struct BitMap *bm = rp->BitMap;
	WORD x, y, x0 = 400, y0 = 200, body = 0, open = 0;
	UBYTE *row;
	int i;

	mouse(x0, y0);
	for (i = 0; i < 2; i++) {
		SetAPen(rp, i);
		RectFill(rp, x0 - 8, y0 - 8, x0 + 24, y0 + 24);
		for (y = 0; y < 16; y++) {
			row = bm->Planes[0] + (y0 + y) * bm->BytesPerRow;
			for (x = 0; x < 16; x++)
				seen[i][y][x] = (row[(x0 + x) >> 3] >>
				    (7 - ((x0 + x) & 7))) & 1;
		}
	}
	check(opaque(0, 0), "pointer tip");
	for (y = 0; y < 16; y++)
		for (x = 0; x < 16; x++)
			if (opaque(x, y) && seen[0][y][x] != seen[0][0][0]) {
				body++;
				open += !opaque(x - 1, y) + !opaque(x + 1, y) +
				    !opaque(x, y - 1) + !opaque(x, y + 1);
			}
	check(body > 30 && open == 0, "pointer outline");
}

/* The Workbench screen: the default public screen, 1024x800 mono */
static void workbench(void)
{
	struct Screen *wb;
	struct RastPort *rp;
	LONG bg;
	char name[MAXPUBSCREENNAME + 1];

	wb = LockPubScreen("Workbench");
	check(wb != NULL, "Workbench screen");
	if (wb == NULL)
		return;
	check(wb->Width == 1024 && wb->Height == 800 &&
	    GetBitMapAttr(wb->RastPort.BitMap, BMA_DEPTH) == 1,
	    "Workbench size");
	GetDefaultPubScreen(name);
	check(strcmp(name, "Workbench") == 0, "GetDefaultPubScreen");
	check(ReadPixel(&wb->RastPort, 600, wb->BarHeight) ==
	    pen(wb, BARTRIMPEN), "Workbench title bar");
	/* rows past the bitmap's first 64 KB */
	rp = &wb->RastPort;
	bg = ReadPixel(rp, 25, 780);
	SetAPen(rp, 1);
	RectFill(rp, 10, 700, 20, 790);
	check(ReadPixel(rp, 15, 780) == 1 && ReadPixel(rp, 25, 780) == bg,
	    "Workbench second segment");
	/* the background: a 50% pattern anchored to the screen */
	check(ReadPixel(rp, 600, 400) != ReadPixel(rp, 602, 400) &&
	    ReadPixel(rp, 600, 400) != ReadPixel(rp, 600, 401),
	    "Workbench backfill");
	pointer(wb);
	UnlockPubScreen(NULL, wb);
	wbmode();
	check(CloseWorkBench(), "CloseWorkBench");
}

/* DrawBorder, PrintIText, DrawImage, EraseImage, IntuiTextLength */
static void rendering(void)
{
	struct RastPort rp;
	struct BitMap *bm;
	static WORD xy[] = { 0, 0, 9, 0, 9, 9, 0, 9, 0, 0 };
	static struct Border border = { 2, 2, 3, 0, JAM1, 5, xy, NULL };
	static UWORD data[] = { 0xF000, 0x9000, 0x9000, 0xF000 };
	static struct Image image = { 20, 2, 4, 4, 1, data, 1, 0, NULL };
	static struct IntuiText text = { 2, 0, JAM1, 40, 2, NULL, "Hi", NULL };
	int x, n;

	bm = AllocBitMap(100, 20, 2, BMF_CLEAR, NULL);
	if (bm == NULL)
		return;
	InitRastPort(&rp);
	rp.BitMap = bm;
	DrawBorder(&rp, &border, 1, 1);
	check(ReadPixel(&rp, 3, 3) == 3 && ReadPixel(&rp, 12, 12) == 3 &&
	    ReadPixel(&rp, 7, 7) == 0, "DrawBorder");
	DrawImage(&rp, &image, 0, 0);
	check(ReadPixel(&rp, 20, 2) == 1 && ReadPixel(&rp, 21, 3) == 0 &&
	    ReadPixel(&rp, 23, 5) == 1, "DrawImage");
	EraseImage(&rp, &image, 0, 0);
	check(ReadPixel(&rp, 20, 2) == 0 && ReadPixel(&rp, 23, 5) == 0,
	    "EraseImage");
	check(IntuiTextLength(&text) == 16, "IntuiTextLength");
	PrintIText(&rp, &text, 0, 0);
	n = 0;
	for (x = 40; x < 56; x++)
		if (ReadPixel(&rp, x, 6) == 2)
			n++;
	check(n > 2, "PrintIText");
	FreeBitMap(bm);
}

int main(void)
{
	port = CreateMsgPort();
	io = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
	if (OpenDevice("input.device", 0, (struct IORequest *)io, 0) != 0) {
		KPutStr("FAIL intuition: input.device\n");
		return 1;
	}
	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	if (GfxBase == NULL || IntuitionBase == NULL) {
		KPutStr("FAIL intuition: OpenLibrary\n");
		return 1;
	}
	check(IntuitionBase->LibNode.lib_Version == 50, "version");

	alert(RECOVERY_ALERT, IECODE_RBUTTON, FALSE, "alert on its own screen");
	check(IntuitionBase->FirstScreen == NULL, "alert screen closed");
	rendering();
	iprefs();
	screens();
	if (scr) {
		alerts();
		inputalerts();
		dbuf(scr);
		windows();
		gadgets();
		menus();
		requesters();
	}
	if (scr)
		check(CloseScreen(scr), "CloseScreen");
	colour();
	oldlook();
	workbench();

	stackuse("input.device");
	stackuse("Intuition menu handler");
	CloseLibrary((struct Library *)IntuitionBase);
	CloseLibrary((struct Library *)GfxBase);
	CloseDevice((struct IORequest *)io);
	if (fails != 0)
		return 1;
	KPutStr("PASS intuition\n");
	return 0;
}
