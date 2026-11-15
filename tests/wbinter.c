/* SPDX-License-Identifier: MIT */
/*
 * intuition.library with the mouse and the keyboard: the Workbench
 * screen and wbdemo's two windows, and a serial line for every IDCMP
 * message they get, until the front window's close gadget is clicked.
 * It passes once that click comes after a gadget release in both of the
 * front window's gadgets, an activation, a mouse button, a key that
 * maps to a character and one that does not, and a window moved or
 * sized; it fails after TIMEOUT minutes without the click.
 *
 * With a display the input is the user's.  Without one (the C emulator)
 * it writes the events itself to input.device, as a user would make
 * them, and checks the same lines.
 */
#include <exec/exec.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <devices/timer.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/modeid.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#define TIMEOUT	10		/* minutes */

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static UBYTE strbuf[40] = "wb900 string gadget";
static UBYTE undobuf[40];
static struct StringInfo strinfo = { strbuf, undobuf, 0, 40 };
static WORD strxy[] = { -2, -2, 201, -2, 201, 9, -2, 9, -2, -2 };
static struct Border strborder = { 0, 0, 1, 0, JAM1, 5, strxy, NULL };
static struct Gadget strgad = {
	NULL, 20, 30, 200, 8, GFLG_GADGHCOMP, GACT_RELVERIFY,
	GTYP_STRGADGET, (APTR)&strborder, NULL, NULL, 0, (APTR)&strinfo, 2
};
static WORD butxy[] = { 0, 0, 71, 0, 71, 13, 0, 13, 0, 0 };
static struct Border butborder = { 0, 0, 1, 0, JAM1, 5, butxy, NULL };
static struct IntuiText buttext = { 1, 0, JAM1, 12, 3, NULL, "Button", NULL };
static struct Gadget butgad = {
	&strgad, 20, 50, 72, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
	GTYP_BOOLGADGET, (APTR)&butborder, NULL, &buttext, 0, NULL, 1
};

#define WIDCMP	(IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_GADGETUP | \
	IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_RAWKEY | \
	IDCMP_VANILLAKEY | IDCMP_CHANGEWINDOW | IDCMP_NEWSIZE | \
	IDCMP_REFRESHWINDOW)

/* What has been seen */
#define S_BUTTON	0x01	/* GADGETUP of the button */
#define S_STRING	0x02	/* GADGETUP of the string gadget */
#define S_ACTIVE	0x04	/* ACTIVEWINDOW */
#define S_MOUSE		0x08	/* MOUSEBUTTONS */
#define S_VANILLA	0x10	/* VANILLAKEY */
#define S_RAWKEY	0x20	/* RAWKEY */
#define S_CHANGE	0x40	/* CHANGEWINDOW */
#define S_ALL		0x7F

static char *seennames[] = { "button GADGETUP", "string GADGETUP",
	"ACTIVEWINDOW", "MOUSEBUTTONS", "VANILLAKEY", "RAWKEY",
	"CHANGEWINDOW" };

static struct Window *front, *back;
static UWORD seen;
static BOOL closed;

static struct Window *openwin(struct Screen *s, WORD x, WORD y, WORD w,
    WORD h, char *title, struct Gadget *g)
{
	struct TagItem t[14];

	t[0].ti_Tag = WA_Left;		t[0].ti_Data = x;
	t[1].ti_Tag = WA_Top;		t[1].ti_Data = y;
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = w;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = h;
	t[4].ti_Tag = WA_Title;		t[4].ti_Data = (IPTR)title;
	t[5].ti_Tag = WA_PubScreen;	t[5].ti_Data = (IPTR)s;
	t[6].ti_Tag = WA_CloseGadget;	t[6].ti_Data = TRUE;
	t[7].ti_Tag = WA_DepthGadget;	t[7].ti_Data = TRUE;
	t[8].ti_Tag = WA_SizeGadget;	t[8].ti_Data = TRUE;
	t[9].ti_Tag = WA_DragBar;	t[9].ti_Data = TRUE;
	t[10].ti_Tag = WA_Activate;	t[10].ti_Data = TRUE;
	t[11].ti_Tag = WA_Gadgets;	t[11].ti_Data = (IPTR)g;
	t[12].ti_Tag = WA_IDCMP;	t[12].ti_Data = WIDCMP;
	t[13].ti_Tag = TAG_DONE;
	return OpenWindowTagList(NULL, t);
}

/* The screen position of the centre of a gadget: a system gadget of the
   type, or (type 0) the one with the ID */
static void gadpos(struct Window *w, UWORD type, UWORD id, WORD *x, WORD *y)
{
	struct Gadget *g;
	WORD gx, gy, gw, gh;

	for (g = w->FirstGadget; g; g = g->NextGadget) {
		if (type ? (g->GadgetType & GTYP_SYSTYPEMASK) != type :
		    (g->GadgetType & GTYP_SYSGADGET) || g->GadgetID != id)
			continue;
		gx = g->LeftEdge + ((g->Flags & GFLG_RELRIGHT) ? w->Width - 1 : 0);
		gy = g->TopEdge + ((g->Flags & GFLG_RELBOTTOM) ? w->Height - 1 : 0);
		gw = g->Width + ((g->Flags & GFLG_RELWIDTH) ? w->Width : 0);
		gh = g->Height + ((g->Flags & GFLG_RELHEIGHT) ? w->Height : 0);
		*x = w->LeftEdge + gx + gw / 2;
		*y = w->TopEdge + gy + gh / 2;
		return;
	}
	*x = *y = -1;
}

