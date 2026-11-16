/*
 * gadtools.library with the mouse and the keyboard: a window on the
 * Workbench screen with a gadget of every kind and a menu strip, and a
 * serial line for every message GT_GetIMsg returns, until the window's
 * close gadget is clicked.  It passes once that click comes after a use
 * of every gadget that reports and a menu pick; it fails after TIMEOUT
 * minutes without the click.
 *
 * With a display the input is the user's.  Without one it writes the events itself to input.device and checks the same lines.
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
#include <libraries/gadtools.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>

#define TIMEOUT	10		/* minutes */

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;

enum { G_BUTTON = 1, G_CHECKBOX, G_INTEGER, G_STRING, G_NUMBER, G_TEXT,
	G_CYCLE, G_SLIDER, G_SCROLLER, G_MX, G_PALETTE, G_LISTVIEW, NGADS };

static char *names[NGADS] = { "", "button", "checkbox", "integer", "string",
	"number", "text", "cycle", "slider", "scroller", "mx", "palette",
	"listview" };

/* The gadgets that report, and a menu pick */
#define S_MENU	(1 << NGADS)
#define S_ALL	(S_MENU | (1 << G_BUTTON) | (1 << G_CHECKBOX) | \
	(1 << G_INTEGER) | (1 << G_STRING) | (1 << G_CYCLE) | \
	(1 << G_SLIDER) | (1 << G_SCROLLER) | (1 << G_MX) | \
	(1 << G_PALETTE) | (1 << G_LISTVIEW))

static struct Gadget *gads[NGADS], *glist;
static struct Window *win;
static struct Menu *menus;
static ULONG seen;
static BOOL closed;

static STRPTR cyclabels[] = { "One", "Two", "Three", NULL };
static STRPTR mxlabels[] = { "Alpha", "Beta", "Gamma", NULL };
static struct List lvlist;
static struct Node lvnodes[10];
static STRPTR lvnames[10] = { "Zero", "One", "Two", "Three", "Four",
	"Five", "Six", "Seven", "Eight", "Nine" };

static struct NewMenu newmenu[] = {
	{ NM_TITLE, "Project", NULL, 0, 0, NULL },
	{ NM_ITEM, "Open", "O", 0, 0, NULL },
	{ NM_ITEM, NM_BARLABEL, NULL, 0, 0, NULL },
	{ NM_ITEM, "Quit", "Q", 0, 0, NULL },
	{ NM_TITLE, "Edit", NULL, 0, 0, NULL },
	{ NM_ITEM, "Cut", "X", 0, 0, NULL },
	{ NM_ITEM, "Toggle", NULL, CHECKIT | MENUTOGGLE, 0, NULL },
	{ NM_ITEM, "More", NULL, 0, 0, NULL },
	{ NM_SUB, "One", NULL, 0, 0, NULL },
	{ NM_SUB, "Two", NULL, 0, 0, NULL },
	{ NM_END, NULL, NULL, 0, 0, NULL }
};

static struct Gadget *make(struct Screen *s, APTR vi, struct Gadget *prev,
    ULONG kind, UWORD id, char *label, ULONG flags, WORD x, WORD y, WORD w,
    WORD h, struct TagItem *tags)
{
	struct NewGadget ng;

	ng.ng_LeftEdge = s->WBorLeft + x;
	ng.ng_TopEdge = s->WBorTop + s->Font->ta_YSize + 1 + y;
	ng.ng_Width = w;
	ng.ng_Height = h;
	ng.ng_GadgetText = label;
	ng.ng_TextAttr = NULL;
	ng.ng_GadgetID = id;
	ng.ng_Flags = flags;
	ng.ng_VisualInfo = vi;
	ng.ng_UserData = NULL;
	gads[id] = CreateGadgetA(kind, prev, &ng, tags);
	return gads[id];
}

static BOOL build(struct Screen *s, APTR vi)
{
	static struct TagItem buttags[] = {
		{ GT_Underscore, '_' }, { TAG_DONE, 0 } };
	static struct TagItem numtags[] = {
		{ GTNM_Number, 0 }, { GTNM_Border, TRUE }, { TAG_DONE, 0 } };
	static struct TagItem texttags[] = {
		{ GTTX_Text, (IPTR)"wb900 gadtools" }, { GTTX_Border, TRUE },
		{ TAG_DONE, 0 } };
	static struct TagItem inttags[] = {
		{ GTIN_Number, 900 }, { GTIN_MaxChars, 8 }, { TAG_DONE, 0 } };
	static struct TagItem strtags[] = {
		{ GTST_String, (IPTR)"Commodore" }, { GTST_MaxChars, 40 },
		{ TAG_DONE, 0 } };
	static struct TagItem cytags[] = {
		{ GTCY_Labels, (IPTR)cyclabels }, { TAG_DONE, 0 } };
	static struct TagItem sltags[] = {
		{ GTSL_Min, 0 }, { GTSL_Max, 20 }, { GTSL_Level, 5 },
		{ GTSL_MaxLevelLen, 2 }, { GTSL_LevelFormat, (IPTR)"%2ld" },
		{ GTSL_LevelPlace, PLACETEXT_RIGHT }, { TAG_DONE, 0 } };
	static struct TagItem sctags[] = {
		{ GTSC_Total, 40 }, { GTSC_Visible, 10 }, { GTSC_Top, 0 },
		{ GTSC_Arrows, 16 }, { TAG_DONE, 0 } };
	static struct TagItem mxtags[] = {
		{ GTMX_Labels, (IPTR)mxlabels }, { GTMX_Spacing, 2 },
		{ TAG_DONE, 0 } };
	static struct TagItem patags[] = {
		{ GTPA_Depth, 1 }, { GTPA_Color, 1 },
		{ GTPA_IndicatorWidth, 16 }, { TAG_DONE, 0 } };
	static struct TagItem lvtags[] = {
		{ GTLV_Labels, (IPTR)&lvlist }, { GTLV_ShowSelected, 0 },
		{ TAG_DONE, 0 } };
	static struct TagItem menutags[] = {
		{ GTMN_NewLookMenus, TRUE }, { TAG_DONE, 0 } };
	struct Gadget *g;
	WORD i;

	NewList(&lvlist);
	for (i = 0; i < 10; i++) {
		lvnodes[i].ln_Name = lvnames[i];
		AddTail(&lvlist, &lvnodes[i]);
	}
	patags[0].ti_Data = GetBitMapAttr(s->RastPort.BitMap, BMA_DEPTH);
	if (patags[0].ti_Data > 4)
		patags[0].ti_Data = 4;

	g = CreateContext(&glist);
	g = make(s, vi, g, BUTTON_KIND, G_BUTTON, "_Button", 0, 90, 4, 80, 14, buttags);
	g = make(s, vi, g, CHECKBOX_KIND, G_CHECKBOX, "Check", 0, 90, 22, 0, 0, NULL);
	g = make(s, vi, g, INTEGER_KIND, G_INTEGER, "Integer", 0, 90, 38, 80, 14, inttags);
	g = make(s, vi, g, STRING_KIND, G_STRING, "String", 0, 90, 56, 140, 14, strtags);
	g = make(s, vi, g, NUMBER_KIND, G_NUMBER, "Number", 0, 90, 74, 80, 12, numtags);
	g = make(s, vi, g, TEXT_KIND, G_TEXT, "Text", 0, 90, 90, 140, 12, texttags);
	g = make(s, vi, g, CYCLE_KIND, G_CYCLE, "Cycle", 0, 90, 106, 120, 14, cytags);
	g = make(s, vi, g, SLIDER_KIND, G_SLIDER, "Slider", 0, 90, 124, 120, 11, sltags);
	g = make(s, vi, g, SCROLLER_KIND, G_SCROLLER, "Scroller", 0, 90, 140, 140, 12, sctags);
	g = make(s, vi, g, MX_KIND, G_MX, NULL, PLACETEXT_RIGHT, 270, 4, 0, 0, mxtags);
	g = make(s, vi, g, PALETTE_KIND, G_PALETTE, "Palette", PLACETEXT_ABOVE, 270, 50, 140, 20, patags);
	g = make(s, vi, g, LISTVIEW_KIND, G_LISTVIEW, "List", PLACETEXT_ABOVE, 270, 90, 140, 70, lvtags);
	if (g == NULL)
		return FALSE;
	menus = CreateMenusA(newmenu, NULL);
	return menus != NULL && LayoutMenusA(menus, vi, menutags);
}