static void geometry(struct Window *w, char *name)
{
	WORD cx, cy, dx, dy, sx, sy;

	gadpos(w, GTYP_CLOSE, 0, &cx, &cy);
	gadpos(w, GTYP_WDRAGGING, 0, &dx, &dy);
	gadpos(w, GTYP_SIZING, 0, &sx, &sy);
	xKPrintf("wbinter: %s window %d,%d %dx%d: close %d,%d, title %d,%d, "
	    "size %d,%d\n", name, w->LeftEdge, w->TopEdge, w->Width, w->Height,
	    cx, cy, dx, dy, sx, sy);
}

static char *bname(UWORD code)
{
	switch (code) {
	case SELECTDOWN:	return "SELECTDOWN";
	case SELECTUP:		return "SELECTUP";
	case MENUDOWN:		return "MENUDOWN";
	case MENUUP:		return "MENUUP";
	case MIDDLEDOWN:	return "MIDDLEDOWN";
	case MIDDLEUP:		return "MIDDLEUP";
	}
	return "?";
}

static void message(struct IntuiMessage *im)
{
	struct Window *w = im->IDCMPWindow;
	char *name = w == front ? "front" : "back";
	struct Gadget *g;

	switch (im->Class) {
	case IDCMP_MOUSEBUTTONS:
		xKPrintf("wbinter: MOUSEBUTTONS %s at %d,%d (screen %d,%d) %s\n",
		    bname(im->Code), im->MouseX, im->MouseY,
		    im->MouseX + w->LeftEdge, im->MouseY + w->TopEdge, name);
		seen |= S_MOUSE;
		break;
	case IDCMP_GADGETUP:
		g = (struct Gadget *)im->IAddress;
		if (g->GadgetID == 2) {
			xKPrintf("wbinter: GADGETUP string \"%s\" %s\n", strbuf,
			    name);
			seen |= S_STRING;
		} else {
			xKPrintf("wbinter: GADGETUP button %s\n", name);
			seen |= S_BUTTON;
		}
		break;
	case IDCMP_ACTIVEWINDOW:
		xKPrintf("wbinter: ACTIVEWINDOW %s\n", name);
		seen |= S_ACTIVE;
		break;
	case IDCMP_INACTIVEWINDOW:
		xKPrintf("wbinter: INACTIVEWINDOW %s\n", name);
		break;
	case IDCMP_RAWKEY:
		xKPrintf("wbinter: RAWKEY 0x%x qualifier 0x%x %s\n", im->Code,
		    im->Qualifier, name);
		seen |= S_RAWKEY;
		break;
	case IDCMP_VANILLAKEY:
		xKPrintf("wbinter: VANILLAKEY '%c' 0x%x %s\n",
		    im->Code >= 0x20 && im->Code < 0x7F ? im->Code : '.',
		    im->Code, name);
		seen |= S_VANILLA;
		break;
	case IDCMP_CHANGEWINDOW:
		xKPrintf("wbinter: CHANGEWINDOW %s now %d,%d %dx%d\n", name,
		    w->LeftEdge, w->TopEdge, w->Width, w->Height);
		seen |= S_CHANGE;
		break;
	case IDCMP_NEWSIZE:
		xKPrintf("wbinter: NEWSIZE %s %dx%d\n", name, w->Width,
		    w->Height);
		break;
	case IDCMP_REFRESHWINDOW:
		BeginRefresh(w);
		EndRefresh(w, TRUE);
		break;
	case IDCMP_CLOSEWINDOW:
		xKPrintf("wbinter: CLOSEWINDOW %s\n", name);
		if (w == front)
			closed = TRUE;
		break;
	}
}

static void drain(void)
{
	struct IntuiMessage *im;

	while ((im = (struct IntuiMessage *)GetMsg(front->UserPort)) != NULL ||
	    (im = (struct IntuiMessage *)GetMsg(back->UserPort)) != NULL) {
		message(im);
		ReplyMsg((struct Message *)im);
	}
}

/* Without a display: the user's events, written to input.device */
static struct IOStdReq *inio;
static struct timerequest *delayio;

static void event(UBYTE class, UWORD code, WORD x, WORD y)
{
	struct InputEvent ie;

	ie.ie_NextEvent = NULL;
	ie.ie_Class = class;
	ie.ie_SubClass = 0;
	ie.ie_Code = code;
	ie.ie_Qualifier = 0;
	ie.ie_X = x;
	ie.ie_Y = y;
	inio->io_Command = IND_WRITEEVENT;
	inio->io_Data = &ie;
	inio->io_Length = sizeof(ie);
	DoIO((struct IORequest *)inio);
}