#define WIDCMP	(BUTTONIDCMP | CHECKBOXIDCMP | INTEGERIDCMP | STRINGIDCMP | \
	CYCLEIDCMP | SLIDERIDCMP | SCROLLERIDCMP | ARROWIDCMP | MXIDCMP | \
	PALETTEIDCMP | LISTVIEWIDCMP | IDCMP_MENUPICK | IDCMP_CLOSEWINDOW | \
	IDCMP_REFRESHWINDOW | IDCMP_VANILLAKEY)

static struct Window *openwin(struct Screen *s)
{
	struct TagItem t[13];

	t[0].ti_Tag = WA_Left;		t[0].ti_Data = 40;
	t[1].ti_Tag = WA_Top;		t[1].ti_Data = 40;
	t[2].ti_Tag = WA_Width;		t[2].ti_Data = 440;
	t[3].ti_Tag = WA_Height;	t[3].ti_Data = 190;
	t[4].ti_Tag = WA_Title;		t[4].ti_Data = (IPTR)"wb900 gadtools";
	t[5].ti_Tag = WA_PubScreen;	t[5].ti_Data = (IPTR)s;
	t[6].ti_Tag = WA_IDCMP;		t[6].ti_Data = WIDCMP;
	t[7].ti_Tag = WA_Gadgets;	t[7].ti_Data = (IPTR)glist;
	t[8].ti_Tag = WA_DragBar;	t[8].ti_Data = TRUE;
	t[9].ti_Tag = WA_DepthGadget;	t[9].ti_Data = TRUE;
	t[10].ti_Tag = WA_CloseGadget;	t[10].ti_Data = TRUE;
	t[11].ti_Tag = WA_Activate;	t[11].ti_Data = TRUE;
	t[12].ti_Tag = TAG_DONE;
	return OpenWindowTagList(NULL, t);
}

/* The screen position of a point of a gadget */
#define GX(g, dx)	(win->LeftEdge + (g)->LeftEdge + (dx))
#define GY(g, dy)	(win->TopEdge + (g)->TopEdge + (dy))

static LONG getattr(UWORD id, Tag tag)
{
	struct TagItem t[2];
	LONG v = 0;

	t[0].ti_Tag = tag;
	t[0].ti_Data = (IPTR)&v;
	t[1].ti_Tag = TAG_DONE;
	GT_GetGadgetAttrsA(gads[id], win, NULL, t);
	return v;
}

/* The number gadget counts the messages */
static LONG nmsgs;

static void message(struct IntuiMessage *im)
{
	struct Gadget *g;
	struct MenuItem *it;
	struct TagItem t[2];
	UWORD id, code = im->Code;

	switch (im->Class) {
	case IDCMP_GADGETUP:
	case IDCMP_GADGETDOWN:
	case IDCMP_MOUSEMOVE:
		g = (struct Gadget *)im->IAddress;
		id = g->GadgetID;
		if (id == 0 || id >= NGADS)
			break;
		xKPrintf("gtdemo: %s %s code %d", im->Class == IDCMP_GADGETUP ?
		    "GADGETUP" : im->Class == IDCMP_GADGETDOWN ? "GADGETDOWN" :
		    "MOUSEMOVE", names[id], code);
		if (id == G_STRING)
			xKPrintf(" \"%s\"", (char *)getattr(G_STRING, GTST_String));
		else if (id == G_INTEGER)
			xKPrintf(" %ld", getattr(G_INTEGER, GTIN_Number));
		else if (id == G_LISTVIEW && code < 10)
			xKPrintf(" \"%s\"", lvnames[code]);
		else if (id == G_CYCLE)
			xKPrintf(" \"%s\"", cyclabels[code]);
		else if (id == G_MX)
			xKPrintf(" \"%s\"", mxlabels[code]);
		KPutStr("\n");
		seen |= 1L << id;
		break;
	case IDCMP_MENUPICK:
		while (code != MENUNULL) {
			it = ItemAddress(menus, code);
			xKPrintf("gtdemo: MENUPICK menu %d item %d sub %d \"%s\"%s\n",
			    MENUNUM(code), ITEMNUM(code), SUBNUM(code),
			    ((struct IntuiText *)it->ItemFill)->IText,
			    (it->Flags & CHECKIT) ? ((it->Flags & CHECKED) ?
			    " checked" : " unchecked") : "");
			seen |= S_MENU;
			code = it->NextSelect;
		}
		break;
	case IDCMP_VANILLAKEY:
		xKPrintf("gtdemo: VANILLAKEY '%c'\n", code);
		break;
	case IDCMP_REFRESHWINDOW:
		GT_BeginRefresh(win);
		GT_EndRefresh(win, TRUE);
		return;
	case IDCMP_CLOSEWINDOW:
		KPutStr("gtdemo: CLOSEWINDOW\n");
		closed = TRUE;
		return;
	default:
		return;
	}
	t[0].ti_Tag = GTNM_Number;
	t[0].ti_Data = ++nmsgs;
	t[1].ti_Tag = TAG_DONE;
	GT_SetGadgetAttrsA(gads[G_NUMBER], win, NULL, t);
}