/* Let the input handler finish what the events started, then log */
static void settle(void)
{
	event(IECLASS_NULL, 0, 0, 0);
	delayio->tr_node.io_Command = TR_ADDREQUEST;
	delayio->tr_time.tv_secs = 0;
	delayio->tr_time.tv_micro = 50000;
	DoIO((struct IORequest *)delayio);
	drain();
}

static void click(WORD x, WORD y)
{
	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON, x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, x, y);
	settle();
}

static void key(UWORD code)
{
	event(IECLASS_RAWKEY, code, 0, 0);
	event(IECLASS_RAWKEY, code | IECODE_UP_PREFIX, 0, 0);
	settle();
}

static void selfdrive(void)
{
	WORD x, y;

	KPutStr("wbinter: no display: writing the events to input.device\n");
	if (OpenDevice("input.device", 0, (struct IORequest *)inio, 0) != 0) {
		KPutStr("FAIL wbinter: input.device\n");
		return;
	}
	settle();
	gadpos(front, 0, 1, &x, &y);
	click(x, y);
	gadpos(front, 0, 2, &x, &y);
	click(x, y);
	key(RAWKEY_X);
	key(RAWKEY_RETURN);
	click(back->LeftEdge + 40, back->TopEdge + back->Height - 40);
	key(RAWKEY_A);
	key(RAWKEY_UP);
	gadpos(front, GTYP_WDRAGGING, 0, &x, &y);
	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, x, y);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON, x, y);
	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, x + 20, y + 10);
	event(IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, x + 20,
	    y + 10);
	settle();
	gadpos(front, GTYP_CLOSE, 0, &x, &y);
	click(x, y);
	CloseDevice((struct IORequest *)inio);
}

int main(void)
{
	struct Screen *wb;
	struct MsgPort *tport;
	struct timerequest *tio;
	ULONG sigs, got;
	int i;

	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	tport = CreateMsgPort();
	tio = (struct timerequest *)CreateIORequest(tport, sizeof(*tio));
	delayio = (struct timerequest *)CreateIORequest(tport, sizeof(*tio));
	inio = (struct IOStdReq *)CreateIORequest(tport, sizeof(*inio));
	if (GfxBase == NULL || IntuitionBase == NULL || delayio == NULL ||
	    inio == NULL || OpenDevice(TIMERNAME, UNIT_VBLANK,
	    (struct IORequest *)tio, 0) != 0) {
		KPutStr("FAIL wbinter: setup\n");
		return 1;
	}
	*delayio = *tio;
	if (OpenWorkBench() == 0 || (wb = LockPubScreen("Workbench")) == NULL) {
		KPutStr("FAIL wbinter: Workbench\n");
		return 1;
	}
	back = openwin(wb, 40, 40, wb->Width / 2, wb->Height / 2,
	    "wb900 back window", NULL);
	front = openwin(wb, wb->Width / 4, wb->Height / 4, 300, 100,
	    "wb900 front window", &butgad);
	UnlockPubScreen(NULL, wb);
	if (back == NULL || front == NULL) {
		KPutStr("FAIL wbinter: OpenWindowTagList\n");
		return 1;
	}
	xKPrintf("wbinter: Workbench %dx%d\n", wb->Width, wb->Height);
	geometry(back, "back");
	geometry(front, "front");
	{
		WORD bx, by, sx, sy;

		gadpos(front, 0, 1, &bx, &by);
		gadpos(front, 0, 2, &sx, &sy);
		xKPrintf("wbinter: button %d,%d, string gadget %d,%d\n",
		    bx, by, sx, sy);
	}

	if (NextDisplayInfo(INVALID_ID) == INVALID_ID) {
		selfdrive();
	} else {
		tio->tr_node.io_Command = TR_ADDREQUEST;
		tio->tr_time.tv_secs = TIMEOUT * 60L;
		tio->tr_time.tv_micro = 0;
		SendIO((struct IORequest *)tio);
		xKPrintf("wbinter: waiting for input, %d minutes\n", TIMEOUT);
		sigs = (1L << front->UserPort->mp_SigBit) |
		    (1L << back->UserPort->mp_SigBit) |
		    (1L << tport->mp_SigBit);
		while (!closed) {
			got = Wait(sigs);
			drain();
			if ((got & (1L << tport->mp_SigBit)) &&
			    CheckIO((struct IORequest *)tio))
				break;
		}
		if (!CheckIO((struct IORequest *)tio))
			AbortIO((struct IORequest *)tio);
		WaitIO((struct IORequest *)tio);
	}
	CloseDevice((struct IORequest *)tio);

	if (!closed) {
		xKPrintf("FAIL wbinter: no close in %d minutes\n", TIMEOUT);
		return 1;
	}
	CloseWindow(front);
	CloseWindow(back);
	if (seen != S_ALL) {
		for (i = 0; i < 7; i++)
			if (!(seen & (1 << i)))
				xKPrintf("FAIL wbinter: no %s\n", seennames[i]);
		return 1;
	}
	KPutStr("PASS wbinter\n");
	return 0;
}