static void drain(void)
{
	struct IntuiMessage *im;

	while ((im = GT_GetIMsg(win->UserPort)) != NULL) {
		message(im);
		GT_ReplyIMsg(im);
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

/* Let the input handler and the menu task finish, then log */
static void settle(void)
{
	event(IECLASS_NULL, 0, 0, 0);
	delayio->tr_node.io_Command = TR_ADDREQUEST;
	delayio->tr_time.tv_secs = 0;
	delayio->tr_time.tv_micro = 100000;
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
}

static void selfdrive(void)
{
	struct Gadget *g;
	struct MenuItem *it;
	WORD x, y;

	KPutStr("gtdemo: no display: writing the events to input.device\n");
	if (OpenDevice("input.device", 0, (struct IORequest *)inio, 0) != 0) {
		KPutStr("FAIL gtdemo: input.device\n");
		return;
	}
	settle();
	click(GX(gads[G_BUTTON], 10), GY(gads[G_BUTTON], 5));
	click(GX(gads[G_CHECKBOX], 5), GY(gads[G_CHECKBOX], 5));
	click(GX(gads[G_CYCLE], 30), GY(gads[G_CYCLE], 5));
	g = gads[G_MX];
	click(GX(g, 5), GY(g, g->Height - 4));
	g = gads[G_SLIDER];
	click(GX(g, g->Width - 3), GY(g, 5));
	g = gads[G_SCROLLER];
	click(GX(g, g->Width - 3), GY(g, 5));
	click(GX(gads[G_PALETTE], 30), GY(gads[G_PALETTE], 10));
	click(GX(gads[G_LISTVIEW], 20), GY(gads[G_LISTVIEW], 12));
	g = gads[G_STRING];
	click(GX(g, g->Width - 4), GY(g, 5));
	key(RAWKEY_SPACE);
	key(RAWKEY_9);
	key(RAWKEY_0);
	key(RAWKEY_0);
	key(RAWKEY_RETURN);
	settle();
	g = gads[G_INTEGER];
	click(GX(g, g->Width - 4), GY(g, 5));
	key(RAWKEY_1);
	key(RAWKEY_RETURN);
	settle();

	/* Project/Quit */
	it = menus->FirstItem->NextItem->NextItem;
	x = menus->LeftEdge + 4;
	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, x, 2);
	event(IECLASS_RAWMOUSE, IECODE_RBUTTON, x, 2);
	settle();
	x = menus->LeftEdge + it->LeftEdge + it->Width / 2;
	y = win->WScreen->BarHeight + 1 + it->TopEdge + it->Height / 2;
	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, x, y);
	event(IECLASS_RAWMOUSE, IECODE_NOBUTTON, x + 1, y);
	settle();
	event(IECLASS_RAWMOUSE, IECODE_RBUTTON | IECODE_UP_PREFIX, x + 1, y);
	settle();
	settle();

	for (g = win->FirstGadget; g; g = g->NextGadget)
		if ((g->GadgetType & GTYP_SYSTYPEMASK) == GTYP_CLOSE)
			click(win->LeftEdge + g->LeftEdge + g->Width / 2,
			    win->TopEdge + g->TopEdge + g->Height / 2);
	CloseDevice((struct IORequest *)inio);
}

/* Where the gadgets and menus are, for a user at the board */
static void geometry(void)
{
	struct Gadget *g;
	struct Menu *m;
	WORD i;

	xKPrintf("gtdemo: window %d,%d %dx%d\n", win->LeftEdge, win->TopEdge,
	    win->Width, win->Height);
	for (i = 1; i < NGADS; i++) {
		g = gads[i];
		xKPrintf("gtdemo: %s %d,%d to %d,%d\n", names[i], GX(g, 0),
		    GY(g, 0), GX(g, g->Width - 1), GY(g, g->Height - 1));
	}
	for (g = win->FirstGadget; g; g = g->NextGadget)
		if ((g->GadgetType & GTYP_SYSTYPEMASK) == GTYP_CLOSE)
			xKPrintf("gtdemo: close gadget %d,%d\n", GX(g, g->Width / 2),
			    GY(g, g->Height / 2));
	for (m = menus; m; m = m->NextMenu)
		xKPrintf("gtdemo: menu \"%s\" title at x %d\n", m->MenuName,
		    m->LeftEdge + 4);
}

int main(void)
{
	struct Screen *wb;
	struct MsgPort *tport;
	struct timerequest *tio;
	APTR vi = NULL;
	ULONG sigs, got;
	int i;

	GfxBase = (struct GfxBase *)OpenLibrary(GRAPHICSNAME, 39L);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary(INTUITIONNAME, 39L);
	GadToolsBase = OpenLibrary(GADTOOLSNAME, 39L);
	tport = CreateMsgPort();
	tio = (struct timerequest *)CreateIORequest(tport, sizeof(*tio));
	delayio = (struct timerequest *)CreateIORequest(tport, sizeof(*tio));
	inio = (struct IOStdReq *)CreateIORequest(tport, sizeof(*inio));
	if (GfxBase == NULL || IntuitionBase == NULL || GadToolsBase == NULL ||
	    delayio == NULL || inio == NULL || OpenDevice(TIMERNAME,
	    UNIT_VBLANK, (struct IORequest *)tio, 0) != 0) {
		KPutStr("FAIL gtdemo: setup\n");
		return 1;
	}
	*delayio = *tio;
	if (OpenWorkBench() == 0 || (wb = LockPubScreen("Workbench")) == NULL) {
		KPutStr("FAIL gtdemo: Workbench\n");
		return 1;
	}
	vi = GetVisualInfoA(wb, NULL);
	if (vi == NULL || !build(wb, vi) || (win = openwin(wb)) == NULL) {
		KPutStr("FAIL gtdemo: gadgets\n");
		return 1;
	}
	UnlockPubScreen(NULL, wb);
	GT_RefreshWindow(win, NULL);
	SetMenuStrip(win, menus);
	xKPrintf("gtdemo: Workbench %dx%d\n", wb->Width, wb->Height);
	geometry();

	if (NextDisplayInfo(INVALID_ID) == INVALID_ID) {
		selfdrive();
	} else {
		tio->tr_node.io_Command = TR_ADDREQUEST;
		tio->tr_time.tv_secs = TIMEOUT * 60L;
		tio->tr_time.tv_micro = 0;
		SendIO((struct IORequest *)tio);
		xKPrintf("gtdemo: waiting for input, %d minutes\n", TIMEOUT);
		sigs = (1L << win->UserPort->mp_SigBit) | (1L << tport->mp_SigBit);
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

	ClearMenuStrip(win);
	CloseWindow(win);
	FreeMenus(menus);
	FreeGadgets(glist);
	FreeVisualInfo(vi);
	if (!closed) {
		xKPrintf("FAIL gtdemo: no close in %d minutes\n", TIMEOUT);
		return 1;
	}
	if ((seen & S_ALL) != S_ALL) {
		for (i = 1; i < NGADS; i++)
			if ((S_ALL & (1L << i)) && !(seen & (1L << i)))
				xKPrintf("FAIL gtdemo: no %s message\n", names[i]);
		if (!(seen & S_MENU))
			KPutStr("FAIL gtdemo: no MENUPICK\n");
		return 1;
	}
	KPutStr("PASS gtdemo\n");
	return 0;
}
